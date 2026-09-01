#include "../Common/depth.header.hlsl"

struct FullscreenVertex
{
    float4 position : SV_POSITION;
};

Texture2D<float4> g_debugSource : register(t0);

cbuffer CompositePassConstants : register(b2)
{
    float g_exposure;
    float g_gamma;
    uint g_visualizationMode;
    float g_depthVisualizationRange;
    float g_nearPlane;
    float g_farPlane;
    float2 g_compositePadding;
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
    const float4 source = g_debugSource.Load(int3(pixel, 0));

    if(g_visualizationMode == 2u)
    {
        const float normalLengthSquared = dot(source.xyz, source.xyz);
        const float3 displayNormal = normalLengthSquared > 0.0001f
            ? normalize(source.xyz) * 0.5f + 0.5f
            : 0.0f;
        return float4(displayNormal, 1.0f);
    }

    if(g_visualizationMode >= 3u && g_visualizationMode <= 6u)
    {
        const uint component = g_visualizationMode - 3u;
        const float scalarValue = source[component];
        return float4(scalarValue.xxx, 1.0f);
    }

    if(g_visualizationMode == 7u)
    {
        const float deviceDepth = source.r;
        if(TrIsBackgroundDepth(deviceDepth))
        {
            return float4(0.0f, 0.0f, 0.0f, 1.0f);
        }

        const float linearDepth = TrDeviceDepthToViewDepth(
            deviceDepth,
            g_nearPlane,
            g_farPlane);
        const float normalizedDepth = saturate(
            linearDepth / max(g_depthVisualizationRange, g_nearPlane));
        return float4(normalizedDepth.xxx, 1.0f);
    }

    float3 linearColor = source.rgb;
    if(g_visualizationMode == 0u)
    {
        linearColor *= g_exposure;
    }
    const float3 displayColor = pow(
        saturate(linearColor),
        1.0f / max(g_gamma, 0.001f));
    return float4(displayColor, 1.0f);
}
