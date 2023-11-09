#pragma once

#ifdef __cplusplus
#include "nvmath/nvmath.h"
// GLSL Type
using vec2 = nvmath::vec2f;
using vec3 = nvmath::vec3f;
using vec4 = nvmath::vec4f;
using mat4 = nvmath::mat4f;
using uint = unsigned int;
#endif

struct TrVulkanWaveFrontMaterial  // See ObjLoader, copy of MaterialObj, could be compressed for device
{
    vec3  mAmbient;
    vec3  mDiffuse;
    vec3  mSpecular;
    vec3  mTransmittance;
    vec3  mEmission;
    float mShininess;
    float mIor;       // index of refraction
    float mDissolve;  // 1 == opaque; 0 == fully transparent
    int   mIllum;     // illumination model (see http://www.fileformat.info/format/material/)
    int   mTextureId;
};