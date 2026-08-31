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

cbuffer SceneConstants : register(b0)
{
    float4x4 g_modelViewProjection;
    float3 g_lightDirection;
    float g_ambientStrength;
};

PSInput VSMain(VSInput input)
{
    PSInput result;
    result.position = mul(float4(input.position, 1.0f), g_modelViewProjection);
    result.worldNormal = input.normal;
    result.albedo = input.albedo;
    return result;
}

GBufferOutput PSMain(PSInput input)
{
    GBufferOutput result;
    result.baseColorRoughness = float4(saturate(input.albedo), 0.65f);
    result.normalMetallic = float4(normalize(input.worldNormal), 0.0f);
    return result;
}
