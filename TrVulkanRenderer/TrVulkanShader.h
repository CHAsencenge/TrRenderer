// for shader usage (include)
#pragma once

#ifdef __cplusplus
#include <glm/glm.hpp>
// GLSL Type
using vec2 = glm::vec2;
using vec3 = glm::vec3;
using vec4 = glm::vec4;
using mat4 = glm::mat4;
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
    int   mFrame;
    int   mNumRayGenSamples;
};

struct TrGlobalUniforms
{
  mat4 mViewProj;     // Camera view * projection
  mat4 mViewInverse;  // Camera inverse view matrix
  mat4 mProjInverse;  // Camera inverse projection matrix
};



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

struct TrObjDescRasterBase
{
    int mTexOffset;  // texture index offset in the array of textures
    uint64_t mVertexAddress;
    uint64_t mIndexAddress;
    uint64_t mMaterialAddress;  // address of the material buffer
    uint64_t mMaterialIndexAddress;
};

// Material
// mIllum
/* 0 Color on and Ambient off
   1 Color on and Ambient on
   2 Highlight on
   3 Reflection on and Ray trace on
   4 Transparency: Glass on
   Reflection: Ray trace on
   5 Reflection: Fresnel on and Ray trace on
   6 Transparency: Refraction on
   Reflection: Fresnel off and Ray trace on
   7 Transparency: Refraction on
   Reflection: Fresnel on and Ray trace on
   8 Reflection on and Ray trace off
   9 Transparency: Glass on
   Reflection: Ray trace off
   10 Casts shadows onto invisible surfaces
*/

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


START_BINDING (ETrRasterSceneBindings)
    RasterGlobals = 0,
    RasterTextures = 1
END_BINDING();