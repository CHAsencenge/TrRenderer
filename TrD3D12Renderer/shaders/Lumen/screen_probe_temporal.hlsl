#include "../Common/ABI/view_constants.header.hlsl"
#include "../Common/Lighting/spherical_harmonics.header.hlsl"

ConstantBuffer<TrViewConstants> g_viewConstants : register(b1);

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
        g_viewConstants.previousViewProjection);
    if(previousClip.w <= 1.0e-6f)
    {
        continuousProbeCoordinate = 0.0f;
        return false;
    }

    float3 previousNdc = previousClip.xyz / previousClip.w;
    previousNdc.xy += g_viewConstants.previousTemporalJitter;
    const float2 previousUv = float2(
        previousNdc.x * 0.5f + 0.5f,
        0.5f - previousNdc.y * 0.5f);
    continuousProbeCoordinate =
        previousUv * float2(g_probeCountX, g_probeCountY) - 0.5f;
    return all(previousUv >= 0.0f) && all(previousUv < 1.0f) &&
        previousNdc.z >= 0.0f && previousNdc.z <= 1.0f;
}

void StoreCurrentIrradianceSh(uint2 probeCoordinate)
{
    [unroll]
    for(uint coefficientIndex = 0u;
        coefficientIndex < TR_SH_L2_COEFFICIENT_COUNT;
        ++coefficientIndex)
    {
        const uint2 atlasCoordinate = TrShL2AtlasCoordinate(
            probeCoordinate,
            coefficientIndex);
        g_outputIrradiance[atlasCoordinate] =
            g_currentIrradiance.Load(int3(atlasCoordinate, 0));
    }
}

void StoreZeroIrradianceSh(uint2 probeCoordinate)
{
    [unroll]
    for(uint coefficientIndex = 0u;
        coefficientIndex < TR_SH_L2_COEFFICIENT_COUNT;
        ++coefficientIndex)
    {
        g_outputIrradiance[TrShL2AtlasCoordinate(
            probeCoordinate,
            coefficientIndex)] = 0.0f;
    }
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
    // Geometry history always follows the current probe placement, even when
    // lighting history is rejected. It becomes next frame's identity test.
    g_outputPositionValidity[probeCoordinate] = currentPosition;
    g_outputNormalDepth[probeCoordinate] = currentNormalDepth;

    const float currentNormalLengthSquared = dot(
        currentNormalDepth.xyz,
        currentNormalDepth.xyz);
    if(currentPosition.w < 0.5f || currentNormalLengthSquared < 1.0e-6f)
    {
        StoreZeroIrradianceSh(probeCoordinate);
        return;
    }
    if(g_historyValid == 0u)
    {
        StoreCurrentIrradianceSh(probeCoordinate);
        return;
    }

    float2 previousProbeCoordinate;
    if(!ProjectToPreviousProbeGrid(
           currentPosition.xyz,
           previousProbeCoordinate))
    {
        StoreCurrentIrradianceSh(probeCoordinate);
        return;
    }

    const float3 currentNormal = currentNormalDepth.xyz *
        rsqrt(currentNormalLengthSquared);
    const float positionThreshold = max(
        g_minimumPositionThreshold,
        length(currentPosition.xyz - g_viewConstants.cameraPosition) *
            g_relativePositionThreshold);
    const int2 baseProbe = int2(floor(previousProbeCoordinate));
    const float2 probeFraction = frac(previousProbeCoordinate);

    float3 confidenceWeightedHistory[TR_SH_L2_COEFFICIENT_COUNT];
    [unroll]
    for(uint coefficientIndex = 0u;
        coefficientIndex < TR_SH_L2_COEFFICIENT_COUNT;
        ++coefficientIndex)
    {
        confidenceWeightedHistory[coefficientIndex] = 0.0f;
    }
    
    float geometryWeightSum = 0.0f;
    float radianceWeightSum = 0.0f;
    
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
            const uint2 candidateProbe = uint2(candidate);
            const float4 previousSh0 = g_previousIrradiance.Load(int3(
                TrShL2AtlasCoordinate(candidateProbe, 0u),
                0));
            const float previousNormalLengthSquared = dot(
                previousNormalDepth.xyz,
                previousNormalDepth.xyz);
            if(previousPosition.w < 0.5f ||
               previousNormalLengthSquared < 1.0e-6f ||
               previousSh0.a <= 1.0e-6f)
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
            
            float geometryWeight = spatialWeight * normalWeight * positionWeight; 
            float previousConfidence = saturate(previousSh0.a); // previousSh0.a: 上一帧该 Probe 的历史光照置信度
            float radianceWeight = geometryWeight * previousConfidence;
            
            [unroll]
            for(uint coefficientIndex = 0u;
                coefficientIndex < TR_SH_L2_COEFFICIENT_COUNT;
                ++coefficientIndex)
            {
                const float3 previousCoefficient =
                    g_previousIrradiance.Load(int3(
                        TrShL2AtlasCoordinate(
                            candidateProbe,
                            coefficientIndex),
                        0)).rgb;
                confidenceWeightedHistory[coefficientIndex] += previousCoefficient * radianceWeight;
            }
            geometryWeightSum += geometryWeight;
            radianceWeightSum += radianceWeight;
        }
    }

    if (geometryWeightSum <= 1.0e-6f)
    {
        StoreCurrentIrradianceSh(probeCoordinate);
        return;
    }
    
    const float currentConfidence = saturate(
        g_currentIrradiance.Load(int3(
            TrShL2AtlasCoordinate(probeCoordinate, 0u),
            0)).a);
    
    float historyConfidence = radianceWeightSum / geometryWeightSum;
    
    // 归一化置信度混合
    // 当前 confidence 低、历史高：更多使用历史
    // 当前 confidence 高、历史低：更多使用当前
    // 两者都高：保持默认约 90% 历史
    // 历史几何验证失败：完全使用当前
    const float currentEvidence =
    (1.0f - g_staticHistoryWeight) *
    currentConfidence; // (1-α)C_c

    const float historyEvidence =
    g_staticHistoryWeight *
    historyConfidence; // αC_h

    const float evidenceSum =
    currentEvidence + historyEvidence;

    // αC_h / {(1-α)C_c + αC_h}
    const float historyWeight =
    evidenceSum > 1.0e-6f
        ? historyEvidence / evidenceSum
        : 0.0f;
    
    const float resolvedConfidence = lerp(
        currentConfidence,
        historyConfidence,
        historyWeight);
    
    [unroll]
    for(uint coefficientIndex = 0u;
        coefficientIndex < TR_SH_L2_COEFFICIENT_COUNT;
        ++coefficientIndex)
    {
        const uint2 atlasCoordinate = TrShL2AtlasCoordinate(
            probeCoordinate,
            coefficientIndex);
        
        const float3 currentCoefficient = g_currentIrradiance.Load(int3(atlasCoordinate, 0)).rgb;
        const float3 historyCoefficient = confidenceWeightedHistory[coefficientIndex] / radianceWeightSum;
        
        g_outputIrradiance[atlasCoordinate] = float4(
            lerp(currentCoefficient, historyCoefficient, historyWeight),
            resolvedConfidence);
    }
}
