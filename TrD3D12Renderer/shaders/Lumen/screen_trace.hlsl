#include "screen_probe_common.header.hlsl"
#include "screen_trace_hit.header.hlsl"

Texture2D<float4> g_probePositionValidity : register(t0);
Texture2D<float4> g_probeNormalDepth : register(t1);
Texture2D<float> g_hzb : register(t2);
RWTexture2D<uint4> g_traceHit : register(u0);
RWTexture2D<float4> g_traceDebug : register(u1);

cbuffer ScreenTraceConstants : register(b2)
{
    uint g_probeCountX;
    uint g_probeCountY;
    uint g_traceAtlasWidth;
    uint g_traceAtlasHeight;
    uint g_rayGridDimension;
    uint g_hzbMipCount;
    uint g_startMip;
    uint g_maxIterations;
    float g_maxTraceDistance;
    float g_surfaceBias;
    float g_surfaceThickness;
    float g_baseStep;
};

static const float TR_PI = 3.14159265359f;
uint HashUint(uint value)
{
    value ^= value >> 16u;
    value *= 0x7feb352du;
    value ^= value >> 15u;
    value *= 0x846ca68bu;
    value ^= value >> 16u;
    return value;
}

float RadicalInverse(uint bits)
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

float3 GenerateProbeRay(
    uint2 probeCoordinate,
    uint rayIndex,
    uint rayCount,
    float3 worldNormal)
{
    const uint probeHash = HashUint(
        probeCoordinate.x + probeCoordinate.y * max(g_probeCountX, 1u));
    const float rotation = float(probeHash & 0xffffu) / 65536.0f;
    const float2 samplePoint = float2(
        (float(rayIndex) + 0.5f) / float(rayCount),
        frac(RadicalInverse(rayIndex) + rotation));

    // Cosine-weighted hemisphere: denser rays around the surface normal where
    // diffuse irradiance has the largest contribution.
    const float radius = sqrt(samplePoint.x);
    const float phi = 2.0f * TR_PI * samplePoint.y;
    const float3 localDirection = float3(
        radius * cos(phi),
        radius * sin(phi),
        sqrt(max(0.0f, 1.0f - samplePoint.x)));

    const float3 helperAxis = abs(worldNormal.z) < 0.999f
        ? float3(0.0f, 0.0f, 1.0f)
        : float3(0.0f, 1.0f, 0.0f);
    const float3 tangent = normalize(cross(helperAxis, worldNormal));
    const float3 bitangent = cross(worldNormal, tangent);
    return normalize(
        tangent * localDirection.x +
        bitangent * localDirection.y +
        worldNormal * localDirection.z);
}

float LoadHzb(float2 screenUv, uint mipLevel)
{
    const uint2 mipSize = max(
        uint2(1u, 1u),
        uint2(g_renderSize) >> mipLevel);
    const uint2 pixel = min(
        uint2(screenUv * float2(mipSize)),
        mipSize - 1u);
    return g_hzb.Load(int3(pixel, mipLevel));
}

bool IsPotentialIntersection(
    float rayDeviceDepth,
    float sceneDeviceDepth)
{
    return !TrIsBackgroundDepth(sceneDeviceDepth) &&
        TrRayIsBehindScene(rayDeviceDepth, sceneDeviceDepth);
}

bool RefineIntersection(
    float3 rayOrigin,
    float3 rayDirection,
    float frontDistance,
    float behindDistance,
    out float hitDistance,
    out float2 hitUv,
    out float confidence)
{
    float front = frontDistance;
    float behind = behindDistance;
    hitDistance = behindDistance;
    hitUv = 0.0f;
    confidence = 0.0f;

    [unroll]
    for(uint iteration = 0; iteration < 6u; ++iteration)
    {
        const float middle = (front + behind) * 0.5f;
        float2 middleUv;
        float middleDepth;
        if(!TrProjectWorldToScreen(
               rayOrigin + rayDirection * middle,
               middleUv,
               middleDepth))
        {
            return false;
        }

        const float sceneDepth = LoadHzb(middleUv, 0u);
        if(IsPotentialIntersection(middleDepth, sceneDepth))
        {
            behind = middle;
            hitUv = middleUv;
        }
        else
        {
            front = middle;
        }
    }

    hitDistance = behind;
    float hitDeviceDepth;
    if(!TrProjectWorldToScreen(
           rayOrigin + rayDirection * hitDistance,
           hitUv,
           hitDeviceDepth))
    {
        return false;
    }

    const float sceneDeviceDepth = LoadHzb(hitUv, 0u);
    if(TrIsBackgroundDepth(sceneDeviceDepth))
    {
        return false;
    }
    const float rayViewDepth = TrDeviceDepthToViewDepth(
        hitDeviceDepth,
        g_nearPlane,
        g_farPlane);
    const float sceneViewDepth = TrDeviceDepthToViewDepth(
        sceneDeviceDepth,
        g_nearPlane,
        g_farPlane);
    const float separation = rayViewDepth - sceneViewDepth;
    const float thickness = max(
        g_surfaceThickness,
        sceneViewDepth * 0.002f);
    const bool isValid =
        separation >= -thickness * 0.25f && separation <= thickness;
    if(isValid)
    {
        const float thicknessConfidence =
            saturate(1.0f - abs(separation) / thickness);
        const float2 edgeDistanceUv = min(hitUv, 1.0f - hitUv);
        const float edgeDistancePixels = min(
            edgeDistanceUv.x * g_renderSize.x,
            edgeDistanceUv.y * g_renderSize.y);
        const float edgeConfidence = saturate(edgeDistancePixels / 4.0f);
        confidence = thicknessConfidence * edgeConfidence;
    }
    return isValid;
}

