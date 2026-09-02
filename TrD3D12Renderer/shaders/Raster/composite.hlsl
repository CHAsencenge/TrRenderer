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
    float2 g_outputSize;
    float2 g_outputPadding;
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
    uint sourceWidth;
    uint sourceHeight;
    g_debugSource.GetDimensions(sourceWidth, sourceHeight);
    const float2 screenUv = input.position.xy / max(
        g_outputSize,
        float2(1.0f, 1.0f));
    const uint2 pixel = min(
        uint2(screenUv * float2(sourceWidth, sourceHeight)),
        uint2(sourceWidth - 1u, sourceHeight - 1u));
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

    if(g_visualizationMode == 8u)
    {
        const uint traceStatus = uint(round(source.a));
        if(traceStatus == 1u)
        {
            // A successful Screen Trace is green. Distance darkens long rays,
            // while hit UV adds enough variation to expose stuck coordinates.
            const float distanceVisibility = 1.0f - source.b * 0.65f;
            const float3 hitColor = lerp(
                float3(0.05f, 0.65f, 0.12f),
                float3(source.r, 1.0f, source.g),
                0.35f);
            return float4(hitColor * distanceVisibility, 1.0f);
        }
        if(traceStatus == 2u)
        {
            return float4(0.85f, 0.12f, 0.08f, 1.0f);
        }
        if(traceStatus == 3u)
        {
            return float4(0.08f, 0.25f, 0.95f, 1.0f);
        }
        if(traceStatus == 4u)
        {
            return float4(0.95f, 0.72f, 0.08f, 1.0f);
        }
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }

    if(g_visualizationMode == 9u)
    {
        const float2 velocityPixels = source.xy * g_outputSize;
        const float motionVisibility = saturate(length(velocityPixels) / 16.0f);
        const float2 direction = velocityPixels / max(
            length(velocityPixels),
            1.0e-5f);
        return float4(
            0.5f + direction.x * 0.5f * motionVisibility,
            0.5f - direction.y * 0.5f * motionVisibility,
            motionVisibility,
            1.0f);
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
