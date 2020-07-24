/* Copyright (c) 2014-2018, NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* Contact ckubisch@nvidia.com (Christoph Kubisch) for feedback */

#define DEBUG_FILTER     1

#define USE_PROGRAM_FILTER        1
#define ALLOW_EMULATION_LAYER     1

#include <nvgl/extensions_gl.hpp>

#include <imgui/imgui_helper.h>
#include <imgui/imgui_impl_gl.h>

#include <nvmath/nvmath_glsltypes.h>
#include <nvgl/glsltypes_gl.hpp>

#include <nvh/geometry.hpp>
#include <nvh/misc.hpp>
#include <nvh/cameracontrol.hpp>

#include <nvgl/appwindowprofiler_gl.hpp>
#include <nvgl/error_gl.hpp>
#include <nvgl/programmanager_gl.hpp>
#include <nvgl/base_gl.hpp>

#include "nvtoken.hpp"
#include "common.h"

using namespace nvtoken;

namespace basiccmdlist
{
  int const SAMPLE_SIZE_WIDTH(800);
  int const SAMPLE_SIZE_HEIGHT(600);
  int const SAMPLE_MAJOR_VERSION(4);
  int const SAMPLE_MINOR_VERSION(5);


  static const int numObjects = 1024;
  static const int grid = 64;
  static const float globalscale = 8.0f;

  class Sample : public nvgl::AppWindowProfilerGL
  {

    enum DrawMode {
      DRAW_STANDARD,
      DRAW_TOKEN_EMULATED,
      DRAW_TOKEN_BUFFER,
      DRAW_TOKEN_LIST,
    };

    struct {
      nvgl::ProgramID
        draw_scene,
        draw_scene_geo;
    } programs;

    struct {
      
      GLuint   scene_color = 0;
      GLuint   scene_depthstencil = 0;
      GLuint   color = 0;
    }textures;

    struct {
      GLuint64
        scene_color,
        scene_depthstencil,
        color;
    }texturesADDR;

    struct {
      GLuint   scene = 0;
    }fbos;

    struct {
      GLuint   box_vbo = 0;
      GLuint   box_ibo = 0;
      GLuint   sphere_vbo = 0;
      GLuint   sphere_ibo = 0;
      
      GLuint   scene_ubo = 0;
      GLuint   objects_ubo = 0;
    } buffers;

    struct {
      GLuint64  
        box_vbo,
        box_ibo,
        sphere_vbo,
        sphere_ibo,

        scene_ubo,
        objects_ubo;
    } buffersADDR;

    struct Vertex {

      Vertex(const nvh::geometry::Vertex& vertex){
        position  = vertex.position;
        normal[0] = short(vertex.normal.x * float(32767));
        normal[1] = short(vertex.normal.y * float(32767));
        normal[2] = short(vertex.normal.z * float(32767));
        uv        = vec2(vertex.texcoord);
      }

      nvmath::vec4     position;
      short             normal[4];
      nvmath::vec2     uv;
    };

    struct ObjectInfo {
      GLuint    vbo;
      GLuint    ibo;
      GLuint64  vboADDR;
      GLuint64  iboADDR;
      GLuint    numIndices;
      nvgl::ProgramID    program;
    };

    // COMMANDLIST
    struct StateChangeID {
      uint  programChangeID;
      uint  fboChangeID;

      bool operator ==(const StateChangeID& other) const
      {
        return memcmp(this,&other,sizeof(StateChangeID)) == 0;
      }

      bool operator !=(const StateChangeID& other) const
      {
        return memcmp(this,&other,sizeof(StateChangeID)) != 0;
      }

      StateChangeID()
        : programChangeID(0)
        , fboChangeID(0)
      {

      }
    };
    struct CmdList {
      // we introduce variables that track when we changed global state
      StateChangeID  state;
      StateChangeID  captured;

      // two state objects
      GLuint                stateobj_draw;
      GLuint                stateobj_draw_geo;

#if ALLOW_EMULATION_LAYER
      // for emulation
      StateSystem       statesystem;
      StateSystem::StateID  stateid_draw;
      StateSystem::StateID  stateid_draw_geo;
#endif

      // there is multiple ways to draw the scene
      // either via buffer, cmdlist object, or emulation
      GLuint          tokenBuffer;
      GLuint          tokenCmdList;
      std::string     tokenData;
      nvtoken::NVTokenSequence tokenSequence;
      nvtoken::NVTokenSequence tokenSequenceList;
      nvtoken::NVTokenSequence tokenSequenceEmu;
    } cmdlist;

    struct Tweak {
      DrawMode    mode = DRAW_STANDARD;
      vec3        lightDir;
      float       animate = 1.0f;
    };

    nvgl::ProgramManager    m_progManager;

    ImGuiH::Registry        m_ui;
    double                  m_uiTime;

    Tweak                   m_tweak;

    std::vector<ObjectInfo> m_sceneObjects;
    SceneData               m_sceneUbo;

    bool      m_bindlessVboUbo;
    bool      m_hwsupport;
 
    nvh::CameraControl   m_control;

    bool begin();
    void processUI(double time);
    void think(double time);
    void resize(int width, int height);

    bool initProgram();
    bool initFramebuffers(int width, int height);
    bool initScene();

#if ALLOW_EMULATION_LAYER
    bool  initCommandList();
    void  updateCommandListState();
#else
    bool  initCommandListMinimal();
    void  updateCommandListStateMinimal();
#endif
    void  drawStandard();
    void  drawTokenBuffer();
    void  drawTokenList();
#if ALLOW_EMULATION_LAYER
    void  drawTokenEmulation();
#endif
    

