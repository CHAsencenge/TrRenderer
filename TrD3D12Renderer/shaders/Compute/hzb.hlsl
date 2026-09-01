#ifndef TR_REVERSED_Z
#define TR_REVERSED_Z 1
#endif

Texture2D<float> g_sourceDepth : register(t0);
RWTexture2D<float> g_destinationDepth : register(u0);

cbuffer HzbBuildConstants : register(b2)
{
    uint g_sourceWidth;
    uint g_sourceHeight;
    uint g_destinationWidth;
    uint g_destinationHeight;
};

float ReduceDepth(float left, float right)
{
#if TR_REVERSED_Z
    return max(left, right);
#else
    return min(left, right);
#endif
}

[numthreads(8, 8, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint2 destinationPixel = dispatchThreadId.xy;
    if(any(destinationPixel >= uint2(
        g_destinationWidth,
        g_destinationHeight)))
    {
        return;
    }

    // Ratio-based footprints cover every source texel for odd dimensions;
    // a fixed 2x2 footprint would drop the last row or column.
    const uint2 sourceSize = uint2(g_sourceWidth, g_sourceHeight);
    const uint2 destinationSize = uint2(
        g_destinationWidth,
        g_destinationHeight);
    const uint2 sourceBegin = destinationPixel * sourceSize / destinationSize;
    const uint2 sourceEnd = min(
        ((destinationPixel + 1u) * sourceSize + destinationSize - 1u) /
            destinationSize,
        sourceSize);

    float reducedDepth = g_sourceDepth.Load(int3(sourceBegin, 0));
    for(uint sourceY = sourceBegin.y; sourceY < sourceEnd.y; ++sourceY)
    {
        for(uint sourceX = sourceBegin.x; sourceX < sourceEnd.x; ++sourceX)
        {
            reducedDepth = ReduceDepth(
                reducedDepth,
                g_sourceDepth.Load(int3(sourceX, sourceY, 0)));
        }
    }
    g_destinationDepth[destinationPixel] = reducedDepth;
}
