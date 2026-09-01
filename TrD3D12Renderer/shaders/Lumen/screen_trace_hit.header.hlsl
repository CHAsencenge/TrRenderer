#ifndef TR_SCREEN_TRACE_HIT_HEADER_HLSL
#define TR_SCREEN_TRACE_HIT_HEADER_HLSL

// Keep these values synchronized with TrScreenTraceHit.h.
static const uint TR_SCREEN_TRACE_INVALID_PROBE = 0u;
static const uint TR_SCREEN_TRACE_HIT = 1u;
static const uint TR_SCREEN_TRACE_MISS_DISTANCE = 2u;
static const uint TR_SCREEN_TRACE_MISS_SCREEN = 3u;
static const uint TR_SCREEN_TRACE_MISS_ITERATIONS = 4u;

static const uint TR_TRACE_SOURCE_NONE = 0u;
static const uint TR_TRACE_SOURCE_SCREEN = 1u;
static const uint TR_TRACE_SOURCE_RAY_QUERY = 2u;
static const uint TR_TRACE_SOURCE_SOFTWARE_RAY_TRACING = 3u;

static const uint TR_INVALID_HIT_PIXEL = 0xffffffffu;
static const uint TR_TRACE_STATUS_SHIFT = 0u;
static const uint TR_TRACE_STATUS_MASK = 0x7u;
static const uint TR_TRACE_SOURCE_SHIFT = 3u;
static const uint TR_TRACE_SOURCE_MASK = 0x3u;
static const uint TR_TRACE_FRONT_FACE_SHIFT = 5u;
static const uint TR_TRACE_LAST_MIP_SHIFT = 6u;
static const uint TR_TRACE_LAST_MIP_MASK = 0x1fu;
static const uint TR_TRACE_ITERATION_SHIFT = 11u;
static const uint TR_TRACE_ITERATION_MASK = 0xffu;

uint TrPackHitPixel(uint2 pixel)
{
    return (pixel.x & 0xffffu) | ((pixel.y & 0xffffu) << 16u);
}

uint2 TrUnpackHitPixel(uint packedPixel)
{
    return uint2(packedPixel & 0xffffu, packedPixel >> 16u);
}

uint TrPackTraceFlags(
    uint status,
    uint source,
    uint lastMip,
    uint iterationCount,
    bool frontFace)
{
    return
        ((status & TR_TRACE_STATUS_MASK) << TR_TRACE_STATUS_SHIFT) |
        ((source & TR_TRACE_SOURCE_MASK) << TR_TRACE_SOURCE_SHIFT) |
        ((frontFace ? 1u : 0u) << TR_TRACE_FRONT_FACE_SHIFT) |
        ((lastMip & TR_TRACE_LAST_MIP_MASK) << TR_TRACE_LAST_MIP_SHIFT) |
        ((iterationCount & TR_TRACE_ITERATION_MASK) << TR_TRACE_ITERATION_SHIFT);
}

uint4 TrEncodeScreenTraceHit(
    uint packedHitPixel,
    float hitT,
    uint status,
    uint source,
    uint lastMip,
    uint iterationCount,
    bool frontFace,
    float confidence)
{
    return uint4(
        packedHitPixel,
        asuint(hitT),
        TrPackTraceFlags(
            status,
            source,
            lastMip,
            iterationCount,
            frontFace),
        asuint(saturate(confidence)));
}

uint TrGetTraceStatus(uint4 encodedHit)
{
    return (encodedHit.z >> TR_TRACE_STATUS_SHIFT) & TR_TRACE_STATUS_MASK;
}

uint TrGetTraceSource(uint4 encodedHit)
{
    return (encodedHit.z >> TR_TRACE_SOURCE_SHIFT) & TR_TRACE_SOURCE_MASK;
}

uint TrGetTraceLastMip(uint4 encodedHit)
{
    return (encodedHit.z >> TR_TRACE_LAST_MIP_SHIFT) & TR_TRACE_LAST_MIP_MASK;
}

uint TrGetTraceIterationCount(uint4 encodedHit)
{
    return
        (encodedHit.z >> TR_TRACE_ITERATION_SHIFT) &
        TR_TRACE_ITERATION_MASK;
}

float TrGetTraceHitT(uint4 encodedHit)
{
    return asfloat(encodedHit.y);
}

float TrGetTraceConfidence(uint4 encodedHit)
{
    return asfloat(encodedHit.w);
}

#endif
