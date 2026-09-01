#include "screen_probe_common.header.hlsl"

Texture2D<float> g_sceneDepth : register(t0);
Texture2D<float4> g_worldNormal : register(t1);
RWTexture2D<float4> g_probePositionValidity : register(u0);
RWTexture2D<float4> g_probeNormalDepth : register(u1);

cbuffer ScreenProbeBuildConstants : register(b2)
{
    uint g_renderWidth;
    uint g_renderHeight;
    uint g_probeCountX;
    uint g_probeCountY;
    uint g_probeTileSize;
    uint3 g_probePadding;
};

bool TryLoadSurface(uint2 pixel, out float depth, out float3 normal)
{
    depth = g_sceneDepth.Load(int3(pixel, 0));
    normal = g_worldNormal.Load(int3(pixel, 0)).xyz;
    const float normalLengthSquared = dot(normal, normal);
    if(TrIsBackgroundDepth(depth) || normalLengthSquared < 1.0e-6f)
    {
        normal = 0.0f;
        return false;
    }
    normal *= rsqrt(normalLengthSquared);
    return true;
}

[numthreads(8, 8, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint2 probeCoordinate = dispatchThreadId.xy;
    if(any(probeCoordinate >= uint2(g_probeCountX, g_probeCountY)))
    {
        return;
    }

    const uint2 renderSize = uint2(g_renderWidth, g_renderHeight);
    const uint2 tileBegin = probeCoordinate * g_probeTileSize;
    const uint2 tileEnd = min(tileBegin + g_probeTileSize, renderSize);
    const uint2 tileCenter = min(
        tileBegin + g_probeTileSize / 2u,
        renderSize - 1u);

    uint2 representativePixel = tileCenter;
    float representativeDepth;
    float3 representativeNormal;
    bool valid = TryLoadSurface(
        representativePixel,
        representativeDepth,
        representativeNormal);

    // Fixed probes remain stable in screen space. Only background-centered
    // tiles search for the nearest valid pixel, which preserves silhouettes
    // without letting a foreground corner always dominate the entire tile.
    if(!valid)
    {
        uint bestDistanceSquared = 0xffffffffu;
        for(uint y = tileBegin.y; y < tileEnd.y; ++y)
        {
            for(uint x = tileBegin.x; x < tileEnd.x; ++x)
            {
                float candidateDepth;
                float3 candidateNormal;
                if(!TryLoadSurface(
                       uint2(x, y),
                       candidateDepth,
                       candidateNormal))
                {
                    continue;
                }

                const int2 offset = int2(x, y) - int2(tileCenter);
                const uint distanceSquared = uint(dot(offset, offset));
                if(distanceSquared < bestDistanceSquared)
                {
                    bestDistanceSquared = distanceSquared;
                    representativePixel = uint2(x, y);
                    representativeDepth = candidateDepth;
                    representativeNormal = candidateNormal;
                    valid = true;
                }
            }
        }
    }

    if(!valid)
    {
        g_probePositionValidity[probeCoordinate] = 0.0f;
        g_probeNormalDepth[probeCoordinate] = 0.0f;
        return;
    }

    const float3 worldPosition = TrReconstructWorldPosition(
        representativePixel,
        representativeDepth);
    g_probePositionValidity[probeCoordinate] = float4(worldPosition, 1.0f);
    g_probeNormalDepth[probeCoordinate] = float4(
        representativeNormal,
        representativeDepth);
}
