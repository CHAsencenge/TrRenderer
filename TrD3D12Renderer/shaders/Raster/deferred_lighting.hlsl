#include "../Common/depth.header.hlsl"
#include "../Common/deferred_lighting_common.header.hlsl"

struct FullscreenVertex
{
    float4 position : SV_POSITION;
};

cbuffer ViewConstants : register(b1)
{
    float4x4 g_view;
    float4x4 g_projection;
    float4x4 g_viewProjection;
    float4x4 g_inverseViewProjection;
    float4x4 g_previousViewProjection;
    float3 g_cameraPosition;
    float g_nearPlane;
    float2 g_renderSize;
    float2 g_inverseRenderSize;
    float2 g_temporalJitter;
    float2 g_previousTemporalJitter;
    uint g_frameNumber;
    float g_farPlane;
    float2 g_viewPadding;
};

Texture2D<float4> g_baseColorRoughness : register(t0);
Texture2D<float4> g_normalMetallic : register(t1);
Texture2D<float> g_depth : register(t2);
Texture2D<float4> g_emissiveOcclusion : register(t3);
Texture2D<float4> g_probeNormalDepth : register(t4);
Texture2D<float4> g_probeIrradiance : register(t5);

float3 TrUpsampleProbeIrradiance(
    uint2 pixel,
    float deviceDepth,
    float3 worldNormal)
{
    uint probeCountX;
    uint probeCountY;
    g_probeIrradiance.GetDimensions(probeCountX, probeCountY);
    const float2 continuousProbeCoordinate =
        (float2(pixel) + 0.5f) *
        float2(probeCountX, probeCountY) / g_renderSize - 0.5f;
    const int2 baseProbeCoordinate = int2(floor(continuousProbeCoordinate));
    const float2 probeFraction = frac(continuousProbeCoordinate);
    const float pixelViewDepth = TrDeviceDepthToViewDepth(
        deviceDepth,
        g_nearPlane,
        g_farPlane);
    const float depthThreshold = max(
        g_minimumDepthThreshold,
        pixelViewDepth * g_relativeDepthThreshold);

    float3 weightedIrradiance = 0.0f;
    float weightSum = 0.0f;
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
            const float spatialWeightX = x == 0
                ? 1.0f - probeFraction.x
                : probeFraction.x;
            const float spatialWeight = spatialWeightX *
                (y == 0 ? 1.0f - probeFraction.y : probeFraction.y);

            const float4 probeNormalDepth =
                g_probeNormalDepth.Load(int3(probeCoordinate, 0));
            const float4 probeIrradiance =
                g_probeIrradiance.Load(int3(probeCoordinate, 0));
            const float probeNormalLengthSquared = dot(
                probeNormalDepth.xyz,
                probeNormalDepth.xyz);
            if(probeNormalLengthSquared < 1.0e-6f ||
               probeIrradiance.a <= 1.0e-6f)
            {
                continue;
            }

            const float3 probeNormal = probeNormalDepth.xyz *
                rsqrt(probeNormalLengthSquared);
            const float normalWeight = pow(
                saturate(dot(worldNormal, probeNormal)),
                max(g_normalWeightPower, 1.0f));
            const float probeViewDepth = TrDeviceDepthToViewDepth(
                probeNormalDepth.w,
                g_nearPlane,
                g_farPlane);
            float depthWeight = saturate(
                1.0f - abs(pixelViewDepth - probeViewDepth) /
                depthThreshold);
            depthWeight *= depthWeight;

            const float weight = spatialWeight * normalWeight * depthWeight *
                saturate(probeIrradiance.a);
            weightedIrradiance += probeIrradiance.rgb * weight;
            weightSum += weight;
        }
    }

    return weightSum > 1.0e-6f
        ? weightedIrradiance / weightSum
        : 0.0f;
}

FullscreenVertex VSMain(uint vertexId : SV_VertexID)
{
    const float2 positions[3] =
    {
        float2(-1.0f, -1.0f),
        float2(-1.0f,  3.0f),
        float2( 3.0f, -1.0f)
    };

    FullscreenVertex result;
    result.position = float4(positions[vertexId], 0.0f, 1.0f);
    return result;
}

float4 PSMain(FullscreenVertex input) : SV_Target
{
    const int2 pixel = min(
        int2(input.position.xy),
        int2(g_renderSize) - 1);
    const float depth = g_depth.Load(int3(pixel, 0));
    if(TrIsBackgroundDepth(depth))
    {
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }

    const float3 baseColor = g_baseColorRoughness.Load(int3(pixel, 0)).rgb;
    const float4 normalMetallic = g_normalMetallic.Load(int3(pixel, 0));
    const float3 worldNormal = normalize(normalMetallic.xyz);
    const float4 emissiveOcclusion = g_emissiveOcclusion.Load(int3(pixel, 0));
    const float3 directRadiance = TrEvaluateDirectSurfaceRadiance(
        baseColor,
        worldNormal,
        emissiveOcclusion.rgb);
    const float3 ambientRadiance = TrEvaluateAmbientSurfaceRadiance(
        baseColor,
        emissiveOcclusion.a);
    float3 indirectRadiance = 0.0f;
    if((g_pipelineFeatureMask & TR_FEATURE_INDIRECT_LIGHTING) != 0u)
    {
        const float3 irradiance = TrUpsampleProbeIrradiance(
            uint2(pixel),
            depth,
            worldNormal);
        indirectRadiance = TrEvaluateIndirectSurfaceRadiance(
            baseColor,
            normalMetallic.a,
            emissiveOcclusion.a,
            irradiance);
    }
    return float4(
        directRadiance + ambientRadiance + indirectRadiance,
        1.0f);
}
