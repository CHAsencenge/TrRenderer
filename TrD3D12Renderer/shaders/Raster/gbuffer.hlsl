#include "../Common/ABI/draw_constants.header.hlsl"
#include "../Common/ABI/material_constants.header.hlsl"
#include "../Common/ABI/primitive_constants.header.hlsl"
#include "../Common/ABI/view_constants.header.hlsl"
#include "../Common/Geometry/static_mesh.header.hlsl"
#include "../Common/Material/material_flags.header.hlsl"
#include "../Common/Material/material_sampling.header.hlsl"

struct GBufferVertex
{
    float4 position : SV_POSITION;
    float3 worldPosition : POSITION;
    float3 worldNormal : NORMAL;
    float3 color : COLOR;
    float2 texCoord0 : TEXCOORD0;
    float2 texCoord1 : TEXCOORD1;
    float2 texCoord2 : TEXCOORD2;
    noperspective float4 velocityPreviousDepth : TEXCOORD3;
};

struct GBufferOutput
{
    float4 baseColorRoughness : SV_Target0;
    float4 normalMetallic : SV_Target1;
    float4 emissiveOcclusion : SV_Target2;
    float4 velocityPreviousDepth : SV_Target3;
};

ConstantBuffer<TrViewConstants> g_viewConstants : register(b1);

cbuffer GBufferPassConstants : register(b2)
{
    float g_baseColorScale;
    float g_roughnessScale;
    float g_metallicScale;
    uint g_geometryVisualization;
};

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

uint HashId(uint value)
{
    value ^= value >> 16u;
    value *= 0x7feb352du;
    value ^= value >> 15u;
    value *= 0x846ca68bu;
    value ^= value >> 16u;
    return value;
}

float3 IdColor(uint value)
{
    const float hue = frac(float(HashId(value) & 0xffffu) / 65535.0f);
    const float3 hueOffsets = float3(0.0f, 2.0f / 3.0f, 1.0f / 3.0f);
    const float3 rgb = saturate(
        abs(frac(hue + hueOffsets) * 6.0f - 3.0f) - 1.0f);
    return lerp(0.18f.xxx, rgb, 0.82f);
}

GBufferVertex VSMain(TrStaticMeshVertexInput input)
{
    GBufferVertex result;
    const float4 worldPosition = TrTransformLocalPosition(
        input.position,
        g_primitiveConstants.world);
    const float4 currentClipPosition = mul(
        worldPosition,
        g_viewConstants.viewProjection);
    const float4 previousWorldPosition = TrTransformLocalPosition(
        input.position,
        g_primitiveConstants.previousWorld);
    const float4 previousClipPosition = mul(
        previousWorldPosition,
        g_viewConstants.previousViewProjection);
    result.position = currentClipPosition;
    result.worldPosition = worldPosition.xyz;
    result.worldNormal = TrTransformLocalNormal(
        input.normal,
        g_primitiveConstants.worldInverseTranspose,
        (g_drawConstants.flags & 1u) != 0u);
    result.color = input.color;
    result.texCoord0 = input.texCoord0;
    result.texCoord1 = input.texCoord1;
    result.texCoord2 = input.texCoord2;

    // Store real surface motion without projection jitter. Resolved color
    // history uses this motion directly; raw depth history applies the jitter
    // delta separately when validating the reprojected surface.
    const float2 currentNdc =
        currentClipPosition.xy / max(currentClipPosition.w, 1.0e-6f) -
        g_viewConstants.temporalJitter;
    const float previousW = max(previousClipPosition.w, 1.0e-6f);
    const float3 previousNdc = previousClipPosition.xyz / previousW;
    const float2 currentUv = float2(
        currentNdc.x * 0.5f + 0.5f,
        0.5f - currentNdc.y * 0.5f);
    const float2 previousUv = float2(
        previousNdc.x * 0.5f + 0.5f,
        0.5f - previousNdc.y * 0.5f);
    const float previousPositionValid =
        previousClipPosition.w > 1.0e-6f &&
        previousNdc.z >= 0.0f && previousNdc.z <= 1.0f
            ? 1.0f
            : 0.0f;
    result.velocityPreviousDepth = float4(
        currentUv - previousUv,
        previousNdc.z,
        previousPositionValid);
    return result;
}

GBufferOutput PSMain(GBufferVertex input)
{
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
           TR_MATERIAL_FLAG_ALPHA_BLEND))
    {
        discard;
    }
    if(TrMaterialHasFlag(
           g_materialConstants.flags,
           TR_MATERIAL_FLAG_ALPHA_MASK))
    {
        clip(coverage.opacity - g_materialConstants.alphaCutoff);
    }

    // GBuffer intentionally ignores vertex color until a material-level
    // UseVertexColorAsBaseColor contract is introduced.
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
        1.0f.xxx,
        input.worldPosition,
        normalize(input.worldNormal));

    float3 baseColor = surface.baseColor * g_baseColorScale;
    float roughness = surface.roughness * g_roughnessScale;
    float metallic = surface.metallic * g_metallicScale;
    float3 emissive = surface.emissive;

    if(g_geometryVisualization == 1u)
    {
        const uint parentKey = g_primitiveConstants.parentNodeId == 0xffffffffu
            ? g_primitiveConstants.instanceId
            : g_primitiveConstants.parentNodeId;
        const float3 parentColor = IdColor(parentKey);
        const float3 nodeColor = IdColor(
            g_primitiveConstants.instanceId + 0x9e3779b9u);
        const float depthScale = 0.72f + 0.12f *
            float(g_primitiveConstants.hierarchyDepth % 3u);
        baseColor = saturate(lerp(parentColor, nodeColor, 0.28f) * depthScale);
        roughness = 0.78f;
        metallic = 0.0f;
        emissive = baseColor * 0.12f;
    }
    else if(g_geometryVisualization == 2u)
    {
        const uint drawKey = HashId(
            g_drawConstants.primitiveId ^
            (g_primitiveConstants.instanceId * 0x9e3779b9u) ^
            (g_drawConstants.localPrimitiveIndex * 0x85ebca6bu));
        baseColor = IdColor(drawKey);
        roughness = 0.82f;
        metallic = 0.0f;
        emissive = baseColor * 0.12f;
    }

    GBufferOutput result;
    result.baseColorRoughness = float4(saturate(baseColor), saturate(roughness));
    result.normalMetallic = float4(surface.worldNormal, saturate(metallic));
    result.emissiveOcclusion = float4(emissive, surface.occlusion);
    result.velocityPreviousDepth = input.velocityPreviousDepth;
    return result;
}
