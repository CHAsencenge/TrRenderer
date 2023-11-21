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

enum ETrSceneBindings
{
    Globals = 0,
    ObjDescs = 1,
    Textures = 2
};

enum ETrRtxBindings
{
    Tlas = 0,
    OutImage = 1,
};

struct TrPushConstantRaster
{
    mat4  mModelMatrix;  // matrix of the instance
    vec3  mLightPosition;
    uint  mObjIndex;
    float mLightIntensity;
    int   mLightType;
};

struct TrPushConstantRay
{
    vec4  mClearColor;
    vec3  mLightPosition;
    float mLightIntensity;
    int   mLightType;
};

struct TrGlobalUniforms
{
  mat4 mViewProj;     // Camera view * projection
  mat4 mViewInverse;  // Camera inverse view matrix
  mat4 mProjInverse;  // Camera inverse projection matrix
};

enum TrStageIndices
{
    Raygen,
    Miss,
    ClosestHit,
    ShaderGroupCount
};