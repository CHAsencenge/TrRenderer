// for shader usage (include)
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

#ifdef __cplusplus // Descriptor binding helper for C++ and GLSL
#define START_BINDING(a) enum a {
#define END_BINDING() }
#else
#define START_BINDING(a)  const uint
#define END_BINDING() 
#endif

// Binding
START_BINDING (ETrSceneBindings)
    Globals = 0,
    ObjDescs = 1,
    Textures = 2
END_BINDING();

START_BINDING (ETrRtxBindings)
    Tlas = 0,
    OutImage = 1
END_BINDING();

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

START_BINDING (TrStageIndices)
    Raygen = 0,
    Miss = 1,
    ClosestHit = 2,
    ShaderGroupCount = 3
END_BINDING();

struct TrVulkanVertexRT
{
    vec3 mPos;
    vec3 mNrm;
    vec3 mColor;
    vec2 mTexCoord;
};

struct TrObjDescRtBase
{
    int mTexOffset;  // texture index offset in the array of textures
    uint64_t mVertexAddress;
    uint64_t mIndexAddress;
    uint64_t mMaterialAddress;  // address of the material buffer
    uint64_t mMaterialIndexAddress;
};

// Material
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