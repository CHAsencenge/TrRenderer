#include "../Common/ABI/light_types.header.hlsl"
#include "../Common/ABI/scene_constants.header.hlsl"
#include "../Common/ABI/view_constants.header.hlsl"
#include "../Common/Lighting/light_evaluation.header.hlsl"
#include "../Common/Lighting/spherical_harmonics.header.hlsl"
#include "../Common/Lighting/surface_lighting.header.hlsl"
#include "../Common/Utility/depth.header.hlsl"
#include "../Common/Utility/fullscreen_triangle.header.hlsl"
#include "../Common/Utility/view_projection.header.hlsl"

static const uint TR_FEATURE_INDIRECT_LIGHTING = 1u << 0u;

ConstantBuffer<TrSceneConstants> g_sceneConstants : register(b0);
ConstantBuffer<TrViewConstants> g_viewConstants : register(b1);

cbuffer DeferredLightingPassConstants : register(b2)
{
    float g_directLightingScale;
    float g_ambientLightingScale;
    float g_indirectLightingScale;
    float g_normalWeightPower;
    float g_planeDistanceWeight;
    float g_bilinearExpandPixels;
    uint g_pipelineFeatureMask;
    uint g_lightingVisualization;
};

Texture2D<float4> g_baseColorRoughness : register(t0);
Texture2D<float4> g_normalMetallic : register(t1);
Texture2D<float> g_depth : register(t2);
Texture2D<float4> g_emissiveOcclusion : register(t3);
Texture2D<float4> g_probeNormalDepth : register(t4);
Texture2D<float4> g_probeIrradiance : register(t5);
StructuredBuffer<TrGpuLight> g_lights : register(t6);
Texture2D<float4> g_probePositionValidity : register(t7);

float2 TrExpandBilinearFraction(
    float2 bilinearFraction,
    float2 probeSpacingPixels,
    float expandPixels)
{
    // 普通 bilinear：
    //
    //     f = localPixel / cellSize
    //
    // Expanded bilinear：
    //
    //     fExpanded =
    //         (localPixel + expandPixels) /
    //         (cellSize + 2 * expandPixels)
    //
    // 因为 localPixel = f * cellSize，所以：
    //
    //     fExpanded =
    //         (f * cellSize + expandPixels) /
    //         (cellSize + 2 * expandPixels).
    //
    // expandPixels = 0 时严格退化为普通 bilinear。
    const float expansion = max(expandPixels, 0.0f);
    const float2 denominator = probeSpacingPixels + 2.0f * expansion;
    return saturate(
        (bilinearFraction * probeSpacingPixels + expansion) /
        max(denominator, 1.0e-5f));
}

float4 TrComputeBilinearWeights(float2 fraction)
{
    const float2 inverseFraction = 1.0f - fraction;

    return float4(
        inverseFraction.x * inverseFraction.y,
        fraction.x * inverseFraction.y,
        inverseFraction.x * fraction.y,
        fraction.x * fraction.y);
}

struct TrUpsampledProbeIrradiance
{
    float3 irradiance;
    // Sum of spatial, normal and plane weights from valid probe geometry.
    float geometrySupport;
    // Geometry-weighted availability of resolved probe lighting.
    float traceConfidence;
};

