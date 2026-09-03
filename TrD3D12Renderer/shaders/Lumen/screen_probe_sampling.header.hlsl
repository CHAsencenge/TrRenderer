#ifndef TR_SCREEN_PROBE_SAMPLING_HEADER_HLSL
#define TR_SCREEN_PROBE_SAMPLING_HEADER_HLSL

static const float TR_SCREEN_PROBE_PI = 3.14159265359f;

uint TrScreenProbeHashUint(uint value)
{
    value ^= value >> 16u;
    value *= 0x7feb352du;
    value ^= value >> 15u;
    value *= 0x846ca68bu;
    value ^= value >> 16u;
    return value;
}

float TrScreenProbeRadicalInverse(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) |
        ((bits & 0xaaaaaaaau) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) |
        ((bits & 0xccccccccu) >> 2u);
    bits = ((bits & 0x0f0f0f0fu) << 4u) |
        ((bits & 0xf0f0f0f0u) >> 4u);
    bits = ((bits & 0x00ff00ffu) << 8u) |
        ((bits & 0xff00ff00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10f;
}

float3 TrGenerateScreenProbeRay(
    uint2 probeCoordinate,
    uint probeCountX,
    uint rayIndex,
    uint rayCount,
    uint frameNumber,
    float3 worldNormal)
{
    const uint probeHash = TrScreenProbeHashUint(
        probeCoordinate.x + probeCoordinate.y * max(probeCountX, 1u));
    const float probeRotation = float(probeHash & 0xffffu) / 65536.0f;
    const uint temporalIndex = frameNumber + 1u;
    const float2 temporalShift = float2(
        TrScreenProbeRadicalInverse(temporalIndex),
        frac(float(temporalIndex) * 0.7548776662466927f));
    const float2 samplePoint = frac(float2(
        (float(rayIndex) + 0.5f) / float(rayCount),
        TrScreenProbeRadicalInverse(rayIndex) + probeRotation) + temporalShift);

    const float radius = sqrt(samplePoint.x);
    const float phi = 2.0f * TR_SCREEN_PROBE_PI * samplePoint.y;
    const float3 localDirection = float3(
        radius * cos(phi),
        radius * sin(phi),
        sqrt(max(0.0f, 1.0f - samplePoint.x)));

    const float3 normal = normalize(worldNormal);
    const float3 helperAxis = abs(normal.z) < 0.999f
        ? float3(0.0f, 0.0f, 1.0f)
        : float3(0.0f, 1.0f, 0.0f);
    const float3 tangent = normalize(cross(helperAxis, normal));
    const float3 bitangent = cross(normal, tangent);
    return normalize(
        tangent * localDirection.x +
        bitangent * localDirection.y +
        normal * localDirection.z);
}

#endif
