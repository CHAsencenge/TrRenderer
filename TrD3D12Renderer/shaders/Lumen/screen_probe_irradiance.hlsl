Texture2D<float4> g_radiance : register(t0);
RWTexture2D<float4> g_irradiance : register(u0);

cbuffer ScreenProbeIrradianceConstants : register(b2)
{
    uint g_probeCountX;
    uint g_probeCountY;
    uint g_rayGridDimension;
    uint g_raysPerProbe;
};

static const float TR_PI = 3.14159265359f;

[numthreads(8, 8, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint2 probeCoordinate = dispatchThreadId.xy;
    if(any(probeCoordinate >= uint2(g_probeCountX, g_probeCountY)))
    {
        return;
    }

    float3 weightedRadiance = 0.0f;
    float confidenceSum = 0.0f;
    [loop]
    for(uint rayIndex = 0u; rayIndex < g_raysPerProbe; ++rayIndex)
    {
        const uint2 rayCoordinate = uint2(
            rayIndex % g_rayGridDimension,
            rayIndex / g_rayGridDimension);
        const uint2 tracePixel =
            probeCoordinate * g_rayGridDimension + rayCoordinate;
        const float4 radiance = g_radiance.Load(int3(tracePixel, 0));
        const float confidence = saturate(radiance.a);
        weightedRadiance += max(radiance.rgb, 0.0f) * confidence;
        confidenceSum += confidence;
    }

    // Probe rays use a cosine-weighted hemisphere. Its Monte Carlo estimator
    // for diffuse irradiance is PI / N * sum(Li). Missing or uncertain screen
    // hits contribute zero until a later fallback pass fills them.
    const float inverseRayCount = 1.0f / max(float(g_raysPerProbe), 1.0f);
    g_irradiance[probeCoordinate] = float4(
        weightedRadiance * (TR_PI * inverseRayCount),
        confidenceSum * inverseRayCount);
}
