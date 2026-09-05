#ifndef TR_MATERIAL_CONSTANTS_ABI_HEADER_HLSL
#define TR_MATERIAL_CONSTANTS_ABI_HEADER_HLSL

struct TrTextureTransformConstants
{
    float2 offset;
    float2 scale;
    float rotation;
    uint texCoord;
    float strength;
    float padding;
};

struct TrMaterialConstants
{
    float4 baseColorFactor;
    float3 emissiveFactor;
    float emissiveStrength;
    float roughness;
    float metallic;
    float alphaCutoff;
    uint flags;
    TrTextureTransformConstants baseColorTexture;
    TrTextureTransformConstants metallicRoughnessTexture;
    TrTextureTransformConstants normalTexture;
    TrTextureTransformConstants occlusionTexture;
    TrTextureTransformConstants emissiveTexture;
};

#endif
