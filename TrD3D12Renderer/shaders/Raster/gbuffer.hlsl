#include "../Common/material_flags.header.hlsl"

struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float3 color : COLOR;
    float2 texCoord0 : TEXCOORD0;
    float2 texCoord1 : TEXCOORD1;
    float2 texCoord2 : TEXCOORD2;
};

struct PSInput
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

struct TextureTransformConstants
{
    float2 offset;
    float2 scale;
    float rotation;
    uint texCoord;
    float strength;
    float padding;
};

// Register contract shared by the renderer:
// b0 Scene, b1 View, b2 Pass, b3 Primitive, b4 Material, b5 Draw.
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

cbuffer GBufferPassConstants : register(b2)
{
    float g_baseColorScale;
    float g_roughnessScale;
    float g_metallicScale;
    uint g_geometryVisualization;
};

cbuffer PrimitiveConstants : register(b3)
{
    float4x4 g_world;
    float4x4 g_previousWorld;
    float4x4 g_worldInverseTranspose;
    float3 g_boundsCenter;
    float g_boundsRadius;
    uint g_instanceId;
    uint g_meshId;
    uint g_parentNodeId;
    uint g_hierarchyDepth;
};

cbuffer MaterialConstants : register(b4)
{
    float4 g_baseColorFactor;
    float3 g_emissiveFactor;
    float g_emissiveStrength;
    float g_roughness;
    float g_metallic;
    float g_alphaCutoff;
    uint g_materialFlags;
    TextureTransformConstants g_baseColorTextureTransform;
    TextureTransformConstants g_metallicRoughnessTextureTransform;
    TextureTransformConstants g_normalTextureTransform;
    TextureTransformConstants g_occlusionTextureTransform;
    TextureTransformConstants g_emissiveTextureTransform;
};

cbuffer DrawConstants : register(b5)
{
    uint g_drawPrimitiveId;
    uint g_drawMaterialId;
    uint g_drawLocalPrimitiveIndex;
    uint g_drawFlags;
};

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

float2 SelectTexCoord(PSInput input, uint texCoord)
{
    return texCoord == 2u
        ? input.texCoord2
        : (texCoord == 1u ? input.texCoord1 : input.texCoord0);
}

float2 TransformTexCoord(PSInput input, TextureTransformConstants transform)
{
    const float2 uv = SelectTexCoord(input, transform.texCoord) * transform.scale;
    float sine;
    float cosine;
    sincos(transform.rotation, sine, cosine);
    return float2(
        cosine * uv.x - sine * uv.y,
        sine * uv.x + cosine * uv.y) + transform.offset;
}

// Nworld = normalize(nx * T + ny * B + nz * N)
float3 ApplyNormalMap(float3 worldPosition, float3 geometricNormal, float2 uv)
{
    // 切线空间法线
    float3 mappedNormal = g_normalTexture.Sample(g_normalSampler, uv).xyz * 2.0f - 1.0f;
    mappedNormal.xy *= g_normalTextureTransform.strength;

    // T = U 增大方向
    // B = V 增大方向
    // N = 几何法线方向

    // Px 为 ddx，Py 为 ddy，屏幕上发生微小移动 (dx, dy), 对应的世界位置变化 dP = Px·dx + Py·dy
    const float3 positionDx = ddx(worldPosition);
    const float3 positionDy = ddy(worldPosition);
    // dU = Ux·dx + Uy·dy
    const float2 uvDx = ddx(uv);
    const float2 uvDy = ddy(uv);
    // 目标：要找到世界空间中的两个方向，使它们能够分别表示 U 和 V 的变化
    // 构造对偶基，Ex 完全忽略 Py，只测量 Px，Ey 完全忽略 Px，只测量 Py
    // Ex · Px = 1
    // Ex · Py = 0
    // Ey · Px = 0
    // Ey · Py = 1
    const float3 positionDyPerpendicular = cross(positionDy, geometricNormal);
    const float3 positionDxPerpendicular = cross(geometricNormal, positionDx);

    const float3 tangent = positionDyPerpendicular * uvDx.x +
        positionDxPerpendicular * uvDy.x;
    const float3 bitangent = positionDyPerpendicular * uvDx.y +
        positionDxPerpendicular * uvDy.y;
    const float inverseScale = rsqrt(max(
        max(dot(tangent, tangent), dot(bitangent, bitangent)),
        1.0e-8f));
    return normalize(
        tangent * (mappedNormal.x * inverseScale) +
        bitangent * (mappedNormal.y * inverseScale) +
        geometricNormal * mappedNormal.z);
}

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

