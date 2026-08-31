struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float3 color : COLOR;
    float2 texCoord0 : TEXCOORD0;
    float2 texCoord1 : TEXCOORD1;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float3 worldPosition : POSITION;
    float3 worldNormal : NORMAL;
    float3 color : COLOR;
    float2 texCoord0 : TEXCOORD0;
    float2 texCoord1 : TEXCOORD1;
};

struct GBufferOutput
{
    float4 baseColorRoughness : SV_Target0;
    float4 normalMetallic : SV_Target1;
    float4 emissiveOcclusion : SV_Target2;
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
    float g_gBufferPadding;
};

cbuffer PrimitiveConstants : register(b3)
{
    float4x4 g_world;
    float4x4 g_previousWorld;
    float4x4 g_worldInverseTranspose;
    float3 g_boundsCenter;
    float g_boundsRadius;
    uint g_primitiveId;
    uint g_primitiveFlags;
    float2 g_primitivePadding;
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
    uint g_drawPrimitiveIndex;
    uint g_drawMaterialIndex;
    uint g_drawInstanceOffset;
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
    return texCoord == 1u ? input.texCoord1 : input.texCoord0;
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
    float3 mappedNormal = g_normalTexture.Sample(g_normalSampler, uv).xyz * 2.0f - 1.0f;
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
    if((g_drawFlags & 1u) != 0u)
    {
        result.worldNormal = -result.worldNormal;
    }
    result.color = input.color;
    result.texCoord0 = input.texCoord0;
    result.texCoord1 = input.texCoord1;
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
    if((g_materialFlags & 4u) != 0u &&
       baseColorSample.a * g_baseColorFactor.a < g_alphaCutoff)
    {
        discard;
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
    const float3 baseColor = input.color * g_baseColorFactor.rgb *
        baseColorSample.rgb * g_baseColorScale;
    const float roughness = g_roughness * metallicRoughnessSample.g *
        g_roughnessScale;
    const float metallic = g_metallic * metallicRoughnessSample.b *
        g_metallicScale;
    const float occlusion = lerp(
        1.0f,
        occlusionSample,
        saturate(g_occlusionTextureTransform.strength));
    const float3 emissive = g_emissiveFactor * g_emissiveStrength * emissiveSample;

    GBufferOutput result;
    result.baseColorRoughness = float4(saturate(baseColor), saturate(roughness));
    result.normalMetallic = float4(worldNormal, saturate(metallic));
    result.emissiveOcclusion = float4(emissive, occlusion);
    return result;
}
