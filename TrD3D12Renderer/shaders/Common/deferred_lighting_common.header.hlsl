#ifndef TR_DEFERRED_LIGHTING_COMMON_HEADER_HLSL
#define TR_DEFERRED_LIGHTING_COMMON_HEADER_HLSL

cbuffer SceneConstants : register(b0)
{
    float3 g_lightDirection;
    float g_lightIntensity;
    float3 g_lightColor;
    float g_ambientStrength;
};

cbuffer DeferredLightingPassConstants : register(b2)
{
    float g_directLightingScale;
    float g_ambientLightingScale;
    float g_indirectLightingScale;
    float g_relativeDepthThreshold;
    float g_minimumDepthThreshold;
    float g_normalWeightPower;
    float2 g_lightingPadding;
};

static const float TR_LIGHTING_INV_PI = 0.31830988618f;

float3 TrEvaluateDirectSurfaceRadiance(
    float3 baseColor,
    float3 worldNormal,
    float3 emissive)
{
    const float nDotL = saturate(dot(
        worldNormal,
        normalize(g_lightDirection)));
    const float directLighting =
        (1.0f - g_ambientStrength) *
        g_lightIntensity *
        g_directLightingScale *
        nDotL;
    return baseColor * g_lightColor * directLighting + emissive;
}

float3 TrEvaluateAmbientSurfaceRadiance(
    float3 baseColor,
    float ambientOcclusion)
{
    return baseColor * g_lightColor *
        (g_ambientStrength * g_ambientLightingScale * ambientOcclusion);
}

float3 TrEvaluateIndirectSurfaceRadiance(
    float3 baseColor,
    float metallic,
    float ambientOcclusion,
    float3 irradiance)
{
    const float3 diffuseAlbedo = baseColor * (1.0f - saturate(metallic));
    return diffuseAlbedo * max(irradiance, 0.0f) *
        (ambientOcclusion * g_indirectLightingScale * TR_LIGHTING_INV_PI);
}

#endif