void StoreTraceResult(
    uint2 tracePixel,
    uint status,
    uint source,
    uint packedHitPixel,
    float hitDistance,
    float confidence,
    uint lastMip,
    uint iterationCount,
    float2 debugUv)
{
    g_traceHit[tracePixel] = TrEncodeScreenTraceHit(
        packedHitPixel,
        hitDistance,
        status,
        source,
        lastMip,
        iterationCount,
        false,
        confidence);
    g_traceDebug[tracePixel] = float4(
        debugUv,
        saturate(hitDistance / max(g_maxTraceDistance, 1.0e-6f)),
        float(status));
}

[numthreads(8, 8, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint2 tracePixel = dispatchThreadId.xy;
    if(any(tracePixel >= uint2(g_traceAtlasWidth, g_traceAtlasHeight)))
    {
        return;
    }

    const uint2 probeCoordinate = tracePixel / g_rayGridDimension;
    const uint2 rayCoordinate = tracePixel % g_rayGridDimension;
    const uint rayIndex =
        rayCoordinate.y * g_rayGridDimension + rayCoordinate.x;
    const uint rayCount = g_rayGridDimension * g_rayGridDimension;

    const float4 positionValidity =
        g_probePositionValidity.Load(int3(probeCoordinate, 0));
    const float4 normalDepth =
        g_probeNormalDepth.Load(int3(probeCoordinate, 0));
    if(positionValidity.w <= 0.0f || dot(normalDepth.xyz, normalDepth.xyz) < 1.0e-6f)
    {
        StoreTraceResult(
            tracePixel,
            TR_SCREEN_TRACE_INVALID_PROBE,
            TR_TRACE_SOURCE_NONE,
            TR_INVALID_HIT_PIXEL,
            0.0f,
            0.0f,
            0u,
            0u,
            0.0f);
        return;
    }

    const float3 worldNormal = normalize(normalDepth.xyz);
    const float3 rayDirection = GenerateProbeRay(
        probeCoordinate,
        rayIndex,
        rayCount,
        worldNormal);
    const float3 rayOrigin =
        positionValidity.xyz + worldNormal * g_surfaceBias;

    float frontDistance = 0.0f;
    float traceDistance = max(g_baseStep, g_surfaceBias);
    uint mipLevel = min(g_startMip, g_hzbMipCount - 1u);

    [loop]
    for(uint iteration = 0; iteration < g_maxIterations; ++iteration)
    {
        if(traceDistance > g_maxTraceDistance)
        {
            StoreTraceResult(
                tracePixel,
                TR_SCREEN_TRACE_MISS_DISTANCE,
                TR_TRACE_SOURCE_NONE,
                TR_INVALID_HIT_PIXEL,
                traceDistance,
                0.0f,
                mipLevel,
                iteration,
                0.0f);
            return;
        }

        float2 screenUv;
        float rayDeviceDepth;
        if(!TrProjectWorldToScreen(
               rayOrigin + rayDirection * traceDistance,
               screenUv,
               rayDeviceDepth))
        {
            StoreTraceResult(
                tracePixel,
                TR_SCREEN_TRACE_MISS_SCREEN,
                TR_TRACE_SOURCE_NONE,
                TR_INVALID_HIT_PIXEL,
                traceDistance,
                0.0f,
                mipLevel,
                iteration + 1u,
                saturate(screenUv));
            return;
        }

        const float sceneDeviceDepth = LoadHzb(screenUv, mipLevel);
        if(IsPotentialIntersection(rayDeviceDepth, sceneDeviceDepth))
        {
            if(mipLevel > 0u)
            {
                --mipLevel;
                continue;
            }

            float hitDistance;
            float2 hitUv;
            float hitConfidence;
            if(RefineIntersection(
                   rayOrigin,
                   rayDirection,
                   frontDistance,
                   traceDistance,
                   hitDistance,
                   hitUv,
                   hitConfidence))
            {
                const uint2 hitPixel = min(
                    uint2(hitUv * g_renderSize),
                    uint2(g_renderSize) - 1u);
                StoreTraceResult(
                    tracePixel,
                    TR_SCREEN_TRACE_HIT,
                    TR_TRACE_SOURCE_SCREEN,
                    TrPackHitPixel(hitPixel),
                    hitDistance,
                    hitConfidence,
                    mipLevel,
                    iteration + 1u,
                    hitUv);
                return;
            }

            // A depth discontinuity can look like a crossing at a coarse
            // sample. Move forward conservatively instead of accepting it.
            frontDistance = traceDistance;
            traceDistance += g_baseStep;
            mipLevel = 0u;
            continue;
        }

        frontDistance = traceDistance;
        traceDistance += g_baseStep * float(1u << mipLevel);
        mipLevel = min(mipLevel + 1u, g_hzbMipCount - 1u);
    }

    StoreTraceResult(
        tracePixel,
        TR_SCREEN_TRACE_MISS_ITERATIONS,
        TR_TRACE_SOURCE_NONE,
        TR_INVALID_HIT_PIXEL,
        traceDistance,
        0.0f,
        mipLevel,
        g_maxIterations,
        0.0f);
}
