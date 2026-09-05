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
    float g_relativeDepthThreshold;
    float g_minimumDepthThreshold;
    float g_normalWeightPower;
    uint g_pipelineFeatureMask;
    float g_lightingPadding;
};

Texture2D<float4> g_baseColorRoughness : register(t0);
Texture2D<float4> g_normalMetallic : register(t1);
Texture2D<float> g_depth : register(t2);
Texture2D<float4> g_emissiveOcclusion : register(t3);
Texture2D<float4> g_probeNormalDepth : register(t4);
Texture2D<float4> g_probeIrradiance : register(t5);
StructuredBuffer<TrGpuLight> g_lights : register(t6);

float3 TrUpsampleProbeIrradiance(
    uint2 pixel,
    float deviceDepth,
    float3 worldNormal)
{
    uint probeCountX;
    uint probeCountY;
    g_probeNormalDepth.GetDimensions(probeCountX, probeCountY);
    const float2 continuousProbeCoordinate =
        (float2(pixel) + 0.5f) *
        float2(probeCountX, probeCountY) / g_viewConstants.renderSize - 0.5f;
    const int2 baseProbeCoordinate = int2(floor(continuousProbeCoordinate));
    const float2 probeFraction = frac(continuousProbeCoordinate);
    const float pixelViewDepth = TrDeviceDepthToViewDepth(
        deviceDepth,
        g_viewConstants.nearPlane,
        g_viewConstants.farPlane);
    const float depthThreshold = max(
        g_minimumDepthThreshold,
        pixelViewDepth * g_relativeDepthThreshold);

    float3 weightedIrradiance = 0.0f;
    float weightSum = 0.0f;
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
            const float spatialWeightX = x == 0
                ? 1.0f - probeFraction.x
                : probeFraction.x;
            const float spatialWeight = spatialWeightX *
                (y == 0 ? 1.0f - probeFraction.y : probeFraction.y);

            const float4 probeNormalDepth =
                g_probeNormalDepth.Load(int3(probeCoordinate, 0));
            const float4 probeSh0 = g_probeIrradiance.Load(int3(
                TrShL2AtlasCoordinate(probeCoordinate, 0u),
                0));
            const float probeNormalLengthSquared = dot(
                probeNormalDepth.xyz,
                probeNormalDepth.xyz);
            if(probeNormalLengthSquared < 1.0e-6f ||
               probeSh0.a <= 1.0e-6f)
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
                g_viewConstants.nearPlane,
                g_viewConstants.farPlane);
            float depthWeight = saturate(
                1.0f - abs(pixelViewDepth - probeViewDepth) /
                depthThreshold);
            depthWeight *= depthWeight;

            const float weight = spatialWeight * normalWeight * depthWeight *
                saturate(probeSh0.a);
            
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
            weightedIrradiance += max(probeIrradiance, 0.0f) * weight;
            weightSum += weight;
        }
    }

    return weightSum > 1.0e-6f
        ? weightedIrradiance / weightSum
        : 0.0f;
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

    const float3 baseColor = g_baseColorRoughness.Load(int3(pixel, 0)).rgb;
    const float4 normalMetallic = g_normalMetallic.Load(int3(pixel, 0));
    const float3 worldNormal = normalize(normalMetallic.xyz);
    const float4 emissiveOcclusion = g_emissiveOcclusion.Load(int3(pixel, 0));
    const float3 worldPosition = TrReconstructWorldPosition(
        uint2(pixel),
        depth,
        g_viewConstants.inverseRenderSize,
        g_viewConstants.inverseViewProjection);
    const float3 directIrradiance = TrEvaluateDirectIrradiance(
        g_lights,
        g_sceneConstants.lightCount,
        worldPosition,
        worldNormal);
    const float3 directRadiance = TrEvaluateDirectDiffuseRadiance(
        baseColor,
        directIrradiance,
        g_directLightingScale) + emissiveOcclusion.rgb;
    const float3 ambientRadiance = TrEvaluateAmbientDiffuseRadiance(
        baseColor,
        g_sceneConstants.ambientColor,
        g_sceneConstants.ambientStrength,
        g_ambientLightingScale,
        emissiveOcclusion.a);
    float3 indirectRadiance = 0.0f;
    if((g_pipelineFeatureMask & TR_FEATURE_INDIRECT_LIGHTING) != 0u)
    {
        const float3 irradiance = TrUpsampleProbeIrradiance(
            uint2(pixel),
            depth,
            worldNormal);
        indirectRadiance = TrEvaluateIndirectDiffuseRadiance(
            baseColor,
            normalMetallic.a,
            emissiveOcclusion.a,
            irradiance,
            g_indirectLightingScale);
    }
    return float4(
        directRadiance + ambientRadiance + indirectRadiance,
        1.0f);
}
