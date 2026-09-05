#ifndef TR_DEPTH_HEADER_HLSL
#define TR_DEPTH_HEADER_HLSL

#ifndef TR_REVERSED_Z
#define TR_REVERSED_Z 0
#endif

bool TrIsBackgroundDepth(float deviceDepth)
{
#if TR_REVERSED_Z
    return deviceDepth <= 0.0f;
#else
    return deviceDepth >= 1.0f;
#endif
}

float TrDeviceDepthToViewDepth(
    float deviceDepth,
    float nearPlane,
    float farPlane)
{
#if TR_REVERSED_Z
    const float denominator =
        nearPlane + deviceDepth * (farPlane - nearPlane);
#else
    const float denominator =
        farPlane - deviceDepth * (farPlane - nearPlane);
#endif
    return nearPlane * farPlane / max(denominator, 0.0001f);
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
