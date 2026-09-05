#ifndef TR_FULLSCREEN_TRIANGLE_HEADER_HLSL
#define TR_FULLSCREEN_TRIANGLE_HEADER_HLSL

struct TrFullscreenVertex
{
    float4 position : SV_POSITION;
};

TrFullscreenVertex TrCreateFullscreenTriangleVertex(uint vertexId)
{
    const float2 positions[3] =
    {
        float2(-1.0f, -1.0f),
        float2(-1.0f,  3.0f),
        float2( 3.0f, -1.0f)
    };

    TrFullscreenVertex result;
    result.position = float4(positions[vertexId], 0.0f, 1.0f);
    return result;
}

#endif
