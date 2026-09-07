#ifndef TR_TONEMAP_HEADER_HLSL
#define TR_TONEMAP_HEADER_HLSL

// Display transforms that map linear HDR scene radiance to a linear
// display-referred range. Every operator here shares one contract:
//
//   input  : linear scene color, already exposure-scaled, non-negative
//   output : linear display-referred color in [0, 1]
//
// The OETF (gamma encode) is deliberately NOT applied here. The composite pass
// owns that final step so the debug gamma control keeps working for every
// operator and for the untonemapped paths.
//
// Values are shared with TrTonemapOperator in Source/Renderer/TrRenderConstants.h.
static const uint TR_TONEMAP_NONE = 0u;
static const uint TR_TONEMAP_KHRONOS_PBR_NEUTRAL = 1u;
static const uint TR_TONEMAP_ACES_FITTED = 2u;
static const uint TR_TONEMAP_AGX = 3u;

// The color matrices below follow the convention of their published sources and
// are applied as mul(matrix, color), i.e. row-vector dot products. This differs
// from the row-vector mul(vector, matrix) convention used for spatial
// transforms elsewhere in the codebase; keep the operand order as written.

// Khronos PBR Neutral. Reference implementation from KhronosGroup/ToneMapping,
// Apache 2.0. Input and output are Linear Rec. 709.
//
// Below the 0.76 knee the transform is slope-1 and leaves hue and saturation
// completely alone; the only change is a constant -0.04 black-point offset
// (smoothly faded out below 0.08 so black stays black). Above the knee the peak
// rolls off hyperbolically and desaturates toward white.
//
// This is the operator to use when validating that base color and lighting are
// numerically correct: on-screen diffuse color tracks base color at slope 1, so
// relative comparisons are exact. Note the -0.04 pedestal when reading absolute
// albedo off the screen.
float3 TrTonemapKhronosPbrNeutral(float3 color)
{
    const float startCompression = 0.8f - 0.04f;
    const float desaturation = 0.15f;

    color = max(color, 0.0f);

    const float minChannel = min(color.r, min(color.g, color.b));
    const float offset = minChannel < 0.08f
        ? minChannel - 6.25f * minChannel * minChannel
        : 0.04f;
    color -= offset;

    const float peak = max(color.r, max(color.g, color.b));
    if(peak < startCompression)
    {
        return saturate(color);
    }

    const float compressionRange = 1.0f - startCompression;
    const float newPeak = 1.0f -
        compressionRange * compressionRange /
        (peak + compressionRange - startCompression);
    color *= newPeak / peak;

    const float desaturationBlend = 1.0f -
        1.0f / (desaturation * (peak - newPeak) + 1.0f);
    return saturate(lerp(color, newPeak.xxx, desaturationBlend));
}

// ACES RRT + sRGB ODT, Stephen Hill's fit. The industry-familiar filmic look:
// adds an S-curve in the shadows and rolls highlights off over a long range.
// Mid-tones are NOT an identity transform, so albedo read off the screen under
// this operator is hue- and saturation-shifted relative to the material.
float3 TrTonemapAcesFitted(float3 color)
{
    const float3x3 acesInputMatrix =
    {
        0.59719f, 0.35458f, 0.04823f,
        0.07600f, 0.90834f, 0.01566f,
        0.02840f, 0.13383f, 0.83777f
    };
    const float3x3 acesOutputMatrix =
    {
         1.60475f, -0.53108f, -0.07367f,
        -0.10208f,  1.10813f, -0.00605f,
        -0.00327f, -0.07276f,  1.07602f
    };

    float3 working = mul(acesInputMatrix, max(color, 0.0f));

    // RRTAndODTFit
    const float3 numerator = working * (working + 0.0245786f) - 0.000090537f;
    const float3 denominator = working *
        (0.983729f * working + 0.4329510f) + 0.238081f;
    working = numerator / max(denominator, 1.0e-10f);

    return saturate(mul(acesOutputMatrix, working));
}

// AgX, Troy Sobotka's transform via the widely used 6th-order contrast fit.
// Log-encodes into a bounded EV window, applies the sigmoid in Rec.2020, then
// returns to linear sRGB. Holds highlight hue better than ACES: very bright
// saturated regions desaturate toward white instead of skewing orange.
float3 TrTonemapAgx(float3 color)
{
    const float3x3 linearSrgbToRec2020 =
    {
        0.6274f, 0.3293f, 0.0433f,
        0.0691f, 0.9195f, 0.0113f,
        0.0164f, 0.0880f, 0.8956f
    };
    const float3x3 rec2020ToLinearSrgb =
    {
         1.6605f, -0.5876f, -0.0728f,
        -0.1246f,  1.1329f, -0.0083f,
        -0.0182f, -0.1006f,  1.1187f
    };
    const float3x3 agxInsetMatrix =
    {
        0.8566271533203144f, 0.09512124707823145f, 0.04825159960145414f,
        0.13731270824004778f, 0.7612419906405366f, 0.10144530111941561f,
        0.11189821299644263f, 0.07679941523873916f, 0.8113023717648182f
    };
    const float3x3 agxOutsetMatrix =
    {
         1.1271005818144368f, -0.11060664309660323f, -0.016493938717834573f,
        -0.1413297634984383f,   1.157823702216272f,  -0.016493938717834257f,
        -0.14132976349843826f, -0.11060664309660294f, 1.2519364065950405f
    };
    const float agxMinEv = -12.47393f;
    const float agxMaxEv = 4.026069f;

    float3 working = mul(linearSrgbToRec2020, max(color, 0.0f));
    working = mul(agxInsetMatrix, working);

    // Log2 encode into the normalized EV window.
    working = log2(max(working, 1.0e-10f));
    working = saturate((working - agxMinEv) / (agxMaxEv - agxMinEv));

    // 6th-order polynomial fit of the default AgX contrast sigmoid.
    const float3 x = working;
    const float3 x2 = x * x;
    const float3 x4 = x2 * x2;
    working = 15.5f * x4 * x2
        - 40.14f * x4 * x
        + 31.96f * x4
        - 6.868f * x2 * x
        + 0.4298f * x2
        + 0.1191f * x
        - 0.00232f;

    working = mul(agxOutsetMatrix, working);

    // The sigmoid emits display-encoded values. Undo that 2.2 encode so this
    // function returns linear display-referred color like the others, letting
    // the composite pass apply the single shared OETF.
    working = pow(max(working, 0.0f), 2.2f);

    return saturate(mul(rec2020ToLinearSrgb, working));
}

// Dispatches to the selected operator. Input must already be exposure-scaled.
float3 TrApplyTonemap(float3 linearColor, uint tonemapOperator)
{
    if(tonemapOperator == TR_TONEMAP_KHRONOS_PBR_NEUTRAL)
    {
        return TrTonemapKhronosPbrNeutral(linearColor);
    }
    if(tonemapOperator == TR_TONEMAP_ACES_FITTED)
    {
        return TrTonemapAcesFitted(linearColor);
    }
    if(tonemapOperator == TR_TONEMAP_AGX)
    {
        return TrTonemapAgx(linearColor);
    }
    return saturate(linearColor);
}

#endif
