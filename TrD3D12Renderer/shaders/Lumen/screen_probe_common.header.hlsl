#ifndef TR_SCREEN_PROBE_COMMON_HEADER_HLSL
#define TR_SCREEN_PROBE_COMMON_HEADER_HLSL

#include "../Common/depth.header.hlsl"

cbuffer ViewConstants : register(b1)
{
    float4x4 g_view;
    float4x4 g_projection;
    float4x4 g_viewProjection;
    float4x4 g_inverseViewProjection;
    float4x4 g_previousViewProjection;
    float3 g_cameraPosition;
    float g_nearPlane;
    float2 g_renderSize;
    float2 g_inverseRenderSize;
    float2 g_temporalJitter;
    float2 g_previousTemporalJitter;
    uint g_frameNumber;
    float g_farPlane;
    float2 g_viewPadding;
};

float2 TrPixelToScreenUv(uint2 pixel)
{
    return (float2(pixel) + 0.5f) * g_inverseRenderSize;
}

float3 TrReconstructWorldPosition(uint2 pixel, float deviceDepth)
{
    const float2 screenUv = TrPixelToScreenUv(pixel);
    const float2 ndc = float2(
        screenUv.x * 2.0f - 1.0f,
        1.0f - screenUv.y * 2.0f);
    const float4 worldPosition = mul(
        float4(ndc, deviceDepth, 1.0f),
        g_inverseViewProjection);
    return worldPosition.xyz / max(abs(worldPosition.w), 1.0e-6f);
}

bool TrProjectWorldToScreen(
    float3 worldPosition,
    out float2 screenUv,
    out float deviceDepth)
{
    const float4 clipPosition = mul(
        float4(worldPosition, 1.0f),
        g_viewProjection);
    if(clipPosition.w <= 1.0e-6f)
    {
        screenUv = 0.0f;
        deviceDepth = 0.0f;
        return false;
    }

    const float3 ndc = clipPosition.xyz / clipPosition.w;
    screenUv = float2(
        ndc.x * 0.5f + 0.5f,
        0.5f - ndc.y * 0.5f);
    deviceDepth = ndc.z;
    return !any(screenUv < 0.0f) && !any(screenUv >= 1.0f) &&
        deviceDepth >= 0.0f && deviceDepth <= 1.0f;
}

bool TrRayIsBehindScene(float rayDeviceDepth, float sceneDeviceDepth)
{
#if TR_REVERSED_Z
    return rayDeviceDepth <= sceneDeviceDepth;
#else
    return rayDeviceDepth >= sceneDeviceDepth;
#endif
}

#endif
