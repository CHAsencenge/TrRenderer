#include "../Common/ABI/draw_constants.header.hlsl"
#include "../Common/ABI/light_types.header.hlsl"
#include "../Common/ABI/material_constants.header.hlsl"
#include "../Common/ABI/primitive_constants.header.hlsl"
#include "../Common/ABI/scene_constants.header.hlsl"
#include "../Common/ABI/view_constants.header.hlsl"
#include "../Common/Geometry/static_mesh.header.hlsl"
#include "../Common/Lighting/light_evaluation.header.hlsl"
#include "../Common/Lighting/surface_lighting.header.hlsl"
#include "../Common/Material/material_flags.header.hlsl"
#include "../Common/Material/material_sampling.header.hlsl"

struct ForwardTransparentVertex
{
    float4 position : SV_POSITION;
    float3 worldPosition : POSITION;
    float3 worldNormal : NORMAL;
    float3 color : COLOR;
    float2 texCoord0 : TEXCOORD0;
    float2 texCoord1 : TEXCOORD1;
    float2 texCoord2 : TEXCOORD2;
};

ConstantBuffer<TrSceneConstants> g_sceneConstants : register(b0);
ConstantBuffer<TrViewConstants> g_viewConstants : register(b1);

cbuffer ForwardTransparentPassConstants : register(b2)
{
    float g_directLightingScale;
    float g_ambientLightingScale;
    float2 g_transparentPadding;
};

ConstantBuffer<TrPrimitiveConstants> g_primitiveConstants : register(b3);
ConstantBuffer<TrMaterialConstants> g_materialConstants : register(b4);
ConstantBuffer<TrDrawConstants> g_drawConstants : register(b5);

Texture2D<float4> g_baseColorTexture : register(t0);
Texture2D<float4> g_metallicRoughnessTexture : register(t1);
Texture2D<float4> g_normalTexture : register(t2);
Texture2D<float4> g_occlusionTexture : register(t3);
Texture2D<float4> g_emissiveTexture : register(t4);
StructuredBuffer<TrGpuLight> g_lights : register(t6);

SamplerState g_baseColorSampler : register(s0);
SamplerState g_metallicRoughnessSampler : register(s1);
SamplerState g_normalSampler : register(s2);
SamplerState g_occlusionSampler : register(s3);
SamplerState g_emissiveSampler : register(s4);

ForwardTransparentVertex VSMain(TrStaticMeshVertexInput input)
{
    ForwardTransparentVertex result;
    const float4 worldPosition = TrTransformLocalPosition(
        input.position,
        g_primitiveConstants.world);
    result.position = mul(worldPosition, g_viewConstants.viewProjection);
    result.worldPosition = worldPosition.xyz;
    result.worldNormal = TrTransformLocalNormal(
        input.normal,
        g_primitiveConstants.worldInverseTranspose,
        false);
    result.color = input.color;
    result.texCoord0 = input.texCoord0;
    result.texCoord1 = input.texCoord1;
    result.texCoord2 = input.texCoord2;
    return result;
}

float4 PSMain(ForwardTransparentVertex input) : SV_Target
{
    if(!TrMaterialHasFlag(
           g_materialConstants.flags,
           TR_MATERIAL_FLAG_ALPHA_BLEND))
    {
        discard;
    }

    const TrMaterialTexCoords texCoords = TrCreateMaterialTexCoords(
        input.texCoord0,
        input.texCoord1,
        input.texCoord2);
    const TrMaterialCoverage coverage = TrSampleMaterialCoverage(
        g_baseColorTexture,
        g_baseColorSampler,
        texCoords,
        g_materialConstants);
    if(TrMaterialHasFlag(
           g_materialConstants.flags,
           TR_MATERIAL_FLAG_ALPHA_MASK))
    {
        clip(coverage.opacity - g_materialConstants.alphaCutoff);
    }
    clip(coverage.opacity - (1.0f / 255.0f));

    const TrMaterialSurface surface = TrSampleMaterialSurface(
        g_metallicRoughnessTexture,
        g_metallicRoughnessSampler,
        g_normalTexture,
        g_normalSampler,
        g_occlusionTexture,
        g_occlusionSampler,
        g_emissiveTexture,
        g_emissiveSampler,
        texCoords,
        coverage,
        g_materialConstants,
        input.color,
        input.worldPosition,
        normalize(input.worldNormal));

    float3 color;
    if(TrMaterialHasFlag(
           g_materialConstants.flags,
           TR_MATERIAL_FLAG_UNLIT))
    {
        color = surface.baseColor + surface.emissive;
    }
    else
    {
        const float3 directIrradiance = TrEvaluateDirectIrradiance(
            g_lights,
            g_sceneConstants.lightCount,
            input.worldPosition,
            surface.worldNormal);
        const float3 directRadiance = TrEvaluateDirectDiffuseRadiance(
            surface.baseColor,
            directIrradiance,
            g_directLightingScale);
        const float3 ambientRadiance = TrEvaluateAmbientDiffuseRadiance(
            surface.baseColor,
            g_sceneConstants.ambientColor,
            g_sceneConstants.ambientStrength,
            g_ambientLightingScale,
            surface.occlusion);
        color = directRadiance + ambientRadiance + surface.emissive;
    }

    return float4(color, surface.opacity);
}
