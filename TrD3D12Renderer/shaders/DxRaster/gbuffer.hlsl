struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float3 albedo : COLOR;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float3 worldNormal : NORMAL;
    float3 albedo : COLOR;
};

struct GBufferOutput
{
    float4 baseColorRoughness : SV_Target0;
    float4 normalMetallic : SV_Target1;
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
    float3 g_baseColorFactor;
    float g_roughness;
    float g_metallic;
    float g_emissiveStrength;
    float2 g_materialPadding;
};

cbuffer DrawConstants : register(b5)
{
    uint g_drawPrimitiveIndex;
    uint g_drawMaterialIndex;
    uint g_drawInstanceOffset;
    uint g_drawFlags;
};

PSInput VSMain(VSInput input)
{
    PSInput result;
    const float4 worldPosition = mul(float4(input.position, 1.0f), g_world);
    result.position = mul(worldPosition, g_viewProjection);
    result.worldNormal = mul(
        float4(input.normal, 0.0f),
        g_worldInverseTranspose).xyz;
    if((g_drawFlags & 1u) != 0u)
    {
        result.worldNormal = -result.worldNormal;
    }
    result.albedo = input.albedo;
    return result;
}

GBufferOutput PSMain(PSInput input)
{
    GBufferOutput result;
    result.baseColorRoughness = float4(
        saturate(input.albedo * g_baseColorFactor * g_baseColorScale),
        saturate(g_roughness * g_roughnessScale));
    result.normalMetallic = float4(
        normalize(input.worldNormal),
        saturate(g_metallic * g_metallicScale));
    return result;
}
