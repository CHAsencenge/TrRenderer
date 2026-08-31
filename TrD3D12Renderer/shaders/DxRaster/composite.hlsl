struct FullscreenVertex
{
    float4 position : SV_POSITION;
};

Texture2D<float4> g_hdrLighting : register(t0);

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
    const float3 linearColor = g_hdrLighting.Load(int3(pixel, 0)).rgb;
    const float3 displayColor = pow(saturate(linearColor), 1.0f / 2.2f);
    return float4(displayColor, 1.0f);
}
