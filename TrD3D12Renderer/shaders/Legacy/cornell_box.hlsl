struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float3 albedo : COLOR;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float3 albedo : COLOR;
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
    result.normal = input.normal;
    result.albedo = input.albedo;
    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    const float diffuse = saturate(dot(normalize(input.normal), normalize(g_lightDirection)));
    const float lighting = g_ambientStrength + (1.0f - g_ambientStrength) * diffuse;
    const float3 linearColor = input.albedo * lighting;
    const float3 displayColor = pow(saturate(linearColor), 1.0f / 2.2f);
    return float4(displayColor, 1.0f);
}
