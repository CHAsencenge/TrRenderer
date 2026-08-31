struct FullscreenVertex
{
    float4 position : SV_POSITION;
};

Texture2D<float4> g_hdrLighting : register(t0);

cbuffer CompositePassConstants : register(b2)
{
    float g_exposure;
    float g_gamma;
    uint g_debugView;
    float g_compositePadding;
};

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
    const float3 linearColor =
        g_hdrLighting.Load(int3(pixel, 0)).rgb * g_exposure;
    const float3 displayColor = pow(
        saturate(linearColor),
        1.0f / max(g_gamma, 0.001f));
    return float4(displayColor, 1.0f);
}
