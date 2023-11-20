/*
 * Copyright (c) 2014-2021, NVIDIA CORPORATION.  All rights reserved.
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
 * SPDX-FileCopyrightText: Copyright (c) 2014-2021 NVIDIA CORPORATION
 * SPDX-License-Identifier: Apache-2.0
 */



#define VERTEX_POS    0
#define VERTEX_NORMAL 1
#define VERTEX_UV     2

#define UBO_SCENE     0
#define UBO_OBJECT    1

#if defined(GL_core_profile) || defined(GL_compatibility_profile) || defined(GL_es_profile)

#extension GL_ARB_bindless_texture : require

#endif

#ifdef __cplusplus
namespace basiccmdlist
{
  using namespace glm;
  using namespace nvgl;
#endif

struct SceneData {
  mat4  viewProjMatrix;
  mat4  viewProjMatrixI;
  mat4  viewMatrix;
  mat4  viewMatrixI;
  mat4  viewMatrixIT;
  
  vec4  wLightPos;
  
  uvec2 viewport;
  float shrinkFactor;
  float time;
};

struct ObjectData {
  mat4  worldMatrix;
  mat4  worldMatrixIT;
  vec4  color;
  vec2  texScale;
  vec2  _pad;
  sampler2D texColor;
};

#ifdef __cplusplus
}
#endif

#if defined(GL_core_profile) || defined(GL_compatibility_profile) || defined(GL_es_profile)

#extension GL_NV_command_list : enable
#if GL_NV_command_list
layout(commandBindableNV) uniform;
#endif

layout(std140,binding=UBO_SCENE) uniform sceneBuffer {
  SceneData   scene;
};

layout(std140,binding=UBO_OBJECT) uniform objectBuffer {
  ObjectData  object;
};

#endif