    void end() {
      ImGui::ShutdownGL();
    }
    // return true to prevent m_windowState updates
    bool mouse_pos(int x, int y) {
      return ImGuiH::mouse_pos(x, y);
    }
    bool mouse_button(int button, int action) {
      return ImGuiH::mouse_button(button, action);
    }
    bool mouse_wheel(int wheel) {
      return ImGuiH::mouse_wheel(wheel);
    }
    bool key_char(int button) {
      return ImGuiH::key_char(button);
    }
    bool key_button(int button, int action, int mods) {
      return ImGuiH::key_button(button, action, mods);
    }

  public:
    Sample()
    {
      m_parameterList.add("drawmode", (uint32_t*)&m_tweak.mode);
      m_parameterList.add("animate", &m_tweak.animate);
    }

  };

  bool Sample::initProgram()
  {
    bool validated(true);
    m_progManager.m_filetype = nvh::ShaderFileManager::FILETYPE_GLSL;
    m_progManager.addDirectory(std::string(PROJECT_NAME));
    m_progManager.addDirectory(std::string("GLSL_" PROJECT_NAME));
    m_progManager.addDirectory(exePath() + std::string(PROJECT_RELDIRECTORY));

    m_progManager.registerInclude("common.h");

    programs.draw_scene = m_progManager.createProgram(
      ProgramManager::Definition(GL_VERTEX_SHADER,          "scene.vert.glsl"),
      ProgramManager::Definition(GL_FRAGMENT_SHADER,        "scene.frag.glsl"));

    programs.draw_scene_geo = m_progManager.createProgram(
      ProgramManager::Definition(GL_VERTEX_SHADER,          "scene.vert.glsl"),
      ProgramManager::Definition(GL_GEOMETRY_SHADER,        "scene.geo.glsl"),
      ProgramManager::Definition(GL_FRAGMENT_SHADER,        "scene.frag.glsl"));

    cmdlist.state.programChangeID++;
    
    validated = m_progManager.areProgramsValid();

    return validated;
  }

  bool Sample::initFramebuffers(int width, int height)
  {
    if (textures.scene_color && has_GL_ARB_bindless_texture){
      glMakeTextureHandleNonResidentARB(texturesADDR.scene_color);
      glMakeTextureHandleNonResidentARB(texturesADDR.scene_depthstencil);
    }

    newTexture(textures.scene_color, GL_TEXTURE_2D);
    glBindTexture (GL_TEXTURE_2D, textures.scene_color);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, width, height);

