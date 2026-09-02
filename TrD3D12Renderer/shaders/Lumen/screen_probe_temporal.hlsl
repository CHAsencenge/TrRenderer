#include "screen_probe_common.header.hlsl"

Texture2D<float4> g_currentIrradiance : register(t0);
Texture2D<float4> g_currentPositionValidity : register(t1);
Texture2D<float4> g_currentNormalDepth : register(t2);
Texture2D<float4> g_previousIrradiance : register(t3);
Texture2D<float4> g_previousPositionValidity : register(t4);
Texture2D<float4> g_previousNormalDepth : register(t5);
RWTexture2D<float4> g_outputIrradiance : register(u0);
RWTexture2D<float4> g_outputPositionValidity : register(u1);
RWTexture2D<float4> g_outputNormalDepth : register(u2);

cbuffer ScreenProbeTemporalConstants : register(b2)
{
    uint g_probeCountX;
    uint g_probeCountY;
    uint g_historyValid;
    uint g_temporalFrameNumber;
    float g_staticHistoryWeight;
    float g_normalSimilarityThreshold;
    float g_relativePositionThreshold;
    float g_minimumPositionThreshold;
};

bool ProjectToPreviousProbeGrid(
    float3 worldPosition,
    out float2 continuousProbeCoordinate)
{
    const float4 previousClip = mul(
        float4(worldPosition, 1.0f),
        g_previousViewProjection);
    if(previousClip.w <= 1.0e-6f)
    {
        continuousProbeCoordinate = 0.0f;
        return false;
    }

    float3 previousNdc = previousClip.xyz / previousClip.w;
    previousNdc.xy += g_previousTemporalJitter;
    const float2 previousUv = float2(
        previousNdc.x * 0.5f + 0.5f,
        0.5f - previousNdc.y * 0.5f);
    continuousProbeCoordinate =
        previousUv * float2(g_probeCountX, g_probeCountY) - 0.5f;
    return all(previousUv >= 0.0f) && all(previousUv < 1.0f) &&
        previousNdc.z >= 0.0f && previousNdc.z <= 1.0f;
}

[numthreads(8, 8, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint2 probeCoordinate = dispatchThreadId.xy;
    if(any(probeCoordinate >= uint2(g_probeCountX, g_probeCountY)))
    {
        return;
    }

    const float4 currentPosition =
        g_currentPositionValidity.Load(int3(probeCoordinate, 0));
    const float4 currentNormalDepth =
        g_currentNormalDepth.Load(int3(probeCoordinate, 0));
    const float4 currentIrradiance =
        g_currentIrradiance.Load(int3(probeCoordinate, 0));

    // Geometry history always follows the current probe placement, even when
    // lighting history is rejected. It becomes next frame's identity test.
    g_outputPositionValidity[probeCoordinate] = currentPosition;
    g_outputNormalDepth[probeCoordinate] = currentNormalDepth;

    const float currentNormalLengthSquared = dot(
        currentNormalDepth.xyz,
        currentNormalDepth.xyz);
    if(currentPosition.w < 0.5f || currentNormalLengthSquared < 1.0e-6f)
    {
        g_outputIrradiance[probeCoordinate] = 0.0f;
        return;
    }
    if(g_historyValid == 0u)
    {
        g_outputIrradiance[probeCoordinate] = currentIrradiance;
        return;
    }

    float2 previousProbeCoordinate;
    if(!ProjectToPreviousProbeGrid(
           currentPosition.xyz,
           previousProbeCoordinate))
    {
        g_outputIrradiance[probeCoordinate] = currentIrradiance;
        return;
    }

    const float3 currentNormal = currentNormalDepth.xyz *
        rsqrt(currentNormalLengthSquared);
    const float positionThreshold = max(
        g_minimumPositionThreshold,
        length(currentPosition.xyz - g_cameraPosition) *
            g_relativePositionThreshold);
    const int2 baseProbe = int2(floor(previousProbeCoordinate));
    const float2 probeFraction = frac(previousProbeCoordinate);

    float3 weightedHistory = 0.0f;
    float weightedHistoryConfidence = 0.0f;
    float historyWeightSum = 0.0f;
    [unroll]
    for(int y = 0; y < 2; ++y)
    {
        [unroll]
        for(int x = 0; x < 2; ++x)
        {
            const int2 candidate = baseProbe + int2(x, y);
            if(any(candidate < 0) ||
               any(candidate >= int2(g_probeCountX, g_probeCountY)))
            {
                continue;
            }

            const float4 previousPosition =
                g_previousPositionValidity.Load(int3(candidate, 0));
            const float4 previousNormalDepth =
                g_previousNormalDepth.Load(int3(candidate, 0));
            const float4 previousIrradiance =
                g_previousIrradiance.Load(int3(candidate, 0));
            const float previousNormalLengthSquared = dot(
                previousNormalDepth.xyz,
                previousNormalDepth.xyz);
            if(previousPosition.w < 0.5f ||
               previousNormalLengthSquared < 1.0e-6f ||
               previousIrradiance.a <= 1.0e-6f)
            {
                continue;
            }

            const float3 previousNormal = previousNormalDepth.xyz *
                rsqrt(previousNormalLengthSquared);
            const float normalSimilarity = dot(
                currentNormal,
                previousNormal);
            if(normalSimilarity < g_normalSimilarityThreshold)
            {
                continue;
            }

            const float positionDistance = length(
                currentPosition.xyz - previousPosition.xyz);
            if(positionDistance >= positionThreshold)
            {
                continue;
            }

            const float spatialWeight =
                (x == 0 ? 1.0f - probeFraction.x : probeFraction.x) *
                (y == 0 ? 1.0f - probeFraction.y : probeFraction.y);
            const float normalWeight = saturate(
                (normalSimilarity - g_normalSimilarityThreshold) /
                max(1.0f - g_normalSimilarityThreshold, 1.0e-4f));
            const float positionWeight = saturate(
                1.0f - positionDistance / positionThreshold);
            const float weight = spatialWeight * normalWeight *
                positionWeight * saturate(previousIrradiance.a);
            weightedHistory += previousIrradiance.rgb * weight;
            weightedHistoryConfidence += previousIrradiance.a * weight;
            historyWeightSum += weight;
        }
    }

    if(historyWeightSum <= 1.0e-6f)
    {
        g_outputIrradiance[probeCoordinate] = currentIrradiance;
        return;
    }

    const float3 historyIrradiance = weightedHistory / historyWeightSum;
    const float historyConfidence = saturate(
        weightedHistoryConfidence / historyWeightSum);
    // Ramp history in over the first few frames after a reset. The frame
    // number also restarts on resize, matching history invalidation.
    const float startupRamp = saturate(float(g_temporalFrameNumber) / 4.0f);
    const float historyWeight = saturate(
        g_staticHistoryWeight * startupRamp * historyConfidence);
    const float resolvedConfidence = lerp(
        saturate(currentIrradiance.a),
        historyConfidence,
        historyWeight);
    g_outputIrradiance[probeCoordinate] = float4(
        lerp(currentIrradiance.rgb, historyIrradiance, historyWeight),
        resolvedConfidence);
}