TrUpsampledProbeIrradiance TrUpsampleProbeIrradiance(
    uint2 pixel,
    float deviceDepth,
    float3 worldPosition,
    float3 worldNormal)
{
    uint probeCountX;
    uint probeCountY;
    g_probeNormalDepth.GetDimensions(probeCountX, probeCountY);

    const float2 probeCount = float2(probeCountX, probeCountY);
    const float2 probeSpacingPixels = g_viewConstants.renderSize / max(probeCount, 1.0f);

    const float2 continuousProbeCoordinate = (float2(pixel) + 0.5f) / probeSpacingPixels - 0.5f;
    const int2 baseProbeCoordinate = int2(floor(continuousProbeCoordinate));
    const float2 probeFraction = frac(continuousProbeCoordinate);

    const float2 expandedProbeFraction = TrExpandBilinearFraction(
                                            probeFraction,
                                            probeSpacingPixels,
                                            g_bilinearExpandPixels);
    const float4 spatialWeights = TrComputeBilinearWeights(expandedProbeFraction);

    const float pixelViewDepth = TrDeviceDepthToViewDepth(
        deviceDepth,
        g_viewConstants.nearPlane,
        g_viewConstants.farPlane);
    const float inverseDepthScale = rcp(max(pixelViewDepth, 1.0e-4f));

    float3 traceWeightedIrradiance = 0.0f;
    float geometryWeightSum = 0.0f;
    float radianceWeightSum = 0.0f;
    float shBasis[TR_SH_L2_COEFFICIENT_COUNT];
    TrEvaluateShL2Basis(worldNormal, shBasis);
    [unroll]
    for(int y = 0; y < 2; ++y)
    {
        [unroll]
        for(int x = 0; x < 2; ++x)
        {
            const int2 unclampedProbe = baseProbeCoordinate + int2(x, y);
            const uint2 probeCoordinate = uint2(clamp(
                unclampedProbe,
                int2(0, 0),
                int2(probeCountX - 1u, probeCountY - 1u)));

            const uint spatialWeightIndex = uint(y) * 2u + uint(x);
            const float spatialWeight = spatialWeights[spatialWeightIndex];

            const float4 probeNormalDepth = g_probeNormalDepth.Load(int3(probeCoordinate, 0));
            const float4 probeSh0 = g_probeIrradiance.Load(int3(TrShL2AtlasCoordinate(probeCoordinate, 0u), 0));
            const float4 probePositionValidity = g_probePositionValidity.Load(int3(probeCoordinate, 0));
            
            const float probeNormalLengthSquared = dot(
                probeNormalDepth.xyz,
                probeNormalDepth.xyz);
            
            // Probe geometry validity is independent of whether its screen
            // traces produced usable lighting evidence.
            if(probePositionValidity.w < 0.5f ||
               probeNormalLengthSquared < 1.0e-6f)
            {
                continue;
            }

            const float3 probeNormal = probeNormalDepth.xyz *
                rsqrt(probeNormalLengthSquared);
            const float normalWeight = pow(
                saturate(dot(worldNormal, probeNormal)),
                max(g_normalWeightPower, 1.0f));
            
            // Distance from the probe position to the tangent plane at the
            // currently shaded pixel:
            //
            //     d_plane = |(P_probe - P_pixel) dot N_pixel|
            //
            // Tangential displacement has no effect, while displacement across
            // the current surface is exponentially rejected.
            const float planeDistance = abs(dot(probePositionValidity.xyz - worldPosition, worldNormal));
            const float relativePlaneDistance = planeDistance * inverseDepthScale;

            // UE-style plane-aware interpolation weight:
            //
            //     w_plane = 2 ^ (-K * d_plane / viewDepth)
            const float planeWeight = exp2(-max(g_planeDistanceWeight, 0.0f) * relativePlaneDistance);

            const float geometryWeight =
                spatialWeight * normalWeight * planeWeight;

            // Geometry support must be accumulated before trace confidence is
            // considered. A probe can geometrically cover this pixel even
            // when its lighting is unresolved.
            geometryWeightSum += geometryWeight;

            const float probeTraceConfidence = saturate(probeSh0.a);
            if(geometryWeight <= 1.0e-6f ||
               probeTraceConfidence <= 1.0e-6f)
            {
                continue;
            }

            // Confidence selects reliable lighting samples. Division by the
            // same sum below prevents confidence from directly dimming energy.
            const float radianceWeight =
                geometryWeight * probeTraceConfidence;

            // E(N_pixel) = Σ_lm E_lm Y_lm(N_pixel)
            float3 probeIrradiance = probeSh0.rgb * shBasis[0];
            [unroll]
            for(uint coefficientIndex = 1u;
                coefficientIndex < TR_SH_L2_COEFFICIENT_COUNT;
                ++coefficientIndex)
            {
                // 重建 irradiance，E(N_pixel) = Σ_lm E_lm Y_lm(N_pixel)
                const float3 coefficient = g_probeIrradiance.Load(int3(
                    TrShL2AtlasCoordinate(
                        probeCoordinate,
                        coefficientIndex),
                    0)).rgb;
                probeIrradiance += coefficient * shBasis[coefficientIndex];
            }
            traceWeightedIrradiance +=
                max(probeIrradiance, 0.0f) * radianceWeight;
            radianceWeightSum += radianceWeight;
        }
    }

    TrUpsampledProbeIrradiance result;
    result.irradiance = 0.0f;
    result.geometrySupport = geometryWeightSum;
    result.traceConfidence = 0.0f;

    if(geometryWeightSum <= 1.0e-6f)
    {
        // No candidate probe geometrically covers the shaded surface.
        return result;
    }

    result.traceConfidence = saturate(
        radianceWeightSum / geometryWeightSum);

    if(radianceWeightSum <= 1.0e-6f)
    {
        // Geometry exists, but the current tracing and temporal stages did
        // not produce usable lighting. A future non-screen fallback belongs
        // here; constant ambient must remain a separate lighting component.
        return result;
    }

    result.irradiance =
        traceWeightedIrradiance / radianceWeightSum;
    return result;
}