    newTexture(textures.scene_depthstencil, GL_TEXTURE_2D);
    glBindTexture (GL_TEXTURE_2D, textures.scene_depthstencil);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH24_STENCIL8, width, height);
    glBindTexture (GL_TEXTURE_2D, 0);

    newFramebuffer(fbos.scene);
    glBindFramebuffer(GL_FRAMEBUFFER,     fbos.scene);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,        GL_TEXTURE_2D, textures.scene_color, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, textures.scene_depthstencil, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // COMMANDLIST
    // As stateobjects and cmdlist objects make references to texture memory, you need ensure residency,
    // as well as rebuilding state objects as FBO memory changes.

    if (has_GL_ARB_bindless_texture){
      texturesADDR.scene_color        = glGetTextureHandleARB(textures.scene_color);
      texturesADDR.scene_depthstencil = glGetTextureHandleARB(textures.scene_depthstencil);
      glMakeTextureHandleResidentARB(texturesADDR.scene_color);
      glMakeTextureHandleResidentARB(texturesADDR.scene_depthstencil);
    }

    cmdlist.state.fboChangeID++;

    return true;
  }

  bool Sample::initScene()
  {
    srand(1238);

    {
      // pattern texture
      int size = 32;
      std::vector<nvmath::vector4<unsigned char> >  texels;
      texels.resize(size * size);

      for (int y = 0; y < size; y++){
        for (int x = 0; x < size; x++){
          int pos = x + y * size;
          nvmath::vector4<unsigned char> texel;

          texel[0] = (( x + y ^ 127 ) & 15) * 17;
          texel[1] = (( x + y ^ 127 ) & 31) * 8;
          texel[2] = (( x + y ^ 127 ) & 63) * 4;
          texel[3] = 255;

          texels[pos] = texel;
        }
      }

      newTexture(textures.color, GL_TEXTURE_2D);
      glBindTexture   (GL_TEXTURE_2D,textures.color);
      glTexStorage2D  (GL_TEXTURE_2D, nvh::mipMapLevels(size), GL_RGBA8, size,size);
      glTexSubImage2D (GL_TEXTURE_2D,0,0,0,size,size,GL_RGBA,GL_UNSIGNED_BYTE, &texels[0]);
      glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, 8.0f);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glGenerateMipmap(GL_TEXTURE_2D);
      glBindTexture   (GL_TEXTURE_2D,0);

      // this sample requires use of bindless texture
      texturesADDR.color = glGetTextureHandleARB(textures.color);
      glMakeTextureHandleResidentARB(texturesADDR.color);
    }

    { // Scene Geometry

      nvh::geometry::Box<Vertex>     box;
      newBuffer(buffers.box_ibo);
      glNamedBufferStorage(buffers.box_ibo, box.getTriangleIndicesSize(), &box.m_indicesTriangles[0], 0);
      newBuffer(buffers.box_vbo);
      glNamedBufferStorage(buffers.box_vbo, box.getVerticesSize(), &box.m_vertices[0], 0);

      if (m_bindlessVboUbo){
        glGetNamedBufferParameterui64vNV(buffers.box_ibo, GL_BUFFER_GPU_ADDRESS_NV, &buffersADDR.box_ibo);
        glGetNamedBufferParameterui64vNV(buffers.box_vbo, GL_BUFFER_GPU_ADDRESS_NV, &buffersADDR.box_vbo);
        glMakeNamedBufferResidentNV(buffers.box_ibo,GL_READ_ONLY);
        glMakeNamedBufferResidentNV(buffers.box_vbo,GL_READ_ONLY);
      }


      nvh::geometry::Sphere<Vertex>  sphere;
      newBuffer(buffers.sphere_ibo);
      glNamedBufferStorage(buffers.sphere_ibo, sphere.getTriangleIndicesSize(), &sphere.m_indicesTriangles[0], 0);
      newBuffer(buffers.sphere_vbo);
      glNamedBufferStorage(buffers.sphere_vbo, sphere.getVerticesSize(), &sphere.m_vertices[0], 0);

      if (m_bindlessVboUbo){
        glGetNamedBufferParameterui64vNV(buffers.sphere_ibo, GL_BUFFER_GPU_ADDRESS_NV, &buffersADDR.sphere_ibo);
        glGetNamedBufferParameterui64vNV(buffers.sphere_vbo, GL_BUFFER_GPU_ADDRESS_NV, &buffersADDR.sphere_vbo);
        glMakeNamedBufferResidentNV(buffers.sphere_ibo,GL_READ_ONLY);
        glMakeNamedBufferResidentNV(buffers.sphere_vbo,GL_READ_ONLY);
      }


      // Scene objects
      newBuffer(buffers.objects_ubo);
      glBindBuffer(GL_UNIFORM_BUFFER, buffers.objects_ubo);
      glBufferData(GL_UNIFORM_BUFFER, uboAligned(sizeof(ObjectData)) * numObjects, NULL, GL_STATIC_DRAW);
      if (m_bindlessVboUbo){
        glGetNamedBufferParameterui64vNV(buffers.objects_ubo, GL_BUFFER_GPU_ADDRESS_NV, &buffersADDR.objects_ubo);
        glMakeNamedBufferResidentNV(buffers.objects_ubo,GL_READ_ONLY);
      }
      
      m_sceneObjects.reserve(numObjects);
      for (int i = 0; i < numObjects; i++){
        ObjectData  ubodata;

        vec3  pos( nvh::frand()* float(grid), nvh::frand()* float(grid), nvh::frand()* float(grid/2) );

        float scale = globalscale/float(grid);
        scale += (nvh::frand()) * 0.25f;

        pos -=  vec3( grid/2, grid/2, grid/4);
        pos /=  float(grid) / globalscale;

        float angle = nvh::frand() * 180.f;

        ubodata.worldMatrix =  nvmath::translation_mat4(pos) *
          nvmath::scale_mat4(vec3(scale)) *
          nvmath::rotation_mat4_x(angle);
        ubodata.worldMatrixIT = nvmath::transpose(nvmath::invert(ubodata.worldMatrix));
        ubodata.texScale.x = rand() % 2 + 1.0f;
        ubodata.texScale.y = rand() % 2 + 1.0f;
        ubodata.color      = vec4(nvh::frand(),nvh::frand(),nvh::frand(),1.0f);

        ubodata.texColor   = texturesADDR.color; // bindless texture used

        glBufferSubData(GL_UNIFORM_BUFFER, uboAligned(sizeof(ObjectData))*i, sizeof(ObjectData), &ubodata);

        ObjectInfo  info;
        info.program = pos.x < 0 ? programs.draw_scene_geo : programs.draw_scene;

        if (rand()%2){
          info.ibo = buffers.sphere_ibo;
          info.vbo = buffers.sphere_vbo;
          info.iboADDR = buffersADDR.sphere_ibo;
          info.vboADDR = buffersADDR.sphere_vbo;
          info.numIndices = sphere.getTriangleIndicesCount();
        }
        else{
          info.ibo = buffers.box_ibo;
          info.vbo = buffers.box_vbo;
          info.iboADDR = buffersADDR.box_ibo;
          info.vboADDR = buffersADDR.box_vbo;
          info.numIndices = box.getTriangleIndicesCount();
        }
        
        m_sceneObjects.push_back(info);
      }

      glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }

    { // Scene UBO
      newBuffer(buffers.scene_ubo);
      glBindBuffer(GL_UNIFORM_BUFFER, buffers.scene_ubo);
      glBufferData(GL_UNIFORM_BUFFER, sizeof(SceneData), NULL, GL_DYNAMIC_DRAW);
      glBindBuffer(GL_UNIFORM_BUFFER, 0);
      if (m_bindlessVboUbo){
        glGetNamedBufferParameterui64vNV(buffers.scene_ubo, GL_BUFFER_GPU_ADDRESS_NV, &buffersADDR.scene_ubo);
        glMakeNamedBufferResidentNV(buffers.scene_ubo,GL_READ_ONLY);
      }
    }


    return true;
  }

  NV_INLINE GLuint getAddressLo(GLuint64 address){
    return GLuint(address & 0xFFFFFFFF);
  }
  NV_INLINE GLuint getAddressHi(GLuint64 address){
    return GLuint(address >> 32);
  }

