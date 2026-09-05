#ifndef TR_SURFACE_LIGHTING_HEADER_HLSL
#define TR_SURFACE_LIGHTING_HEADER_HLSL

static const float TR_LIGHTING_INV_PI = 0.31830988618f;

float3 TrEvaluateDirectDiffuseRadiance(
    float3 baseColor,
    float3 directIrradiance,
    float directLightingScale)
{
    return baseColor * max(directIrradiance, 0.0f) *
        (directLightingScale * TR_LIGHTING_INV_PI);
}

float3 TrEvaluateAmbientDiffuseRadiance(
    float3 baseColor,
    float3 ambientColor,
    float ambientStrength,
    float ambientLightingScale,
    float ambientOcclusion)
{
    return baseColor * ambientColor *
        (ambientStrength * ambientLightingScale * ambientOcclusion);
}

float3 TrEvaluateIndirectDiffuseRadiance(
    float3 baseColor,
    float metallic,
    float ambientOcclusion,
    float3 irradiance,
    float indirectLightingScale)
{
    const float3 diffuseAlbedo = baseColor * (1.0f - saturate(metallic));
    return diffuseAlbedo * max(irradiance, 0.0f) *
        (ambientOcclusion * indirectLightingScale * TR_LIGHTING_INV_PI);
}

#endif
