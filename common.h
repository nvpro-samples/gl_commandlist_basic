
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