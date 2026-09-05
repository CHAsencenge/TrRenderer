#ifndef TR_DRAW_CONSTANTS_ABI_HEADER_HLSL
#define TR_DRAW_CONSTANTS_ABI_HEADER_HLSL

struct TrDrawConstants
{
    uint primitiveId;
    uint materialId;
    uint localPrimitiveIndex;
    uint flags;
};

#endif
