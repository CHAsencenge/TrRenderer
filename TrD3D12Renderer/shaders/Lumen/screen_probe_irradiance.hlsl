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
    float g_maxRayIntensity;
    uint2 g_irradiancePadding;
};

float3 TrClampScreenProbeRayIntensity(
    float3 radiance,
    float maxRayIntensity)
{
    radiance = max(radiance, 0.0f);
    const float maximumChannel = max(radiance.x, max(radiance.y, radiance.z));
    const float intensityLimit = max(maxRayIntensity, 0);
    if (maximumChannel > intensityLimit &&
       maximumChannel > 1.0e-6f)
    {
        radiance *= intensityLimit / maximumChannel;
    }
    return radiance;
}

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
    
    const float inverseRayCount =
    1.0f / max(float(g_raysPerProbe), 1.0f);

    const float sampleSolidAngle =
    2.0f * TR_SCREEN_PROBE_PI * inverseRayCount;
    
    // Sum of lighting evidence supplied by the screen traces. This is not
    // screen-space geometry coverage; probe geometry validity is stored in
    // g_probePositionValidity by the probe placement pass.
    float traceEvidenceSum = 0.0f;
    [loop]
    for(uint rayIndex = 0u; rayIndex < g_raysPerProbe; ++rayIndex)
    {
        const uint2 rayCoordinate = uint2(
            rayIndex % g_rayGridDimension,
            rayIndex / g_rayGridDimension);
        const uint2 tracePixel =
            probeCoordinate * g_rayGridDimension + rayCoordinate;
        const float4 radiance = g_radiance.Load(int3(tracePixel, 0));
        // Per-ray hit quality from the depth-thickness and screen-edge tests.
        const float rayTraceConfidence = saturate(radiance.a);
        
        traceEvidenceSum += rayTraceConfidence;
        
        if(rayTraceConfidence <= 1.0e-6f)
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
        
        const float3 clampedRadiance =
        TrClampScreenProbeRayIntensity(
            radiance.rgb,
            g_maxRayIntensity);
        
        const float3 resolvedRadiance =
            max(clampedRadiance.rgb, 0.0f) * rayTraceConfidence;
        
        float shBasis[TR_SH_L2_COEFFICIENT_COUNT];
        TrEvaluateShL2Basis(rayDirection, shBasis);
        
        // 预卷积成 Irradiance SH
        [unroll]
        for(uint coefficientIndex = 0u;
            coefficientIndex < TR_SH_L2_COEFFICIENT_COUNT;
            ++coefficientIndex)
        {
            // Radiance SH 投影：
            //
            //     L_lm ≈ Δω Σ L_i Y_lm(w_i)
            //
            // 再乘 Lambert clamped-cosine convolution：把 Radiance 球面函数与 Clamped Cosine 核进行球面卷积 = 每个 SH band 分别乘一个常数
            //
            //     E_lm = A_l L_lm
            irradianceCoefficients[coefficientIndex] +=
                resolvedRadiance *
                shBasis[coefficientIndex] *
                TrShDiffuseConvolutionFactor(coefficientIndex) * // A_l
                sampleSolidAngle; 
        }

    }

    const float probeTraceConfidence = traceEvidenceSum * inverseRayCount;
    
    [unroll]
    for(uint coefficientIndex = 0u;
        coefficientIndex < TR_SH_L2_COEFFICIENT_COUNT;
        ++coefficientIndex)
    {
        g_irradiance[TrShL2AtlasCoordinate(
            probeCoordinate,
            coefficientIndex)] = float4(
                irradianceCoefficients[coefficientIndex],
                probeTraceConfidence);
    }
}