TrFullscreenVertex VSMain(uint vertexId : SV_VertexID)
{
    return TrCreateFullscreenTriangleVertex(vertexId);
}

float4 PSMain(TrFullscreenVertex input) : SV_Target
{
    const int2 pixel = min(
        int2(input.position.xy),
        int2(g_viewConstants.renderSize) - 1);
    const float depth = g_depth.Load(int3(pixel, 0));
    if(TrIsBackgroundDepth(depth))
    {
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }

    const float4 baseColorRoughness =
        g_baseColorRoughness.Load(int3(pixel, 0));
    const float3 baseColor = baseColorRoughness.rgb;
    const float roughness = baseColorRoughness.a;
    const float4 normalMetallic = g_normalMetallic.Load(int3(pixel, 0));
    const float3 worldNormal = normalize(normalMetallic.xyz);
    const float metallic = normalMetallic.a;
    const float4 emissiveOcclusion = g_emissiveOcclusion.Load(int3(pixel, 0));
    const float3 worldPosition = TrReconstructWorldPosition(
        uint2(pixel),
        depth,
        g_viewConstants.inverseRenderSize,
        g_viewConstants.inverseViewProjection);
    const float3 directionToView = normalize(
        g_viewConstants.cameraPosition - worldPosition);
    const TrDirectPbrRadiance directRadiance = TrEvaluateDirectPbrRadiance(
        g_lights,
        g_sceneConstants.lightCount,
        worldPosition,
        worldNormal,
        directionToView,
        baseColor,
        metallic,
        roughness,
        g_directLightingScale);
    const float3 ambientRadiance = TrEvaluateAmbientDiffuseRadiance(
        baseColor,
        metallic,
        g_sceneConstants.ambientColor,
        g_sceneConstants.ambientStrength,
        g_ambientLightingScale,
        emissiveOcclusion.a);
    float3 screenProbeDiffuseRadiance = 0.0f;
    if((g_pipelineFeatureMask & TR_FEATURE_INDIRECT_LIGHTING) != 0u)
    {
        const TrUpsampledProbeIrradiance probeSample =
            TrUpsampleProbeIrradiance(
            uint2(pixel),
            depth,
            worldPosition,
            worldNormal);
        screenProbeDiffuseRadiance = TrEvaluateIndirectDiffuseRadiance(
            baseColor,
            metallic,
            emissiveOcclusion.a,
            probeSample.irradiance,
            g_indirectLightingScale);
    }
    return float4(TrResolveLightingVisualization(
        directRadiance,
        ambientRadiance,
        screenProbeDiffuseRadiance,
        emissiveOcclusion.rgb,
        g_lightingVisualization), 1.0f);
}
