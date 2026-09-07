
#ifndef TR_SCREEN_PROBE_DIRECTION_HEADER_HLSL
#define TR_SCREEN_PROBE_DIRECTION_HEADER_HLSL

static const float TR_DIRECTION_PI = 3.14159265359f;

float2 TrSignNotZero(float2 value)
{
    return float2(
        value.x >= 0.0f ? 1.0f : -1.0f,
        value.y >= 0.0f ? 1.0f : -1.0f);
}


// 世界空间单位方向 -> octahedral [-1, 1]^2。
float2 TrEncodeOctahedralDirection(float3 direction)
{
    const float3 n = normalize(direction);
    const float inverseL1Norm = rcp(
        max(abs(n.x) + abs(n.y) + abs(n.z), 1.0e-6f));

    float2 oct = n.xy * inverseL1Norm;

    if (n.z < 0.0f)
    {
        oct = (1.0f - abs(oct.yx)) *
            TrSignNotZero(oct);
    }

    return oct;
}

// Octahedral [-1, 1]^2 -> 世界空间单位方向。
float3 TrDecodeOctahedralDirection(float2 oct)
{
    float3 direction = float3(
        oct,
        1.0f - abs(oct.x) - abs(oct.y));

    const float fold = max(-direction.z, 0.0f);

    direction.xy += float2(
        direction.x >= 0.0f ? -fold : fold,
        direction.y >= 0.0f ? -fold : fold);

    return normalize(direction);
}

// 固定方向 texel 中心对应的世界空间方向。
float3 TrDirectionTexelToWorldDirection(
    uint2 directionTexel,
    uint directionDimension)
{
    const float2 uv =
        (float2(directionTexel) + 0.5f) /
        max(float(directionDimension), 1.0f);

    return TrDecodeOctahedralDirection(
        uv * 2.0f - 1.0f);
}

uint2 TrDirectionAtlasCoordinate(
    uint2 probeCoordinate,
    uint2 directionTexel,
    uint directionDimension)
{
    return
        probeCoordinate * directionDimension +
        directionTexel;
}

// 小球面三角形的 solid angle。
// 由球心出发的三个单位方向向量 \(a,b,c\) 在单位球面上围成的球面三角形面积
float TrSphericalTriangleSolidAngle(
    float3 a,
    float3 b,
    float3 c)
{
    const float numerator =
        abs(dot(a, cross(b, c))); // 标量三重积，其绝对值等于三个向量构成的平行六面体体积，描述三个方向“不共面”的程度

    const float denominator =
        1.0f +
        dot(a, b) +
        dot(b, c) +
        dot(c, a);

    return 2.0f * atan2(
        numerator,
        max(denominator, 1.0e-7f));
}

// 标准 oct map 不是严格等面积映射的，因此根据 texel 四角计算该 texel 实际覆盖的球面立体角。
// 同样大的二维 texel，在八面体面中心对应的球面立体角，可以明显大于靠近八面体顶点的 texel
float TrDirectionTexelSolidAngle(
    uint2 directionTexel,
    uint directionDimension)
{
    const float inverseDimension =
        rcp(max(float(directionDimension), 1.0f));

    const float2 uv00 =
        float2(directionTexel) * inverseDimension;
    const float2 uv10 =
        float2(directionTexel + uint2(1u, 0u)) *
        inverseDimension;
    const float2 uv01 =
        float2(directionTexel + uint2(0u, 1u)) *
        inverseDimension;
    const float2 uv11 =
        float2(directionTexel + uint2(1u, 1u)) *
        inverseDimension;

    const float3 d00 =
        TrDecodeOctahedralDirection(uv00 * 2.0f - 1.0f);
    const float3 d10 =
        TrDecodeOctahedralDirection(uv10 * 2.0f - 1.0f);
    const float3 d01 =
        TrDecodeOctahedralDirection(uv01 * 2.0f - 1.0f);
    const float3 d11 =
        TrDecodeOctahedralDirection(uv11 * 2.0f - 1.0f);
    
    // 一个 oct texel 是球面四边形，因此通常拆成两个球面三角形
    return
        TrSphericalTriangleSolidAngle(d00, d10, d11) +
        TrSphericalTriangleSolidAngle(d00, d11, d01);
}

float TrRadianceLuminance(float3 radiance)
{
    return dot(
        radiance,
        float3(0.2126f, 0.7152f, 0.0722f));
}

#endif