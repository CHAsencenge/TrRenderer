#ifndef TR_VIEW_PROJECTION_HEADER_HLSL
#define TR_VIEW_PROJECTION_HEADER_HLSL

float2 TrPixelToScreenUv(uint2 pixel, float2 inverseRenderSize)
{
    return (float2(pixel) + 0.5f) * inverseRenderSize;
}

float3 TrReconstructWorldPosition(
    uint2 pixel,
    float deviceDepth,
    float2 inverseRenderSize,
    float4x4 inverseViewProjection)
{
    const float2 screenUv = TrPixelToScreenUv(pixel, inverseRenderSize);
    const float2 ndc = float2(
        screenUv.x * 2.0f - 1.0f,
        1.0f - screenUv.y * 2.0f);
    const float4 worldPosition = mul(
        float4(ndc, deviceDepth, 1.0f),
        inverseViewProjection);
    return worldPosition.xyz / max(abs(worldPosition.w), 1.0e-6f);
}

bool TrProjectWorldToScreen(
    float3 worldPosition,
    float4x4 viewProjection,
    out float2 screenUv,
    out float deviceDepth)
{
    const float4 clipPosition = mul(
        float4(worldPosition, 1.0f),
        viewProjection);
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

#endif
