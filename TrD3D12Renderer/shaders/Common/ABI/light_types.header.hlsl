#ifndef TR_LIGHT_TYPES_ABI_HEADER_HLSL
#define TR_LIGHT_TYPES_ABI_HEADER_HLSL

static const uint TR_LIGHT_TYPE_DIRECTIONAL = 0u;
static const uint TR_LIGHT_TYPE_POINT = 1u;
static const uint TR_LIGHT_TYPE_SPOT = 2u;

struct TrGpuLight
{
    float3 position;
    uint type;
    float3 direction;
    float intensity;
    float3 color;
    float range;
    float innerConeCos;
    float outerConeCos;
    float2 padding;
};

#endif
