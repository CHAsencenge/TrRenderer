#ifndef TR_NORMAL_MAPPING_HEADER_HLSL
#define TR_NORMAL_MAPPING_HEADER_HLSL

float3 TrApplyNormalMap(
    float3 tangentSpaceNormal,
    float normalScale,
    float3 worldPosition,
    float3 geometricNormal,
    float2 uv)
{
    tangentSpaceNormal.xy *= normalScale;

    const float3 positionDx = ddx(worldPosition);
    const float3 positionDy = ddy(worldPosition);
    const float2 uvDx = ddx(uv);
    const float2 uvDy = ddy(uv);
    const float3 positionDyPerpendicular = cross(positionDy, geometricNormal);
    const float3 positionDxPerpendicular = cross(geometricNormal, positionDx);
    const float3 tangent = positionDyPerpendicular * uvDx.x +
        positionDxPerpendicular * uvDy.x;
    const float3 bitangent = positionDyPerpendicular * uvDx.y +
        positionDxPerpendicular * uvDy.y;
    const float inverseScale = rsqrt(max(
        max(dot(tangent, tangent), dot(bitangent, bitangent)),
        1.0e-8f));
    return normalize(
        tangent * (tangentSpaceNormal.x * inverseScale) +
        bitangent * (tangentSpaceNormal.y * inverseScale) +
        geometricNormal * tangentSpaceNormal.z);
}

#endif