#if !ALLOW_EMULATION_LAYER

  bool Sample::initCommandListMinimal()
  {
    m_hwsupport = init_NV_command_list(NVPSystem::GetProcAddressGL) ? true : false;
    if (!m_hwsupport) return true;

    glCreateStatesNV(1,&cmdlist.stateobj_draw);
    glCreateStatesNV(1,&cmdlist.stateobj_draw_geo);

    glCreateBuffers(1,&cmdlist.tokenBuffer);
    glCreateCommandListsNV(1,&cmdlist.tokenCmdList);

    GLenum    headerUbo   = glGetCommandHeaderNV( GL_UNIFORM_ADDRESS_COMMAND_NV,    sizeof(UniformAddressCommandNV) );
    GLenum    headerVbo   = glGetCommandHeaderNV( GL_ATTRIBUTE_ADDRESS_COMMAND_NV,  sizeof(AttributeAddressCommandNV) );
    GLenum    headerIbo   = glGetCommandHeaderNV( GL_ELEMENT_ADDRESS_COMMAND_NV,    sizeof(ElementAddressCommandNV) );
    GLenum    headerDraw  = glGetCommandHeaderNV( GL_DRAW_ELEMENTS_COMMAND_NV,      sizeof(DrawElementsCommandNV) );

    GLushort  stageVertex    = glGetStageIndexNV(GL_VERTEX_SHADER);
    GLushort  stageFragment  = glGetStageIndexNV(GL_FRAGMENT_SHADER);
    GLushort  stageGeometry  = glGetStageIndexNV(GL_GEOMETRY_SHADER);


    // create actual token stream from our scene
    {
      NVTokenSequence& seq = cmdlist.tokenSequence;
      std::string& stream  = cmdlist.tokenData;

      size_t offset = 0;

      // at first we bind the scene ubo to all used stages
      {
        UniformAddressCommandNV ubo;
        ubo.header    = headerUbo;
        ubo.index     = UBO_SCENE;
        ubo.addressLo = getAddressLo(buffersADDR.scene_ubo);
        ubo.addressHi = getAddressHi(buffersADDR.scene_ubo);

        ubo.stage     = stageVertex;
        nvtokenEnqueue(stream, ubo);
        ubo.stage     = stageGeometry;
        nvtokenEnqueue(stream, ubo);
        ubo.stage     = stageFragment;
        nvtokenEnqueue(stream, ubo);
      }

      // then we iterate over all objects in our scene
      GLuint lastStateobj = 0;
      for (size_t i = 0; i < m_sceneObjects.size(); i++){
        const ObjectInfo& obj = m_sceneObjects[i];

        GLuint usedStateobj = obj.program == programs.draw_scene ? cmdlist.stateobj_draw : cmdlist.stateobj_draw_geo;

        if (lastStateobj != 0 && (usedStateobj != lastStateobj || !USE_PROGRAM_FILTER)){
          // Whenever our program changes a new stateobject is required,
          // hence the current sequence gets appended
          seq.offsets.push_back(offset);
          seq.sizes.push_back(GLsizei(stream.size() - offset));
          seq.states.push_back(lastStateobj);

          // By passing the fbo here, it means we can render objects
          // even as the fbos get resized (and their textures changed).
          // If we would pass 0 it would mean the stateobject's fbo was used
          // which means on fbo resizes we would have to recreate all stateobjects.
          seq.fbos.push_back( fbos.scene );  


          // new sequence start
          offset = stream.size();
        }

        AttributeAddressCommandNV vbo;
        vbo.header    = headerVbo;
        vbo.index     = 0;
        vbo.addressLo = getAddressLo(obj.vboADDR);
        vbo.addressHi = getAddressHi(obj.vboADDR);
        nvtokenEnqueue(stream, vbo);

        ElementAddressCommandNV ibo;
        ibo.header    = headerIbo;
        ibo.typeSizeInByte = 4;
        ibo.addressLo = getAddressLo(obj.iboADDR);
        ibo.addressHi = getAddressHi(obj.iboADDR);
        nvtokenEnqueue(stream, ibo);

        UniformAddressCommandNV ubo;
        ubo.header = headerUbo;
        ubo.index  = UBO_OBJECT;
        ubo.addressLo = getAddressLo(buffersADDR.objects_ubo + GLuint(uboAligned(sizeof(ObjectData))*i));
        ubo.addressHi = getAddressHi(buffersADDR.objects_ubo + GLuint(uboAligned(sizeof(ObjectData))*i));

        ubo.stage  = stageVertex;
        nvtokenEnqueue(stream, ubo);
        ubo.stage  = stageFragment;
        nvtokenEnqueue(stream, ubo);

        if (usedStateobj == cmdlist.stateobj_draw_geo){
          // also add for geometry stage
          ubo.stage  = stageGeometry;
          nvtokenEnqueue(stream, ubo);
        }

        DrawElementsCommandNV  draw;
        draw.header = headerDraw;
        draw.baseVertex = 0;
        draw.firstIndex = 0;
        draw.count = obj.numIndices;
        nvtokenEnqueue(stream, draw);

        lastStateobj = usedStateobj;
      }

      seq.offsets.push_back(offset);
      seq.sizes.push_back(GLsizei(stream.size() - offset));
      seq.fbos.push_back(fbos.scene );
      seq.states.push_back(lastStateobj);
    }

    // upload the tokens once, so we can reuse them efficiently
    glNamedBufferStorage(cmdlist.tokenBuffer, cmdlist.tokenData.size(), &cmdlist.tokenData[0], 0);

    // for list generation convert offsets to pointers
    cmdlist.tokenSequenceList = cmdlist.tokenSequence;
    for (size_t i = 0; i < cmdlist.tokenSequenceList.offsets.size(); i++){
      cmdlist.tokenSequenceList.offsets[i] += (GLintptr)&cmdlist.tokenData[0];
    }

    updateCommandListStateMinimal();

    return true;
  }


  void Sample::updateCommandListStateMinimal()
  {

    if (cmdlist.state.programChangeID != cmdlist.captured.programChangeID)
    {
      // generic state shared by both programs
      glBindFramebuffer(GL_FRAMEBUFFER, fbos.scene);

      glEnable(GL_DEPTH_TEST);
      glEnable(GL_CULL_FACE);

      glEnableVertexAttribArray(VERTEX_POS);
      glEnableVertexAttribArray(VERTEX_NORMAL);
      glEnableVertexAttribArray(VERTEX_UV);

      glVertexAttribFormat(VERTEX_POS,    3, GL_FLOAT, GL_FALSE,  offsetof(Vertex,position));
      glVertexAttribFormat(VERTEX_NORMAL, 3, GL_SHORT, GL_TRUE,   offsetof(Vertex,normal));
      glVertexAttribFormat(VERTEX_UV,     2, GL_FLOAT, GL_FALSE,  offsetof(Vertex,uv));
      glVertexAttribBinding(VERTEX_POS,   0);
      glVertexAttribBinding(VERTEX_NORMAL,0);
      glVertexAttribBinding(VERTEX_UV,    0);
      // prime the stride parameter, used by bindless VBO and statesystem
      glBindVertexBuffer(0,0,0, sizeof(Vertex));

      glBufferAddressRangeNV(GL_VERTEX_ATTRIB_ARRAY_ADDRESS_NV,0,0,0);
      glBufferAddressRangeNV(GL_ELEMENT_ARRAY_ADDRESS_NV,0,0,0);
      glBufferAddressRangeNV(GL_UNIFORM_BUFFER_ADDRESS_NV,UBO_OBJECT,0,0);
      glBufferAddressRangeNV(GL_UNIFORM_BUFFER_ADDRESS_NV,UBO_SCENE,0,0);

      // let's create the first stateobject
      glUseProgram( m_progManager.get( programs.draw_scene) );
      glStateCaptureNV(cmdlist.stateobj_draw, GL_TRIANGLES);

      // and second
      glUseProgram( m_progManager.get( programs.draw_scene_geo) );
      glStateCaptureNV(cmdlist.stateobj_draw_geo, GL_TRIANGLES);

      glDisableVertexAttribArray(VERTEX_POS);
      glDisableVertexAttribArray(VERTEX_NORMAL);
      glDisableVertexAttribArray(VERTEX_UV);
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      glDisable(GL_DEPTH_TEST);
      glDisable(GL_CULL_FACE);
    }

    if (  cmdlist.state.programChangeID != cmdlist.captured.programChangeID ||
          cmdlist.state.fboChangeID     != cmdlist.captured.fboChangeID)
    {
      // Because the commandlist object takes all state information 
      // from the objects during compile, we have to update commandlist
      // every time a state object or fbo changes.
      NVTokenSequence &seq = cmdlist.tokenSequenceList;
      glCommandListSegmentsNV(cmdlist.tokenCmdList,1);
      glListDrawCommandsStatesClientNV(cmdlist.tokenCmdList,0, (const void**)&seq.offsets[0], &seq.sizes[0], &seq.states[0], &seq.fbos[0], int(seq.states.size()) );
      glCompileCommandListNV(cmdlist.tokenCmdList);
    }

    cmdlist.captured = cmdlist.state;
  }

