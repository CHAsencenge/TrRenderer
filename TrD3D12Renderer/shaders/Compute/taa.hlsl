#include "../Common/Utility/depth.header.hlsl"

Texture2D<float4> g_currentColor : register(t0);
Texture2D<float4> g_previousColorHistory : register(t1);
Texture2D<float4> g_velocityPreviousDepth : register(t2);
Texture2D<float> g_depth : register(t3);
Texture2D<float> g_previousDepthHistory : register(t4);
RWTexture2D<float4> g_outputColorHistory : register(u0);
RWTexture2D<float> g_outputDepthHistory : register(u1);
SamplerState g_linearClampSampler : register(s0);
SamplerState g_pointClampSampler : register(s1);

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
        g_outputColorHistory[pixel] = float4(current.rgb, 1.0f);
        g_outputDepthHistory[pixel] = currentDepth;
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
    // The resolved color history lives on the stable output pixel grid, so it
    // is reprojected only by non-jittered surface motion. The depth history is
    // copied from the previous frame's jittered depth buffer and therefore
    // needs the previous-to-current jitter offset to address the same surface.
    const float2 colorHistoryUv = currentUv - surfaceVelocity;
    const float2 depthHistoryUv = colorHistoryUv +
        NdcJitterToUv(g_previousJitterNdc - g_currentJitterNdc);
    const bool colorHistoryUvValid =
        all(colorHistoryUv >= 0.0f) && all(colorHistoryUv < 1.0f);
    const bool depthHistoryUvValid =
        all(depthHistoryUv >= 0.0f) && all(depthHistoryUv < 1.0f);
    if(!colorHistoryUvValid || !depthHistoryUvValid ||
       (!currentIsBackground && velocityPreviousDepth.w < 0.5f))
    {
        g_outputColorHistory[pixel] = float4(current.rgb, 1.0f);
        g_outputDepthHistory[pixel] = currentDepth;
        return;
    }

    const float3 historyColor = g_previousColorHistory.SampleLevel(
        g_linearClampSampler,
        colorHistoryUv,
        0.0f).rgb;
    // Depth represents geometry identity, not a continuously filterable color.
    // Point sampling prevents foreground and background device depths from being
    // blended together at silhouettes before the history rejection test.
    const float historyDepth = g_previousDepthHistory.SampleLevel(
        g_pointClampSampler,
        depthHistoryUv,
        0.0f);
    const bool historyIsBackground = TrIsBackgroundDepth(historyDepth);
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
            saturate(historyDepth),
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
        historyColor,
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
    g_outputColorHistory[pixel] = float4(resolvedColor, 1.0f);
    g_outputDepthHistory[pixel] = currentDepth;
}
