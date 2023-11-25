/*
 * Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2019-2021 NVIDIA CORPORATION
 * SPDX-License-Identifier: Apache-2.0
 */

#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_GOOGLE_include_directive : enable

#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require

#include "raycommon.glsl"
#include "wavefront.glsl"

hitAttributeEXT vec2 attribs;

// clang-format off
layout(location = 0) rayPayloadInEXT hitPayload prd;
layout(location = 1) rayPayloadEXT bool isShadowed;

layout(buffer_reference, scalar) buffer Vertices {TrVulkanVertexRT v[]; }; // Positions of an object
layout(buffer_reference, scalar) buffer Indices {ivec3 i[]; }; // Triangle indices
layout(buffer_reference, scalar) buffer Materials {TrVulkanWaveFrontMaterial m[]; }; // Array of all materials on an object
layout(buffer_reference, scalar) buffer MatIndices {int i[]; }; // Material ID for each triangle
layout(set = 0, binding = Tlas) uniform accelerationStructureEXT topLevelAS;
layout(set = 1, binding = ObjDescs, scalar) buffer ObjDesc_ { TrObjDescRtBase i[]; } objDesc;
layout(set = 1, binding = Textures) uniform sampler2D textureSamplers[];

layout(push_constant) uniform _PushConstantRay { TrPushConstantRay pcRay; };
// clang-format on


void main()
{
  // Object data
  TrObjDescRtBase objResource = objDesc.i[gl_InstanceCustomIndexEXT];
  MatIndices matIndices  = MatIndices(objResource.mMaterialIndexAddress);
  Materials  materials   = Materials(objResource.mMaterialAddress);
  Indices    indices     = Indices(objResource.mIndexAddress);
  Vertices   vertices    = Vertices(objResource.mVertexAddress);

  // Indices of the triangle
  ivec3 ind = indices.i[gl_PrimitiveID];

  // Vertex of the triangle
  TrVulkanVertexRT v0 = vertices.v[ind.x];
  TrVulkanVertexRT v1 = vertices.v[ind.y];
  TrVulkanVertexRT v2 = vertices.v[ind.z];

  const vec3 barycentrics = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);

  // Computing the coordinates of the hit position
  const vec3 pos      = v0.mPos * barycentrics.x + v1.mPos * barycentrics.y + v2.mPos * barycentrics.z;
  // 一般认作列向量，并搭配左乘
  const vec3 worldPos = vec3(gl_ObjectToWorldEXT * vec4(pos, 1.0));  // Transforming the position to world space

  // Computing the normal at hit position
  const vec3 nrm      = v0.mNrm * barycentrics.x + v1.mNrm * barycentrics.y + v2.mNrm * barycentrics.z;
  // const vec3 worldNrm = normalize(vec3(nrm * gl_WorldToObjectEXT));  // Transforming the normal to world space
  const vec3 worldNrm = normalize(vec3(gl_ObjectToWorldEXT * vec4(nrm, 1.0)));  // Transforming the normal to world space

  // Vector toward the light
  vec3  L;
  float lightIntensity = pcRay.mLightIntensity;
  float lightDistance  = 100000.0;
  // Point light
  if(pcRay.mLightType == 0)
  {
    vec3 lDir      = pcRay.mLightPosition - worldPos;
    lightDistance  = length(lDir);
    lightIntensity = pcRay.mLightIntensity / (lightDistance * lightDistance);
    L              = normalize(lDir);
  }
  else  // Directional light
  {
    L = normalize(pcRay.mLightPosition);
  }

  // Material of the object
  int               matIdx = matIndices.i[gl_PrimitiveID];
  TrVulkanWaveFrontMaterial mat    = materials.m[matIdx];
  
  
  // test code
  // mat.mDiffuse = vec3(0.8, 0.8, 0.8);
  

  // Diffuse
  vec3 diffuse = computeDiffuse(mat, L, worldNrm);
  if(mat.mTextureId >= 0)
  {
    uint txtId    = mat.mTextureId + objDesc.i[gl_InstanceCustomIndexEXT].mTexOffset;
    vec2 texCoord = v0.mTexCoord * barycentrics.x + v1.mTexCoord * barycentrics.y + v2.mTexCoord * barycentrics.z;
    diffuse *= texture(textureSamplers[nonuniformEXT(txtId)], texCoord).xyz;
  }

  vec3  specular    = vec3(0);
  float attenuation = 1;

  // Tracing shadow ray only if the light is visible from the surface
  if(dot(worldNrm, L) > 0)
  {
    float tMin   = 0.001;
    float tMax   = lightDistance;
    vec3  origin = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
    vec3  rayDir = L;
    // uint  flags  = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT;
    uint  flags  = gl_RayFlagsSkipClosestHitShaderEXT;

    isShadowed   = true;
    traceRayEXT(topLevelAS,  // acceleration structure
                flags,       // rayFlags
                0xFF,        // cullMask
                0,           // sbtRecordOffset
                0,           // sbtRecordStride
                1,           // missIndex 这个做到选择shadow miss shader
                origin,      // ray origin
                tMin,        // ray min range
                rayDir,      // ray direction
                tMax,        // ray max range
                1            // payload (location = 1)
    );

    if(isShadowed)
    {
      attenuation = 0.3;
    }
    else
    {
      // Specular
      specular = computeSpecular(mat, gl_WorldRayDirectionEXT, L, worldNrm);
    }
  }

  prd.hitValue = vec3(lightIntensity * attenuation * (diffuse + specular));
}