#else

  bool Sample::initCommandList()
  {
    m_hwsupport = has_GL_NV_command_list ? true : false;
    nvtokenInitInternals(m_hwsupport,m_bindlessVboUbo);
    cmdlist.statesystem.init();

    {
      cmdlist.statesystem.generate(1,&cmdlist.stateid_draw);
      cmdlist.statesystem.generate(1,&cmdlist.stateid_draw_geo);
    }
    if (m_hwsupport){
      glCreateStatesNV(1,&cmdlist.stateobj_draw);
      glCreateStatesNV(1,&cmdlist.stateobj_draw_geo);

      glCreateBuffers(1,&cmdlist.tokenBuffer);
      glCreateCommandListsNV(1,&cmdlist.tokenCmdList);
    }
    else{
      cmdlist.stateobj_draw     = 1;
      cmdlist.stateobj_draw_geo = 2;
    }


    // create actual token stream from our scene
    {
      NVTokenSequence& seq = cmdlist.tokenSequence;
      std::string& stream  = cmdlist.tokenData;

      size_t offset = 0;

      // at first we bind the scene ubo to all used stages
      {
        NVTokenUbo  ubo;
        ubo.setBuffer(buffers.scene_ubo, buffersADDR.scene_ubo, 0, sizeof(SceneData));
        ubo.setBinding(UBO_SCENE, NVTOKEN_STAGE_VERTEX);
        nvtokenEnqueue(stream, ubo);
        ubo.setBinding(UBO_SCENE, NVTOKEN_STAGE_GEOMETRY);
        nvtokenEnqueue(stream, ubo);
        ubo.setBinding(UBO_SCENE, NVTOKEN_STAGE_FRAGMENT);
        nvtokenEnqueue(stream, ubo);
      }

      // then we iterate over all objects in our scene
      GLuint lastStateobj = 0;
      for (size_t i = 0; i < m_sceneObjects.size(); i++){
        const ObjectInfo& obj = m_sceneObjects[i];
        
        GLuint usedStateobj = obj.program == programs.draw_scene ? cmdlist.stateobj_draw : cmdlist.stateobj_draw_geo;

        if (lastStateobj != 0 && (usedStateobj != lastStateobj || !USE_PROGRAM_FILTER)){
          // Whenever our program changes a new stateobject is required,
          // hence the current sequence gets appended
          seq.offsets.push_back(offset);
          seq.sizes.push_back(GLsizei(stream.size() - offset));
          seq.states.push_back(lastStateobj);

          // By passing the fbo here, it means we can render objects
          // even as the fbos get resized (and their textures changed).
          // If we would pass 0 it would mean the stateobject's fbo was used
          // which means on fbo resizes we would have to recreate all stateobjects.
          seq.fbos.push_back( fbos.scene );  


          // new sequence start
          offset = stream.size();
        }

        NVTokenVbo vbo;
        vbo.setBinding(0);
        vbo.setBuffer(obj.vbo, obj.vboADDR,0);
        nvtokenEnqueue(stream, vbo);

        NVTokenIbo ibo;
        ibo.setType(GL_UNSIGNED_INT);
        ibo.setBuffer(obj.ibo, obj.iboADDR);
        nvtokenEnqueue(stream, ibo);

        NVTokenUbo ubo;
        ubo.setBuffer( buffers.objects_ubo, buffersADDR.objects_ubo, GLuint(uboAligned(sizeof(ObjectData))*i), sizeof(ObjectData));
        ubo.setBinding(UBO_OBJECT, NVTOKEN_STAGE_VERTEX );
        nvtokenEnqueue(stream, ubo);
        ubo.setBinding(UBO_OBJECT, NVTOKEN_STAGE_FRAGMENT );
        nvtokenEnqueue(stream, ubo);

        if (usedStateobj == cmdlist.stateobj_draw_geo){
          // also add for geometry stage
          ubo.setBinding(UBO_OBJECT, NVTOKEN_STAGE_GEOMETRY );
          nvtokenEnqueue(stream, ubo);
        }

        NVTokenDrawElems  draw;
        draw.setParams(obj.numIndices );
        // be aware the stateobject's primitive mode must be compatible!
        draw.setMode(GL_TRIANGLES);
        nvtokenEnqueue(stream, draw);

        lastStateobj = usedStateobj;
      }

      seq.offsets.push_back(offset);
      seq.sizes.push_back(GLsizei(stream.size() - offset));
      seq.fbos.push_back(fbos.scene );
      seq.states.push_back(lastStateobj);
    }

    if (m_hwsupport){
      // upload the tokens once, so we can reuse them efficiently
      glNamedBufferStorage(cmdlist.tokenBuffer, cmdlist.tokenData.size(), &cmdlist.tokenData[0], 0);

      // for list generation convert offsets to pointers
      cmdlist.tokenSequenceList = cmdlist.tokenSequence;
      for (size_t i = 0; i < cmdlist.tokenSequenceList.offsets.size(); i++){
        cmdlist.tokenSequenceList.offsets[i] += (GLintptr)&cmdlist.tokenData[0];
      }
    }

    {
      // for emulation we have to convert the stateobject ids to statesystem ids
      cmdlist.tokenSequenceEmu = cmdlist.tokenSequence;
      for (size_t i = 0; i < cmdlist.tokenSequenceEmu.states.size(); i++){
        GLuint oldstate = cmdlist.tokenSequenceEmu.states[i];
        cmdlist.tokenSequenceEmu.states[i] = 
          (oldstate == cmdlist.stateobj_draw) ? cmdlist.stateid_draw : cmdlist.stateid_draw_geo ;
      }
    }

    updateCommandListState();

    return true;
  }

  void Sample::updateCommandListState()
  {
    
    if (cmdlist.state.programChangeID != cmdlist.captured.programChangeID)
    {
      // generic state shared by both programs
      glBindFramebuffer(GL_FRAMEBUFFER, fbos.scene);

      glEnable(GL_DEPTH_TEST);
      glEnable(GL_CULL_FACE);

      glEnableVertexAttribArray(VERTEX_POS);
      glEnableVertexAttribArray(VERTEX_NORMAL);
      glEnableVertexAttribArray(VERTEX_UV);

      glVertexAttribFormat(VERTEX_POS,    3, GL_FLOAT, GL_FALSE,  offsetof(Vertex,position));
      glVertexAttribFormat(VERTEX_NORMAL, 3, GL_SHORT, GL_TRUE,   offsetof(Vertex,normal));
      glVertexAttribFormat(VERTEX_UV,     2, GL_FLOAT, GL_FALSE,  offsetof(Vertex,uv));
      glVertexAttribBinding(VERTEX_POS,   0);
      glVertexAttribBinding(VERTEX_NORMAL,0);
      glVertexAttribBinding(VERTEX_UV,    0);
      // prime the stride parameter, used by bindless VBO and statesystem
      glBindVertexBuffer(0,0,0, sizeof(Vertex));
      
      // temp workaround
      if (m_hwsupport){
        glBufferAddressRangeNV(GL_VERTEX_ATTRIB_ARRAY_ADDRESS_NV,0,0,0);
        glBufferAddressRangeNV(GL_ELEMENT_ARRAY_ADDRESS_NV,0,0,0);
        glBufferAddressRangeNV(GL_UNIFORM_BUFFER_ADDRESS_NV,UBO_OBJECT,0,0);
        glBufferAddressRangeNV(GL_UNIFORM_BUFFER_ADDRESS_NV,UBO_SCENE,0,0);
      }

      // let's create the first stateobject
      glUseProgram( m_progManager.get( programs.draw_scene) );

      if (m_hwsupport){
        glStateCaptureNV(cmdlist.stateobj_draw, GL_TRIANGLES);
      }


      StateSystem::State state;
      state.getGL(); // this is a costly operation
      cmdlist.statesystem.set( cmdlist.stateid_draw, state, GL_TRIANGLES);
      

      // The statesystem also provides an alternative approach
      // to getGL, by manipulating the state data directly.
      // When no getGL is called the state data matches the default
      // state of OpenGL.

      state.program.program = m_progManager.get( programs.draw_scene_geo );
      cmdlist.statesystem.set( cmdlist.stateid_draw_geo, state, GL_TRIANGLES);
      if (m_hwsupport){
        // we can apply the state directly with
        // glUseProgram( state.program.program );
        // or
        // state.applyGL(false,false, 1<<StateSystem::DYNAMIC_VIEWPORT);
        // or more efficiently using the system which will use state diffs
        cmdlist.statesystem.applyGL( cmdlist.stateid_draw_geo, cmdlist.stateid_draw, true);

        glStateCaptureNV(cmdlist.stateobj_draw_geo, GL_TRIANGLES);
      }

      // since we will toggle between two states, let the emulation cache the differences
      cmdlist.statesystem.prepareTransition( cmdlist.stateid_draw, cmdlist.stateid_draw_geo);
      cmdlist.statesystem.prepareTransition( cmdlist.stateid_draw_geo, cmdlist.stateid_draw);

      glDisableVertexAttribArray(VERTEX_POS);
      glDisableVertexAttribArray(VERTEX_NORMAL);
      glDisableVertexAttribArray(VERTEX_UV);
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      glDisable(GL_DEPTH_TEST);
      glDisable(GL_CULL_FACE);
    }

    if (m_hwsupport && (
        cmdlist.state.programChangeID != cmdlist.captured.programChangeID ||
        cmdlist.state.fboChangeID     != cmdlist.captured.fboChangeID))
    {
      // Because the commandlist object takes all state information 
      // from the objects during compile, we have to update commandlist
      // every time a state object or fbo changes.
      NVTokenSequence &seq = cmdlist.tokenSequenceList;
      glCommandListSegmentsNV(cmdlist.tokenCmdList,1);
      glListDrawCommandsStatesClientNV(cmdlist.tokenCmdList,0, (const void**)&seq.offsets[0], &seq.sizes[0], &seq.states[0], &seq.fbos[0], int(seq.states.size()) );
      glCompileCommandListNV(cmdlist.tokenCmdList);
    }

    cmdlist.captured = cmdlist.state;
  }

