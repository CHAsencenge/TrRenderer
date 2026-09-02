#include "../Common/depth.header.hlsl"

Texture2D<float4> g_currentColor : register(t0);
Texture2D<float4> g_previousHistory : register(t1);
Texture2D<float4> g_velocityPreviousDepth : register(t2);
Texture2D<float> g_depth : register(t3);
RWTexture2D<float4> g_outputHistory : register(u0);
SamplerState g_linearClampSampler : register(s0);

cbuffer TaaConstants : register(b2)
{
    uint g_width;
    uint g_height;
    uint g_historyValid;
    uint g_frameNumber;
    float2 g_currentJitterNdc;
    float2 g_previousJitterNdc;
    float g_staticHistoryWeight;
    float g_motionHistoryReduction;
    float g_relativeDepthThreshold;
    float g_minimumDepthThreshold;
    float g_nearPlane;
    float g_farPlane;
    float2 g_taaPadding;
};

float2 NdcJitterToUv(float2 jitterNdc)
{
    return float2(jitterNdc.x * 0.5f, -jitterNdc.y * 0.5f);
}

[numthreads(8, 8, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint2 pixel = dispatchThreadId.xy;
    if(any(pixel >= uint2(g_width, g_height)))
    {
        return;
    }

    const float4 current = g_currentColor.Load(int3(pixel, 0));
    const float currentDepth = g_depth.Load(int3(pixel, 0));
    if(g_historyValid == 0u)
    {
        g_outputHistory[pixel] = float4(current.rgb, currentDepth);
        return;
    }

    const float4 velocityPreviousDepth =
        g_velocityPreviousDepth.Load(int3(pixel, 0));
    const bool currentIsBackground = TrIsBackgroundDepth(currentDepth);
    const float2 currentUv =
        (float2(pixel) + 0.5f) / float2(g_width, g_height);
    const float2 surfaceVelocity = currentIsBackground
        ? 0.0f
        : velocityPreviousDepth.xy;
    const float2 historyUv = currentUv - surfaceVelocity +
        NdcJitterToUv(g_previousJitterNdc - g_currentJitterNdc);
    const bool historyUvValid =
        all(historyUv >= 0.0f) && all(historyUv < 1.0f);
    if(!historyUvValid ||
       (!currentIsBackground && velocityPreviousDepth.w < 0.5f))
    {
        g_outputHistory[pixel] = float4(current.rgb, currentDepth);
        return;
    }

    const float4 history = g_previousHistory.SampleLevel(
        g_linearClampSampler,
        historyUv,
        0.0f);
    const bool historyIsBackground = TrIsBackgroundDepth(history.a);
    float depthHistoryWeight = 1.0f;
    if(currentIsBackground != historyIsBackground)
    {
        // Preserve a small amount of cross-edge history so projection jitter
        // can converge sub-pixel silhouette coverage. Neighborhood clipping
        // below prevents a long foreground trail through a revealed surface.
        depthHistoryWeight = 0.25f;
    }
    else if(!currentIsBackground)
    {
        const float expectedPreviousViewDepth = TrDeviceDepthToViewDepth(
            saturate(velocityPreviousDepth.z),
            g_nearPlane,
            g_farPlane);
        const float historyViewDepth = TrDeviceDepthToViewDepth(
            saturate(history.a),
            g_nearPlane,
            g_farPlane);
        const float depthThreshold = max(
            g_minimumDepthThreshold,
            expectedPreviousViewDepth * g_relativeDepthThreshold);
        if(abs(historyViewDepth - expectedPreviousViewDepth) > depthThreshold)
        {
            depthHistoryWeight = 0.0f;
        }
    }

    float3 neighborhoodMinimum = float3(1.0e20f, 1.0e20f, 1.0e20f);
    float3 neighborhoodMaximum = float3(-1.0e20f, -1.0e20f, -1.0e20f);
    [unroll]
    for(int y = -1; y <= 1; ++y)
    {
        [unroll]
        for(int x = -1; x <= 1; ++x)
        {
            const uint2 samplePixel = uint2(clamp(
                int2(pixel) + int2(x, y),
                int2(0, 0),
                int2(g_width - 1u, g_height - 1u)));
            const float3 sampleColor =
                g_currentColor.Load(int3(samplePixel, 0)).rgb;
            neighborhoodMinimum = min(neighborhoodMinimum, sampleColor);
            neighborhoodMaximum = max(neighborhoodMaximum, sampleColor);
        }
    }

    const float3 neighborhoodExtent =
        neighborhoodMaximum - neighborhoodMinimum;
    neighborhoodMinimum -= neighborhoodExtent * 0.05f;
    neighborhoodMaximum += neighborhoodExtent * 0.05f;
    const float3 clippedHistory = clamp(
        history.rgb,
        neighborhoodMinimum,
        neighborhoodMaximum);

    const float motionPixels = length(
        surfaceVelocity * float2(g_width, g_height));
    const float motionRejection = saturate(
        motionPixels * g_motionHistoryReduction);
    const float historyWeight = saturate(
        g_staticHistoryWeight * (1.0f - motionRejection) *
        depthHistoryWeight);
    const float3 resolvedColor = lerp(
        current.rgb,
        clippedHistory,
        historyWeight);
    g_outputHistory[pixel] = float4(resolvedColor, currentDepth);
}
