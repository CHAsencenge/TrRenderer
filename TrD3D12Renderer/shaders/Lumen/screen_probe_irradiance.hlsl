#include "screen_probe_sampling.header.hlsl"
#include "../Common/Lighting/spherical_harmonics.header.hlsl"

Texture2D<float4> g_radiance : register(t0);
Texture2D<float4> g_probeNormalDepth : register(t1);
RWTexture2D<float4> g_irradiance : register(u0);

cbuffer ScreenProbeIrradianceConstants : register(b2)
{
    uint g_probeCountX;
    uint g_probeCountY;
    uint g_rayGridDimension;
    uint g_raysPerProbe;
    uint g_irradianceFrameNumber;
    uint3 g_irradiancePadding;
};

[numthreads(8, 8, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint2 probeCoordinate = dispatchThreadId.xy;
    if(any(probeCoordinate >= uint2(g_probeCountX, g_probeCountY)))
    {
        return;
    }

    float3 irradianceCoefficients[TR_SH_L2_COEFFICIENT_COUNT];
    [unroll]
    for(uint coefficientIndex = 0u;
        coefficientIndex < TR_SH_L2_COEFFICIENT_COUNT;
        ++coefficientIndex)
    {
        irradianceCoefficients[coefficientIndex] = 0.0f;
    }

    const float3 probeNormal = g_probeNormalDepth.Load(
        int3(probeCoordinate, 0)).xyz;
    const float probeNormalLengthSquared = dot(probeNormal, probeNormal);
    if(probeNormalLengthSquared < 1.0e-6f)
    {
        [unroll]
        for(uint coefficientIndex = 0u;
            coefficientIndex < TR_SH_L2_COEFFICIENT_COUNT;
            ++coefficientIndex)
        {
            g_irradiance[TrShL2AtlasCoordinate(
                probeCoordinate,
                coefficientIndex)] = 0.0f;
        }
        return;
    }

    const float3 normalizedProbeNormal = probeNormal *
        rsqrt(probeNormalLengthSquared);
    float confidenceSum = 0.0f;
    [loop]
    for(uint rayIndex = 0u; rayIndex < g_raysPerProbe; ++rayIndex)
    {
        const uint2 rayCoordinate = uint2(
            rayIndex % g_rayGridDimension,
            rayIndex / g_rayGridDimension);
        const uint2 tracePixel =
            probeCoordinate * g_rayGridDimension + rayCoordinate;
        const float4 radiance = g_radiance.Load(int3(tracePixel, 0));
        const float confidence = saturate(radiance.a);
        if(confidence <= 1.0e-6f)
        {
            continue;
        }

        const float3 rayDirection = TrGenerateScreenProbeRay(
            probeCoordinate,
            g_probeCountX,
            rayIndex,
            g_raysPerProbe,
            g_irradianceFrameNumber,
            normalizedProbeNormal);
        const float cosineAtProbe = saturate(dot(
            normalizedProbeNormal,
            rayDirection));
        
        // 余弦加权半球采样 p(w) = cosθ / pi
        const float samplePdf = max(
            cosineAtProbe / TR_SCREEN_PROBE_PI,
            1.0e-4f);
        float shBasis[TR_SH_L2_COEFFICIENT_COUNT];
        TrEvaluateShL2Basis(rayDirection, shBasis);
        const float3 weightedRadiance = max(radiance.rgb, 0.0f) * confidence / samplePdf;
        
        // L_lm = 1/N Σ {L(wi) Y_lm(wi) / p(wi)}
        // 把 Radiance 球面函数与 Clamped Cosine 核进行球面卷积 = 每个 SH band 分别乘一个常数，E_{lm}=A_l L_{lm}
        // 预卷积成 Irradiance SH
        [unroll]
        for(uint coefficientIndex = 0u;
            coefficientIndex < TR_SH_L2_COEFFICIENT_COUNT;
            ++coefficientIndex)
        {
            irradianceCoefficients[coefficientIndex] += weightedRadiance *
                shBasis[coefficientIndex] *
                TrShDiffuseConvolutionFactor(coefficientIndex); // A_l
        }
        confidenceSum += confidence;
    }

    const float inverseRayCount = 1.0f / max(float(g_raysPerProbe), 1.0f);
    const float confidence = confidenceSum * inverseRayCount;
    
    if (confidenceSum > 1.0e-6f)
    {
        for (uint coefficientIndex = 0u;
            coefficientIndex < TR_SH_L2_COEFFICIENT_COUNT;
            ++coefficientIndex)
        {
            irradianceCoefficients[coefficientIndex] /= confidenceSum;
        }
    }
    
    [unroll]
    for(uint coefficientIndex = 0u;
        coefficientIndex < TR_SH_L2_COEFFICIENT_COUNT;
        ++coefficientIndex)
    {
        g_irradiance[TrShL2AtlasCoordinate(probeCoordinate, coefficientIndex)] = 
            float4(irradianceCoefficients[coefficientIndex], confidence);
    }
}
