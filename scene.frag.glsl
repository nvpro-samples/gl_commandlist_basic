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



#version 430
/**/

#extension GL_ARB_shading_language_include : enable
#include "common.h"

in Interpolants {
  vec3 wPos;
  vec3 wNormal;
  vec2 uv;
} IN;

layout(location=0,index=0) out vec4 out_Color;

void main()
{
  vec4 color = texture(object.texColor, IN.uv * object.texScale.xy);
  
  vec3 lightDir = normalize(scene.wLightPos.xyz - IN.wPos);
  vec3 viewDir  = normalize((-scene.viewMatrix[3].xyz) - IN.wPos);
  vec3 halfDir  = normalize(lightDir + viewDir);
  vec3 normal   = normalize(IN.wNormal);
  
  float intensity = max(0,dot(normal,lightDir));
  intensity += pow(max(0,dot(normal,halfDir)),8);
  
  out_Color = color * object.color * intensity;
}
