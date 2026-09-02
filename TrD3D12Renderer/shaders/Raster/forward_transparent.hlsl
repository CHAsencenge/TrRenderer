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

cbuffer SceneConstants : register(b0)
{
    float3 g_lightDirection;
    float g_lightIntensity;
    float3 g_lightColor;
    float g_ambientStrength;
};

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

cbuffer ForwardTransparentPassConstants : register(b2)
{
    float g_directLightingScale;
    float g_ambientLightingScale;
    float2 g_transparentPadding;
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

float3 ApplyNormalMap(float3 worldPosition, float3 geometricNormal, float2 uv)
{
    float3 mappedNormal =
        g_normalTexture.Sample(g_normalSampler, uv).xyz * 2.0f - 1.0f;
    mappedNormal.xy *= g_normalTextureTransform.strength;

    const float3 positionDx = ddx(worldPosition);
    const float3 positionDy = ddy(worldPosition);
    const float2 uvDx = ddx(uv);
    const float2 uvDy = ddy(uv);
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

PSInput VSMain(VSInput input)
{
    PSInput result;
    const float4 worldPosition = mul(float4(input.position, 1.0f), g_world);
    result.position = mul(worldPosition, g_viewProjection);
    result.worldPosition = worldPosition.xyz;
    result.worldNormal = mul(
        float4(input.normal, 0.0f),
        g_worldInverseTranspose).xyz;
    result.color = input.color;
    result.texCoord0 = input.texCoord0;
    result.texCoord1 = input.texCoord1;
    result.texCoord2 = input.texCoord2;
    return result;
}

float4 PSMain(PSInput input) : SV_Target
{
    const float2 baseColorUv = TransformTexCoord(
        input,
        g_baseColorTextureTransform);
    const float2 normalUv = TransformTexCoord(
        input,
        g_normalTextureTransform);
    const float2 occlusionUv = TransformTexCoord(
        input,
        g_occlusionTextureTransform);
    const float2 emissiveUv = TransformTexCoord(
        input,
        g_emissiveTextureTransform);

    const float4 baseColorSample = g_baseColorTexture.Sample(
        g_baseColorSampler,
        baseColorUv);
    if(!TrMaterialHasFlag(g_materialFlags, TR_MATERIAL_FLAG_ALPHA_BLEND))
    {
        discard;
    }
    const float alpha = saturate(baseColorSample.a * g_baseColorFactor.a);
    if(TrMaterialHasFlag(g_materialFlags, TR_MATERIAL_FLAG_ALPHA_MASK))
    {
        clip(alpha - g_alphaCutoff);
    }
    clip(alpha - (1.0f / 255.0f));

    const float3 baseColor =
        input.color * g_baseColorFactor.rgb * baseColorSample.rgb;
    const float3 geometricNormal = normalize(input.worldNormal);
    const float3 worldNormal = ApplyNormalMap(
        input.worldPosition,
        geometricNormal,
        normalUv);
    const float occlusionSample = g_occlusionTexture.Sample(
        g_occlusionSampler,
        occlusionUv).r;
    const float occlusion = lerp(
        1.0f,
        occlusionSample,
        saturate(g_occlusionTextureTransform.strength));
    const float3 emissive = g_emissiveFactor * g_emissiveStrength *
        g_emissiveTexture.Sample(g_emissiveSampler, emissiveUv).rgb;

    float3 color;
    if(TrMaterialHasFlag(g_materialFlags, TR_MATERIAL_FLAG_UNLIT))
    {
        color = baseColor + emissive;
    }
    else
    {
        const float diffuse = saturate(dot(
            worldNormal,
            normalize(g_lightDirection)));
        const float lighting = g_ambientStrength *
            g_ambientLightingScale * occlusion +
            (1.0f - g_ambientStrength) * g_lightIntensity *
            g_directLightingScale * diffuse;
        color = baseColor * g_lightColor * lighting + emissive;
    }

    return float4(color, alpha);
}
