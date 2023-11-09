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

#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_scalar_block_layout : enable

#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require

#include "wavefront.glsl"


layout(push_constant) uniform _PushConstantRaster
{
  TrPushConstantRaster pcRaster;
};

// clang-format off
// Incoming
layout(location = 1) in vec3 i_worldPos;
layout(location = 2) in vec3 i_worldNrm;
layout(location = 3) in vec3 i_viewDir;
layout(location = 4) in vec2 i_texCoord;
// Outgoing
layout(location = 0) out vec4 o_color;

// buffer_reference表示这是个缓冲区引用
// scalar表示使用标量布局，每个元素都单独对齐
// buffer表示定义 对象是个缓冲区对象
// Vertices是定义的缓冲区对象的名称
// {TrVulkanVertexRT v[]; }定义了缓冲区对象的内容，包含一个TrVulkanVertexRT类型的数组v
layout(buffer_reference, scalar) buffer Vertices {TrVulkanVertexRT v[]; }; // Positions of an object
layout(buffer_reference, scalar) buffer Indices {uint i[]; }; // Triangle indices
layout(buffer_reference, scalar) buffer Materials {TrVulkanWaveFrontMaterial m[]; }; // Array of all materials on an object
layout(buffer_reference, scalar) buffer MatIndices {int i[]; }; // Material ID for each triangle

layout(binding = eObjDescs, scalar) buffer ObjDesc_ { TrObjDescRtBase i[]; } objDesc;
layout(binding = eTextures) uniform sampler2D[] textureSamplers;
// clang-format on


void main()
{
  // Material of the object
  TrObjDescRtBase    objResource = objDesc.i[pcRaster.mObjIndex];
  MatIndices matIndices  = MatIndices(objResource.mMaterialIndexAddress);
  Materials  materials   = Materials(objResource.mMaterialAddress);

  int               matIndex = matIndices.i[gl_PrimitiveID];
  TrVulkanWaveFrontMaterial mat      = materials.m[matIndex];

  vec3 N = normalize(i_worldNrm);

  // Vector toward light
  vec3  L;
  float lightIntensity = pcRaster.mLightIntensity;
  if(pcRaster.mLightType == 0)
  {
    vec3  lDir     = pcRaster.mLightPosition - i_worldPos;
    float d        = length(lDir);
    lightIntensity = pcRaster.mLightIntensity / (d * d);
    L              = normalize(lDir);
  }
  else
  {
    L = normalize(pcRaster.mLightPosition);
  }


  // Diffuse
  vec3 diffuse = computeDiffuse(mat, L, N);
  if(mat.mTextureId >= 0)
  {
    int  txtOffset  = objDesc.i[pcRaster.mObjIndex].txtOffset;
    uint txtId      = txtOffset + mat.mTextureId;
    vec3 diffuseTxt = texture(textureSamplers[nonuniformEXT(txtId)], i_texCoord).xyz;
    diffuse *= diffuseTxt;
  }

  // Specular
  vec3 specular = computeSpecular(mat, i_viewDir, L, N);

  // Result
  o_color = vec4(lightIntensity * (diffuse + specular), 1);
}
