#include "screen_trace_hit.header.hlsl"
#include "../Common/deferred_lighting_common.header.hlsl"

Texture2D<uint4> g_traceHit : register(t0);
Texture2D<float4> g_baseColorRoughness : register(t1);
Texture2D<float4> g_normalMetallic : register(t2);
Texture2D<float> g_depth : register(t3);
Texture2D<float4> g_emissiveOcclusion : register(t4);
RWTexture2D<float4> g_radiance : register(u0);

[numthreads(8, 8, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint traceWidth;
    uint traceHeight;
    g_radiance.GetDimensions(traceWidth, traceHeight);
    const uint2 tracePixel = dispatchThreadId.xy;
    if(any(tracePixel >= uint2(traceWidth, traceHeight)))
    {
        return;
    }

    const uint4 encodedHit = g_traceHit.Load(int3(tracePixel, 0));
    const uint status = TrGetTraceStatus(encodedHit);
    const uint source = TrGetTraceSource(encodedHit);
    if(status != TR_SCREEN_TRACE_HIT ||
       source != TR_TRACE_SOURCE_SCREEN ||
       encodedHit.x == TR_INVALID_HIT_PIXEL)
    {
        g_radiance[tracePixel] = 0.0f;
        return;
    }

    uint sceneWidth;
    uint sceneHeight;
    g_baseColorRoughness.GetDimensions(sceneWidth, sceneHeight);
    const uint2 hitPixel = min(
        TrUnpackHitPixel(encodedHit.x),
        uint2(sceneWidth - 1u, sceneHeight - 1u));
    const float3 baseColor =
        g_baseColorRoughness.Load(int3(hitPixel, 0)).rgb;
    const float3 normal =
        g_normalMetallic.Load(int3(hitPixel, 0)).xyz;
    const float3 emissive =
        g_emissiveOcclusion.Load(int3(hitPixel, 0)).rgb;
    const float normalLengthSquared = dot(normal, normal);
    if(normalLengthSquared < 1.0e-6f)
    {
        g_radiance[tracePixel] = 0.0f;
        return;
    }

    // One-bounce bootstrap: the hit surface contributes only direct lighting
    // and emissive. Probe indirect lighting is deliberately excluded to avoid
    // same-frame recursive feedback.
    const float3 incidentRadiance = TrEvaluateDirectSurfaceRadiance(
        baseColor,
        normal * rsqrt(normalLengthSquared),
        emissive);
    g_radiance[tracePixel] = float4(
        max(incidentRadiance, 0.0f),
        TrGetTraceConfidence(encodedHit));
}
