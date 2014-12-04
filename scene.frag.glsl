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