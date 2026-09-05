#ifndef TR_VIEW_CONSTANTS_ABI_HEADER_HLSL
#define TR_VIEW_CONSTANTS_ABI_HEADER_HLSL

struct TrViewConstants
{
    float4x4 view;
    float4x4 projection;
    float4x4 viewProjection;
    float4x4 inverseViewProjection;
    float4x4 previousViewProjection;
    float3 cameraPosition;
    float nearPlane;
    float2 renderSize;
    float2 inverseRenderSize;
    float2 temporalJitter;
    float2 previousTemporalJitter;
    uint frameNumber;
    float farPlane;
    float2 padding;
};

#endif
