#ifndef TR_SCENE_CONSTANTS_ABI_HEADER_HLSL
#define TR_SCENE_CONSTANTS_ABI_HEADER_HLSL

struct TrSceneConstants
{
    float3 ambientColor;
    float ambientStrength;
    uint lightCount;
    float3 padding;
};

#endif
