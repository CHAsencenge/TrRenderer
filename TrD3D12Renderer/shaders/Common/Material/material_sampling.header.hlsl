#ifndef TR_MATERIAL_SAMPLING_HEADER_HLSL
#define TR_MATERIAL_SAMPLING_HEADER_HLSL

#include "../ABI/material_constants.header.hlsl"
#include "normal_mapping.header.hlsl"

struct TrMaterialTexCoords
{
    float2 uv0;
    float2 uv1;
    float2 uv2;
};

struct TrMaterialCoverage
{
    float4 baseColorSample;
    float opacity;
};

struct TrMaterialSurface
{
    float3 baseColor;
    float3 worldNormal;
    float roughness;
    float metallic;
    float occlusion;
    float3 emissive;
    float opacity;
};

TrMaterialTexCoords TrCreateMaterialTexCoords(
    float2 uv0,
    float2 uv1,
    float2 uv2)
{
    TrMaterialTexCoords result;
    result.uv0 = uv0;
    result.uv1 = uv1;
    result.uv2 = uv2;
    return result;
}

float2 TrSelectTexCoord(TrMaterialTexCoords texCoords, uint texCoord)
{
    return texCoord == 2u
        ? texCoords.uv2
        : (texCoord == 1u ? texCoords.uv1 : texCoords.uv0);
}

float2 TrTransformTexCoord(
    TrMaterialTexCoords texCoords,
    TrTextureTransformConstants transform)
{
    const float2 uv = TrSelectTexCoord(texCoords, transform.texCoord) *
        transform.scale;
    float sine;
    float cosine;
    sincos(transform.rotation, sine, cosine);
    return float2(
        cosine * uv.x - sine * uv.y,
        sine * uv.x + cosine * uv.y) + transform.offset;
}

// Fast path for depth/coverage passes: only the base-color texture is read.
TrMaterialCoverage TrSampleMaterialCoverage(
    Texture2D<float4> baseColorTexture,
    SamplerState baseColorSampler,
    TrMaterialTexCoords texCoords,
    TrMaterialConstants material)
{
    TrMaterialCoverage result;
    const float2 uv = TrTransformTexCoord(
        texCoords,
        material.baseColorTexture);
    result.baseColorSample = baseColorTexture.Sample(baseColorSampler, uv);
    result.opacity = saturate(
        result.baseColorSample.a * material.baseColorFactor.a);
    return result;
}

float3 TrSampleMaterialWorldNormal(
    Texture2D<float4> normalTexture,
    SamplerState normalSampler,
    TrMaterialTexCoords texCoords,
    TrMaterialConstants material,
    float3 worldPosition,
    float3 geometricNormal)
{
    const float2 uv = TrTransformTexCoord(
        texCoords,
        material.normalTexture);
    const float3 tangentSpaceNormal =
        normalTexture.Sample(normalSampler, uv).xyz * 2.0f - 1.0f;
    return TrApplyNormalMap(
        tangentSpaceNormal,
        material.normalTexture.strength,
        worldPosition,
        geometricNormal,
        uv);
}

// Full path for passes that require the complete material surface. Coverage is
// passed in so the base-color texture is not sampled twice after alpha testing.
TrMaterialSurface TrSampleMaterialSurface(
    Texture2D<float4> metallicRoughnessTexture,
    SamplerState metallicRoughnessSampler,
    Texture2D<float4> normalTexture,
    SamplerState normalSampler,
    Texture2D<float4> occlusionTexture,
    SamplerState occlusionSampler,
    Texture2D<float4> emissiveTexture,
    SamplerState emissiveSampler,
    TrMaterialTexCoords texCoords,
    TrMaterialCoverage coverage,
    TrMaterialConstants material,
    float3 vertexColorMultiplier,
    float3 worldPosition,
    float3 geometricNormal)
{
    const float2 metallicRoughnessUv = TrTransformTexCoord(
        texCoords,
        material.metallicRoughnessTexture);
    const float2 occlusionUv = TrTransformTexCoord(
        texCoords,
        material.occlusionTexture);
    const float2 emissiveUv = TrTransformTexCoord(
        texCoords,
        material.emissiveTexture);
    const float4 metallicRoughnessSample = metallicRoughnessTexture.Sample(
        metallicRoughnessSampler,
        metallicRoughnessUv);
    const float occlusionSample = occlusionTexture.Sample(
        occlusionSampler,
        occlusionUv).r;
    const float3 emissiveSample = emissiveTexture.Sample(
        emissiveSampler,
        emissiveUv).rgb;

    TrMaterialSurface result;
    result.baseColor = vertexColorMultiplier * material.baseColorFactor.rgb *
        coverage.baseColorSample.rgb;
    result.worldNormal = TrSampleMaterialWorldNormal(
        normalTexture,
        normalSampler,
        texCoords,
        material,
        worldPosition,
        geometricNormal);
    result.roughness = material.roughness * metallicRoughnessSample.g;
    result.metallic = material.metallic * metallicRoughnessSample.b;
    result.occlusion = lerp(
        1.0f,
        occlusionSample,
        saturate(material.occlusionTexture.strength));
    result.emissive = material.emissiveFactor * material.emissiveStrength *
        emissiveSample;
    result.opacity = coverage.opacity;
    return result;
}

#endif
