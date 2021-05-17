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

layout(triangles) in;
layout(triangle_strip,max_vertices=3) out;

in Interpolants {
  vec3 wPos;
  vec3 wNormal;
  vec2 uv;
} IN[];

out Interpolants {
  vec3 wPos;
  vec3 wNormal;
  vec2 uv;
} OUT;

void main()
{
  const int numVertices = IN.length();
  vec3 avg = vec3(0);
  for (int i = 0; i < numVertices; i++){
    avg += IN[i].wPos;
  }
  
  avg /= float(numVertices);
  
  bool useFaceNormal = fract(scene.time * 2.0) > 0.5;
  vec3 normal = vec3(0);
  if (useFaceNormal) {
    vec3 a = IN[1].wPos-IN[0].wPos;
    vec3 b = IN[2].wPos-IN[0].wPos;
    normal = normalize(cross(a,b));
  }
  
  float shrink = (cos(scene.time) * 0.5 + 0.5) * scene.shrinkFactor;
  
  for (int i = 0; i < numVertices; i++){
    vec3 wPos = mix(IN[i].wPos, avg, shrink);
    OUT.wPos = wPos;
    OUT.wNormal = useFaceNormal ? normal : IN[i].wNormal;
    OUT.uv = IN[i].uv;
    gl_Position = scene.viewProjMatrix * vec4(wPos,1);
    EmitVertex();
  }
  
}