#endif

  bool Sample::begin()
  {
    ImGuiH::Init(m_windowState.m_winSize[0], m_windowState.m_winSize[1], this);
    ImGui::InitGL();

    if (!has_GL_ARB_bindless_texture){
      LOGE("This sample requires ARB_bindless_texture");
      return false;
    }

    m_bindlessVboUbo = has_GL_NV_vertex_buffer_unified_memory && m_contextWindow.extensionSupported("GL_NV_uniform_buffer_unified_memory");

    bool validated(true);

    GLuint defaultVAO;
    glGenVertexArrays(1, &defaultVAO);
    glBindVertexArray(defaultVAO);

    validated = validated && initProgram();
    validated = validated && initFramebuffers(m_windowState.m_winSize[0],m_windowState.m_winSize[1]);
    validated = validated && initScene();
#if ALLOW_EMULATION_LAYER
    validated = validated && initCommandList();
#else
    validated = validated && initCommandListMinimal();
#endif

    {
      m_ui.enumAdd(0, DRAW_STANDARD, "standard");
#if ALLOW_EMULATION_LAYER
      m_ui.enumAdd(0, DRAW_TOKEN_EMULATED, "nvcmdlist emulated");
#endif
      if (m_hwsupport) {
        m_ui.enumAdd(0, DRAW_TOKEN_BUFFER, "nvcmdlist buffer");
        m_ui.enumAdd(0, DRAW_TOKEN_LIST, "nvcmdlist list");
      }
    }

    m_tweak.lightDir = normalize(vec3(-1,1,1));

    m_control.m_sceneOrbit      = vec3(0.0f);
    m_control.m_sceneDimension  = float(grid) * 0.2f;
    m_control.m_viewMatrix      = nvmath::look_at(m_control.m_sceneOrbit - vec3(0,0,-m_control.m_sceneDimension), m_control.m_sceneOrbit, vec3(0,1,0));

    m_sceneUbo.shrinkFactor = 0.5f;

    return validated;
  }

  void Sample::processUI(double time)
  {

    int width = m_windowState.m_winSize[0];
    int height = m_windowState.m_winSize[1];

    // Update imgui configuration
    auto &imgui_io = ImGui::GetIO();
    imgui_io.DeltaTime = static_cast<float>(time - m_uiTime);
    imgui_io.DisplaySize = ImVec2(width, height);

    m_uiTime = time;

    ImGui::NewFrame();
    ImGui::SetNextWindowSize(ImVec2(350, 0), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("NVIDIA " PROJECT_NAME, nullptr)) {
      m_ui.enumCombobox(0, "draw mode", &m_tweak.mode);
      ImGui::SliderFloat("shrink factor", &m_sceneUbo.shrinkFactor, 0, 1.0f);
    }
    ImGui::End();
  }

  void Sample::think(double time)
  {
    NV_PROFILE_GL_SECTION("Frame");

    processUI(time);

    m_control.processActions(m_windowState.m_winSize,
      nvmath::vec2f(m_windowState.m_mouseCurrent[0],m_windowState.m_mouseCurrent[1]),
      m_windowState.m_mouseButtonFlags, m_windowState.m_mouseWheel);

    int width   = m_windowState.m_winSize[0];
    int height  = m_windowState.m_winSize[1];

    if (m_windowState.onPress(KEY_R)){
      m_progManager.reloadPrograms();
      cmdlist.state.programChangeID++;
    }
    if (!m_progManager.areProgramsValid()){
      waitEvents();
      return;
    }

    {
      NV_PROFILE_GL_SECTION("Setup");
      m_sceneUbo.viewport = uvec2(width,height);

      nvmath::mat4 projection = nvmath::perspective(45.f, float(width)/float(height), 0.1f, 1000.0f);
      nvmath::mat4 view = m_control.m_viewMatrix;

      m_sceneUbo.viewProjMatrix = projection * view;
      m_sceneUbo.viewProjMatrixI = nvmath::invert(m_sceneUbo.viewProjMatrix);
      m_sceneUbo.viewMatrix = view;
      m_sceneUbo.viewMatrixI = nvmath::invert(view);
      m_sceneUbo.viewMatrixIT = nvmath::transpose(m_sceneUbo.viewMatrixI );
      m_sceneUbo.wLightPos = vec4(m_tweak.lightDir * float(grid),1.0f);
      m_sceneUbo.time = float(time) * m_tweak.animate;

      glNamedBufferSubData(buffers.scene_ubo,0,sizeof(SceneData),&m_sceneUbo);

      glBindFramebuffer(GL_FRAMEBUFFER, fbos.scene);
      glViewport(0, 0, width, height);

      nvmath::vec4   bgColor(0.2,0.2,0.2,0.0);
      glClearColor(bgColor.x,bgColor.y,bgColor.z,bgColor.w);
      glClearDepth(1.0);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    }

    {
      NV_PROFILE_GL_SECTION("Draw");

      switch(m_tweak.mode){
      case DRAW_STANDARD:
        drawStandard();
        break;
#if ALLOW_EMULATION_LAYER
      case DRAW_TOKEN_EMULATED:
        drawTokenEmulation();
        break;
#endif
      case DRAW_TOKEN_BUFFER:
        drawTokenBuffer();
        break;
      case DRAW_TOKEN_LIST:
        drawTokenList();
        break;
      }
    }

    {
      NV_PROFILE_GL_SECTION("Blit");
      glBindFramebuffer(GL_READ_FRAMEBUFFER, fbos.scene);
      glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
      glBlitFramebuffer(0,0,width,height,
        0,0,width,height,GL_COLOR_BUFFER_BIT, GL_NEAREST);
    }

    {
      NV_PROFILE_GL_SECTION("GUI");
      ImGui::Render();
      ImGui::RenderDrawDataGL(ImGui::GetDrawData());
    }

    ImGui::EndFrame();
  }

  void Sample::resize(int width, int height)
  {
    initFramebuffers(width,height);
  }

  void Sample::drawStandard()
  {
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    glVertexAttribFormat(VERTEX_POS,    3, GL_FLOAT, GL_FALSE,  offsetof(Vertex,position));
    glVertexAttribFormat(VERTEX_NORMAL, 3, GL_SHORT, GL_TRUE,   offsetof(Vertex,normal));
    glVertexAttribFormat(VERTEX_UV,     2, GL_FLOAT, GL_FALSE,  offsetof(Vertex,uv));
    glVertexAttribBinding(VERTEX_POS,   0);
    glVertexAttribBinding(VERTEX_NORMAL,0);
    glVertexAttribBinding(VERTEX_UV,    0);

    glEnableVertexAttribArray(VERTEX_POS);
    glEnableVertexAttribArray(VERTEX_NORMAL);
    glEnableVertexAttribArray(VERTEX_UV);

    glBindBufferBase(GL_UNIFORM_BUFFER, UBO_SCENE,  buffers.scene_ubo);

    GLuint lastProg = 0;
    for (int i = 0; i < m_sceneObjects.size(); i++){
      const ObjectInfo&  obj = m_sceneObjects[i];
      GLuint usedProg = m_progManager.get( obj.program);

      if (usedProg != lastProg || !USE_PROGRAM_FILTER){
        // simple redundancy tracker
        glUseProgram ( usedProg );
        lastProg = usedProg;
      }
      
      glBindBufferRange(GL_UNIFORM_BUFFER, UBO_OBJECT, buffers.objects_ubo, uboAligned(sizeof(ObjectData))*i, sizeof(ObjectData));

      glBindVertexBuffer(0,obj.vbo,0,sizeof(Vertex));
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, obj.ibo);
      glDrawElements(GL_TRIANGLES, obj.numIndices, GL_UNSIGNED_INT, NV_BUFFER_OFFSET(0));
    }

    glDisableVertexAttribArray(VERTEX_POS);
    glDisableVertexAttribArray(VERTEX_NORMAL);
    glDisableVertexAttribArray(VERTEX_UV);

    glBindBufferBase(GL_UNIFORM_BUFFER, UBO_SCENE, 0);
    glBindBufferBase(GL_UNIFORM_BUFFER, UBO_OBJECT, 0);
    glBindVertexBuffer(0,0,0,0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  }

  void Sample::drawTokenBuffer()
  {
    if ( cmdlist.state != cmdlist.captured ){
      #if ALLOW_EMULATION_LAYER
        updateCommandListState();
      #else
        updateCommandListStateMinimal();
      #endif
    }

    glDrawCommandsStatesNV(cmdlist.tokenBuffer, 
      &cmdlist.tokenSequence.offsets[0],
      &cmdlist.tokenSequence.sizes[0],
      &cmdlist.tokenSequence.states[0],
      &cmdlist.tokenSequence.fbos[0],
      GLuint(cmdlist.tokenSequence.offsets.size()));
  }

  void Sample::drawTokenList()
  {
    if ( cmdlist.state != cmdlist.captured ){
#if ALLOW_EMULATION_LAYER
      updateCommandListState();
#else
      updateCommandListStateMinimal();
#endif
    }

    glCallCommandListNV(cmdlist.tokenCmdList);

  }
