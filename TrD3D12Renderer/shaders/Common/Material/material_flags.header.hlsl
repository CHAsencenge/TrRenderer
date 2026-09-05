#ifndef TR_MATERIAL_FLAGS_HEADER_HLSL
#define TR_MATERIAL_FLAGS_HEADER_HLSL

#include "../ABI/material_flags.header.hlsl"

bool TrMaterialHasFlag(uint flags, uint flag)
{
    return (flags & flag) != 0u;
}

#endif
