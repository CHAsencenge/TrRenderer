#include "../Common/ABI/draw_constants.header.hlsl"
#include "../Common/ABI/material_constants.header.hlsl"
#include "../Common/ABI/primitive_constants.header.hlsl"
#include "../Common/ABI/view_constants.header.hlsl"
#include "../Common/Geometry/static_mesh.header.hlsl"
#include "../Common/Material/material_flags.header.hlsl"
#include "../Common/Material/material_sampling.header.hlsl"

struct DepthNormalVertex
{
    float4 position : SV_POSITION;
    float3 worldPosition : POSITION;
    float3 worldNormal : NORMAL;
    float2 texCoord0 : TEXCOORD0;
    float2 texCoord1 : TEXCOORD1;
    float2 texCoord2 : TEXCOORD2;
};

ConstantBuffer<TrViewConstants> g_viewConstants : register(b1);
ConstantBuffer<TrPrimitiveConstants> g_primitiveConstants : register(b3);
ConstantBuffer<TrMaterialConstants> g_materialConstants : register(b4);
ConstantBuffer<TrDrawConstants> g_drawConstants : register(b5);

Texture2D<float4> g_baseColorTexture : register(t0);
Texture2D<float4> g_metallicRoughnessTexture : register(t1);
Texture2D<float4> g_normalTexture : register(t2);
Texture2D<float4> g_occlusionTexture : register(t3);
Texture2D<float4> g_emissiveTexture : register(t4);

SamplerState g_baseColorSampler : register(s0);
SamplerState g_metallicRoughnessSampler : register(s1);
SamplerState g_normalSampler : register(s2);
SamplerState g_occlusionSampler : register(s3);
SamplerState g_emissiveSampler : register(s4);

DepthNormalVertex VSMain(TrStaticMeshVertexInput input)
{
    DepthNormalVertex result;
    const float4 worldPosition = TrTransformLocalPosition(
        input.position,
        g_primitiveConstants.world);
    result.position = mul(worldPosition, g_viewConstants.viewProjection);
    result.worldPosition = worldPosition.xyz;
    result.worldNormal = TrTransformLocalNormal(
        input.normal,
        g_primitiveConstants.worldInverseTranspose,
        (g_drawConstants.flags & 1u) != 0u);
    result.texCoord0 = input.texCoord0;
    result.texCoord1 = input.texCoord1;
    result.texCoord2 = input.texCoord2;
    return result;
}

float4 PSMain(DepthNormalVertex input) : SV_Target
{
    if(TrMaterialHasFlag(
           g_materialConstants.flags,
           TR_MATERIAL_FLAG_ALPHA_BLEND))
    {
        discard;
    }

    const TrMaterialTexCoords texCoords = TrCreateMaterialTexCoords(
        input.texCoord0,
        input.texCoord1,
        input.texCoord2);
    if(TrMaterialHasFlag(
           g_materialConstants.flags,
           TR_MATERIAL_FLAG_ALPHA_MASK))
    {
        const TrMaterialCoverage coverage = TrSampleMaterialCoverage(
            g_baseColorTexture,
            g_baseColorSampler,
            texCoords,
            g_materialConstants);
        clip(coverage.opacity - g_materialConstants.alphaCutoff);
    }

    const float3 worldNormal = TrSampleMaterialWorldNormal(
        g_normalTexture,
        g_normalSampler,
        texCoords,
        g_materialConstants,
        input.worldPosition,
        normalize(input.worldNormal));
    return float4(worldNormal, 0.0f);
}
