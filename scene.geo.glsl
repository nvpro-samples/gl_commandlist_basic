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
  vec3 avg = vec3(0);
  for (int i = 0; i < IN.length; i++){
    avg += IN[i].wPos;
  }
  
  avg /= float(IN.length);
  
  bool useFaceNormal = fract(scene.time * 2.0) > 0.5;
  vec3 normal = vec3(0);
  if (useFaceNormal) {
    vec3 a = IN[1].wPos-IN[0].wPos;
    vec3 b = IN[2].wPos-IN[0].wPos;
    normal = normalize(cross(a,b));
  }
  
  float shrink = (cos(scene.time) * 0.5 + 0.5) * scene.shrinkFactor;
  
  for (int i = 0; i < IN.length; i++){
    vec3 wPos = mix(IN[i].wPos, avg, shrink);
    OUT.wPos = wPos;
    OUT.wNormal = useFaceNormal ? normal : IN[i].wNormal;
    OUT.uv = IN[i].uv;
    gl_Position = scene.viewProjMatrix * vec4(wPos,1);
    EmitVertex();
  }
  
}

/*-----------------------------------------------------------------------
  Copyright (c) 2014, NVIDIA. All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:
   * Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
   * Neither the name of its contributors may be used to endorse 
     or promote products derived from this software without specific
     prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
  EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
  PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
  OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
-----------------------------------------------------------------------*/