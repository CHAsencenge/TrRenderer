#ifndef TR_PRIMITIVE_CONSTANTS_ABI_HEADER_HLSL
#define TR_PRIMITIVE_CONSTANTS_ABI_HEADER_HLSL

struct TrPrimitiveConstants
{
    float4x4 world;
    float4x4 previousWorld;
    float4x4 worldInverseTranspose;
    float3 boundsCenter;
    float boundsRadius;
    uint instanceId;
    uint meshId;
    uint parentNodeId;
    uint hierarchyDepth;
};

#endif