#if ALLOW_EMULATION_LAYER
  void Sample::drawTokenEmulation()
  {
    if (m_bindlessVboUbo){
      glEnableClientState(GL_VERTEX_ATTRIB_ARRAY_UNIFIED_NV);
      glEnableClientState(GL_ELEMENT_ARRAY_UNIFIED_NV);
      glEnableClientState(GL_UNIFORM_BUFFER_UNIFIED_NV);
#if _DEBUG
      // we are using what is considered "unsafe" addresses which will throw tons of warnings
      GLuint msgid = 65537;
      glDebugMessageControlARB(GL_DEBUG_SOURCE_API, GL_DEBUG_TYPE_OTHER, GL_DONT_CARE, 1, &msgid, GL_FALSE);
#endif
    }

    if ( cmdlist.state != cmdlist.captured ){
      updateCommandListState();
    }

    nvtokenDrawCommandsStatesSW(&cmdlist.tokenData[0], cmdlist.tokenData.size(), 
      &cmdlist.tokenSequenceEmu.offsets[0],
      &cmdlist.tokenSequenceEmu.sizes[0],
      &cmdlist.tokenSequenceEmu.states[0],
      &cmdlist.tokenSequenceEmu.fbos[0],
      GLuint(cmdlist.tokenSequenceEmu.offsets.size()),
      cmdlist.statesystem);

    if (m_bindlessVboUbo){
      glDisableClientState(GL_VERTEX_ATTRIB_ARRAY_UNIFIED_NV);
      glDisableClientState(GL_ELEMENT_ARRAY_UNIFIED_NV);
      glDisableClientState(GL_UNIFORM_BUFFER_UNIFIED_NV);

#if _DEBUG
      glDebugMessageControlARB(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
#endif
    }
  }
#endif
}

using namespace basiccmdlist;

int main(int argc, const char** argv)
{
  NVPSystem system(argv[0], PROJECT_NAME);

  Sample sample;
  return sample.run(
    PROJECT_NAME,
    argc, argv,
    SAMPLE_SIZE_WIDTH, SAMPLE_SIZE_HEIGHT);
}