PSInput VSMain(VSInput input)
{
    PSInput result;
    const float4 worldPosition = mul(float4(input.position, 1.0f), g_world);
    const float4 currentClipPosition = mul(worldPosition, g_viewProjection);
    const float4 previousWorldPosition = mul(
        float4(input.position, 1.0f),
        g_previousWorld);
    const float4 previousClipPosition = mul(
        previousWorldPosition,
        g_previousViewProjection);
    result.position = currentClipPosition;
    result.worldPosition = worldPosition.xyz;
    result.worldNormal = mul(
        float4(input.normal, 0.0f),
        g_worldInverseTranspose).xyz;
    if((g_drawFlags & 1u) != 0u)
    {
        result.worldNormal = -result.worldNormal;
    }
    result.color = input.color;
    result.texCoord0 = input.texCoord0;
    result.texCoord1 = input.texCoord1;
    result.texCoord2 = input.texCoord2;

    // Store real surface motion without projection jitter. Resolved color
    // history uses this motion directly; raw depth history applies the jitter
    // delta separately when validating the reprojected surface.
    const float2 currentNdc =
        currentClipPosition.xy / max(currentClipPosition.w, 1.0e-6f) -
        g_temporalJitter;
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

GBufferOutput PSMain(PSInput input)
{
    const float2 baseColorUv = TransformTexCoord(input, g_baseColorTextureTransform);
    const float2 metallicRoughnessUv = TransformTexCoord(
        input,
        g_metallicRoughnessTextureTransform);
    const float2 normalUv = TransformTexCoord(input, g_normalTextureTransform);
    const float2 occlusionUv = TransformTexCoord(input, g_occlusionTextureTransform);
    const float2 emissiveUv = TransformTexCoord(input, g_emissiveTextureTransform);

    const float4 baseColorSample = g_baseColorTexture.Sample(
        g_baseColorSampler,
        baseColorUv);
    if(TrMaterialHasFlag(g_materialFlags, TR_MATERIAL_FLAG_ALPHA_BLEND))
    {
        discard;
    }
    if(TrMaterialHasFlag(g_materialFlags, TR_MATERIAL_FLAG_ALPHA_MASK))
    {
        clip(baseColorSample.a * g_baseColorFactor.a - g_alphaCutoff);
    }
    const float4 metallicRoughnessSample = g_metallicRoughnessTexture.Sample(
        g_metallicRoughnessSampler,
        metallicRoughnessUv);
    const float occlusionSample = g_occlusionTexture.Sample(
        g_occlusionSampler,
        occlusionUv).r;
    const float3 emissiveSample = g_emissiveTexture.Sample(
        g_emissiveSampler,
        emissiveUv).rgb;

    const float3 geometricNormal = normalize(input.worldNormal);
    const float3 worldNormal = ApplyNormalMap(
        input.worldPosition,
        geometricNormal,
        normalUv);
    float3 baseColor = input.color * g_baseColorFactor.rgb *
        baseColorSample.rgb * g_baseColorScale;
    float roughness = g_roughness * metallicRoughnessSample.g *
        g_roughnessScale;
    float metallic = g_metallic * metallicRoughnessSample.b *
        g_metallicScale;
    const float occlusion = lerp(
        1.0f,
        occlusionSample,
        saturate(g_occlusionTextureTransform.strength));
    float3 emissive = g_emissiveFactor * g_emissiveStrength * emissiveSample;

    if(g_geometryVisualization == 1u)
    {
        const uint parentKey = g_parentNodeId == 0xffffffffu
            ? g_instanceId
            : g_parentNodeId;
        const float3 parentColor = IdColor(parentKey);
        const float3 nodeColor = IdColor(g_instanceId + 0x9e3779b9u);
        const float depthScale = 0.72f + 0.12f * float(g_hierarchyDepth % 3u);
        baseColor = saturate(lerp(parentColor, nodeColor, 0.28f) * depthScale);
        roughness = 0.78f;
        metallic = 0.0f;
        emissive = baseColor * 0.12f;
    }
    else if(g_geometryVisualization == 2u)
    {
        const uint drawKey = HashId(
            g_drawPrimitiveId ^
            (g_instanceId * 0x9e3779b9u) ^
            (g_drawLocalPrimitiveIndex * 0x85ebca6bu));
        baseColor = IdColor(drawKey);
        roughness = 0.82f;
        metallic = 0.0f;
        emissive = baseColor * 0.12f;
    }

    GBufferOutput result;
    result.baseColorRoughness = float4(saturate(baseColor), saturate(roughness));
    result.normalMetallic = float4(worldNormal, saturate(metallic));
    result.emissiveOcclusion = float4(emissive, occlusion);
    result.velocityPreviousDepth = input.velocityPreviousDepth;
    return result;
}
