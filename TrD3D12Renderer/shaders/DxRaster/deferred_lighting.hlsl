struct FullscreenVertex
{
    float4 position : SV_POSITION;
};

cbuffer SceneConstants : register(b0)
{
    float4x4 g_modelViewProjection;
    float3 g_lightDirection;
    float g_ambientStrength;
};

Texture2D<float4> g_baseColorRoughness : register(t0);
Texture2D<float4> g_normalMetallic : register(t1);
Texture2D<float> g_depth : register(t2);

FullscreenVertex VSMain(uint vertexId : SV_VertexID)
{
    const float2 positions[3] =
    {
        float2(-1.0f, -1.0f),
        float2(-1.0f,  3.0f),
        float2( 3.0f, -1.0f)
    };

    FullscreenVertex result;
    result.position = float4(positions[vertexId], 0.0f, 1.0f);
    return result;
}

float4 PSMain(FullscreenVertex input) : SV_Target
{
    const int2 pixel = int2(input.position.xy);
    const float depth = g_depth.Load(int3(pixel, 0));
    if(depth >= 1.0f)
    {
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }

    const float3 baseColor = g_baseColorRoughness.Load(int3(pixel, 0)).rgb;
    const float3 worldNormal = normalize(g_normalMetallic.Load(int3(pixel, 0)).xyz);
    const float diffuse = saturate(dot(worldNormal, normalize(g_lightDirection)));
    const float lighting = g_ambientStrength +
        (1.0f - g_ambientStrength) * diffuse;
    return float4(baseColor * lighting, 1.0f);
}
