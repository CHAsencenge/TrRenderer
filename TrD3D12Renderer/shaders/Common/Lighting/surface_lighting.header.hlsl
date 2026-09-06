#ifndef TR_SURFACE_LIGHTING_HEADER_HLSL
#define TR_SURFACE_LIGHTING_HEADER_HLSL

#include "light_evaluation.header.hlsl"
#include "pbr_brdf.header.hlsl"

// Keep these values synchronized with TrLightingVisualization in
// Source/Renderer/TrRenderConstants.h.
static const uint TR_LIGHTING_VISUALIZATION_COMBINED = 0u;
static const uint TR_LIGHTING_VISUALIZATION_INDIRECT = 1u;
static const uint TR_LIGHTING_VISUALIZATION_DIRECT_SPECULAR = 2u;
static const uint TR_LIGHTING_VISUALIZATION_DIRECT_DIFFUSE = 3u;
static const uint TR_LIGHTING_VISUALIZATION_CONSTANT_AMBIENT = 4u;
static const uint TR_LIGHTING_VISUALIZATION_SCREEN_PROBE_DIFFUSE = 5u;

struct TrDirectPbrRadiance
{
    float3 diffuse;
    float3 specular;
};

float3 TrEvaluateDirectDiffuseRadiance(
    float3 baseColor,
    float metallic,
    float3 directIrradiance,
    float directLightingScale)
{
    // Lambertian diffuse for the non-metallic part of the material:
    // L_o = E_direct * baseColor * (1 - metallic) / PI.
    const float3 diffuseAlbedo = saturate(baseColor) *
        (1.0f - saturate(metallic));
    return diffuseAlbedo * max(directIrradiance, 0.0f) *
        (directLightingScale * TR_PBR_INV_PI);
}

TrDirectPbrRadiance TrEvaluateDirectPbrRadiance(
    StructuredBuffer<TrGpuLight> lights,
    uint lightCount,
    float3 worldPosition,
    float3 worldNormal,
    float3 directionToView,
    float3 baseColor,
    float metallic,
    float perceptualRoughness,
    float directLightingScale)
{
    // Direct-light rendering equation evaluated as a discrete sum:
    // L_o(V) = sum_i f_r(V, L_i) * L_i * max(N dot L_i, 0).
    TrDirectPbrRadiance result = (TrDirectPbrRadiance)0;
    [loop]
    for(uint lightIndex = 0u; lightIndex < lightCount; ++lightIndex)
    {
        const TrLightSample lightSample = TrEvaluateLightSample(
            lights[lightIndex],
            worldPosition);
        const float normalDotLight = saturate(dot(
            worldNormal,
            lightSample.directionToLight));
        if(normalDotLight <= 0.0f)
        {
            continue;
        }

        const TrPbrBrdfTerms brdf = TrEvaluateMetallicRoughnessBrdfTerms(
            baseColor,
            metallic,
            perceptualRoughness,
            worldNormal,
            directionToView,
            lightSample.directionToLight);
        const float3 incidentContribution =
            lightSample.radiance * normalDotLight;
        // Keep the BRDF lobes separate so visualization modes report actual
        // outgoing radiance from each lobe, after light color and N dot L.
        result.diffuse += brdf.diffuse * incidentContribution;
        result.specular += brdf.specular * incidentContribution;
    }
    result.diffuse = max(result.diffuse * directLightingScale, 0.0f);
    result.specular = max(result.specular * directLightingScale, 0.0f);
    return result;
}

float3 TrEvaluateAmbientDiffuseRadiance(
    float3 baseColor,
    float metallic,
    float3 ambientColor,
    float ambientStrength,
    float ambientLightingScale,
    float ambientOcclusion)
{
    // Constant ambient approximation. AO only attenuates this indirect term:
    // L_ambient = baseColor * (1 - metallic) * ambientColor * strength * AO.
    const float3 diffuseAlbedo = saturate(baseColor) *
        (1.0f - saturate(metallic));
    return diffuseAlbedo * ambientColor *
        (ambientStrength * ambientLightingScale * ambientOcclusion);
}

float3 TrEvaluateIndirectDiffuseRadiance(
    float3 baseColor,
    float metallic,
    float ambientOcclusion,
    float3 irradiance,
    float indirectLightingScale)
{
    // Diffuse response to probe irradiance:
    // L_indirect = baseColor * (1 - metallic) / PI * E_probe * AO.
    const float3 diffuseAlbedo = baseColor * (1.0f - saturate(metallic));
    return diffuseAlbedo * max(irradiance, 0.0f) *
        (ambientOcclusion * indirectLightingScale * TR_PBR_INV_PI);
}

float3 TrResolveLightingVisualization(
    TrDirectPbrRadiance directRadiance,
    float3 ambientRadiance,
    float3 screenProbeDiffuseRadiance,
    float3 emissiveRadiance,
    uint visualization)
{
    if(visualization == TR_LIGHTING_VISUALIZATION_INDIRECT)
    {
        // Total indirect lighting includes both the constant approximation and
        // Screen Probe diffuse. Self-emission is not incoming light.
        return max(ambientRadiance + screenProbeDiffuseRadiance, 0.0f);
    }
    if(visualization == TR_LIGHTING_VISUALIZATION_CONSTANT_AMBIENT)
    {
        return max(ambientRadiance, 0.0f);
    }
    if(visualization == TR_LIGHTING_VISUALIZATION_SCREEN_PROBE_DIFFUSE)
    {
        return max(screenProbeDiffuseRadiance, 0.0f);
    }
    if(visualization == TR_LIGHTING_VISUALIZATION_DIRECT_DIFFUSE)
    {
        return max(directRadiance.diffuse, 0.0f);
    }
    if(visualization == TR_LIGHTING_VISUALIZATION_DIRECT_SPECULAR)
    {
        return max(directRadiance.specular, 0.0f);
    }
    return max(
        directRadiance.diffuse + directRadiance.specular +
            ambientRadiance + screenProbeDiffuseRadiance + emissiveRadiance,
        0.0f);
}

#endif
