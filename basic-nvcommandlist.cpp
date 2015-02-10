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
/* Contact ckubisch@nvidia.com (Christoph Kubisch) for feedback */

#define DEBUG_FILTER     1

#define USE_PROGRAM_FILTER  1

#include <GL/glew.h>
#include <nv_helpers/anttweakbar.hpp>
#include <nv_helpers_gl/WindowProfiler.hpp>
#include <nv_helpers_gl/glsltypes.hpp>

#include <nv_helpers_gl/error.hpp>
#include <nv_helpers_gl/programmanager.hpp>
#include <nv_helpers/geometry.hpp>
#include <nv_helpers/misc.hpp>
#include <nv_helpers_gl/glresources.hpp>
#include <nv_helpers/cameracontrol.hpp>

#include "nvtoken.hpp"
using namespace nvtoken;

using namespace nv_helpers;
using namespace nv_helpers_gl;
using namespace nv_math;
#include "common.h"


namespace basiccmdlist
{
  int const SAMPLE_SIZE_WIDTH(800);
  int const SAMPLE_SIZE_HEIGHT(600);
  int const SAMPLE_MAJOR_VERSION(4);
  int const SAMPLE_MINOR_VERSION(3);


  static const int numObjects = 1024;
  static const int grid = 64;
  static const float globalscale = 8.0f;

  class Sample : public nv_helpers_gl::WindowProfiler 
  {
    ProgramManager progManager;

    enum DrawMode {
      DRAW_STANDARD,
      DRAW_TOKEN_EMULATED,
      DRAW_TOKEN_BUFFER,
      DRAW_TOKEN_LIST,
    };

    struct {
      ProgramManager::ProgramID
        draw_scene,
        draw_scene_geo;
    } programs;

    struct {
      ResourceGLuint
        scene_color,
        scene_depthstencil,
        color;
    }textures;

    struct {
      GLuint64
        scene_color,
        scene_depthstencil,
        color;
    }texturesADDR;

    struct {
      ResourceGLuint
        scene;
    }fbos;

    struct {
      ResourceGLuint  
        box_vbo,
        box_ibo,
        sphere_vbo,
        sphere_ibo,

        scene_ubo,
        objects_ubo;
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

      Vertex(const geometry::Vertex& vertex){
        position  = vertex.position;
        normal[0] = short(vertex.normal.x * float(32767));
        normal[1] = short(vertex.normal.y * float(32767));
        normal[2] = short(vertex.normal.z * float(32767));
        uv        = vec2(vertex.texcoord);
      }

      nv_math::vec4     position;
      short             normal[4];
      nv_math::vec2     uv;
    };

    struct ObjectInfo {
      GLuint    vbo;
      GLuint    ibo;
      GLuint64  vboADDR;
      GLuint64  iboADDR;
      GLuint    numIndices;
      ProgramManager::ProgramID    program;
    };

    // COMMANDLIST
    struct StateIncarnation {
      uint  programIncarnation;
      uint  fboIncarnation;

      bool operator ==(const StateIncarnation& other) const
      {
        return memcmp(this,&other,sizeof(StateIncarnation)) == 0;
      }

      bool operator !=(const StateIncarnation& other) const
      {
        return memcmp(this,&other,sizeof(StateIncarnation)) != 0;
      }

      StateIncarnation()
        : programIncarnation(0)
        , fboIncarnation(0)
      {

      }
    };
    struct CmdList {
      // for emulation
      StateSystem       statesystem;
      
      // we introduce variables that track when we changed global state
      StateIncarnation  state;
      StateIncarnation  captured;

      // two state objects
      GLuint                stateobj_draw;
      GLuint                stateobj_draw_geo;
      StateSystem::StateID  stateid_draw;
      StateSystem::StateID  stateid_draw_geo;


      // there is multiple ways to draw the scene
      // either via buffer, cmdlist object, or emulation
      GLuint          tokenBuffer;
      GLuint          tokenCmdList;
      std::string     tokenData;
      NVTokenSequence tokenSequence;
      NVTokenSequence tokenSequenceList;
      NVTokenSequence tokenSequenceEmu;
    } cmdlist;

    struct Tweak {
      DrawMode    mode;
      vec3        lightDir;

      Tweak() 
        : mode(DRAW_STANDARD)
      {

      }
    };

    Tweak    tweak;

    std::vector<ObjectInfo> objects;

    SceneData sceneUbo;
    bool      bindlessVboUbo;
    bool      hwsupport;
 

    bool begin();
    void think(double time);
    void resize(int width, int height);

    bool initProgram();
    bool initFramebuffers(int width, int height);
    bool initScene();

    bool  initCommandList();
    void  updateCommandListState();

    void  drawStandard();
    void  drawTokenBuffer();
    void  drawTokenList();
    void  drawTokenEmulation();

    CameraControl m_control;

    void end() {
      TwTerminate();
    }
    // return true to prevent m_window updates
    bool mouse_pos    (int x, int y) {
      return !!TwEventMousePosGLFW(x,y); 
    }
    bool mouse_button (int button, int action) {
      return !!TwEventMouseButtonGLFW(button, action);
    }
    bool mouse_wheel  (int wheel) {
      return !!TwEventMouseWheelGLFW(wheel); 
    }
    bool key_button   (int button, int action, int mods) {
      return handleTwKeyPressed(button,action,mods);
    }

  };

  bool Sample::initProgram()
  {
    bool validated(true);
    progManager.addDirectory( std::string(PROJECT_NAME));
    progManager.addDirectory( sysExePath() + std::string(PROJECT_RELDIRECTORY));
    progManager.addDirectory( std::string(PROJECT_ABSDIRECTORY));

    progManager.registerInclude("common.h", "common.h");

    programs.draw_scene = progManager.createProgram(
      ProgramManager::Definition(GL_VERTEX_SHADER,          "scene.vert.glsl"),
      ProgramManager::Definition(GL_FRAGMENT_SHADER,        "scene.frag.glsl"));

    programs.draw_scene_geo = progManager.createProgram(
      ProgramManager::Definition(GL_VERTEX_SHADER,          "scene.vert.glsl"),
      ProgramManager::Definition(GL_GEOMETRY_SHADER,        "scene.geo.glsl"),
      ProgramManager::Definition(GL_FRAGMENT_SHADER,        "scene.frag.glsl"));

    cmdlist.state.programIncarnation++;
    
    validated = progManager.areProgramsValid();

    return validated;
  }

  bool Sample::initFramebuffers(int width, int height)
  {
    if (textures.scene_color && GLEW_ARB_bindless_texture){
      glMakeTextureHandleNonResidentARB(texturesADDR.scene_color);
      glMakeTextureHandleNonResidentARB(texturesADDR.scene_depthstencil);
    }

    newTexture(textures.scene_color);
    glBindTexture (GL_TEXTURE_2D, textures.scene_color);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, width, height);

    newTexture(textures.scene_depthstencil);
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

    if (GLEW_ARB_bindless_texture){
      texturesADDR.scene_color        = glGetTextureHandleARB(textures.scene_color);
      texturesADDR.scene_depthstencil = glGetTextureHandleARB(textures.scene_depthstencil);
      glMakeTextureHandleResidentARB(texturesADDR.scene_color);
      glMakeTextureHandleResidentARB(texturesADDR.scene_depthstencil);
    }

    cmdlist.state.fboIncarnation++;

    return true;
  }

  bool Sample::initScene()
  {
    srand(1238);

    {
      // pattern texture
      int size = 32;
      std::vector<nv_math::vector4<unsigned char> >  texels;
      texels.resize(size * size);

      for (int y = 0; y < size; y++){
        for (int x = 0; x < size; x++){
          int pos = x + y * size;
          nv_math::vector4<unsigned char> texel;

          texel[0] = (( x + y ^ 127 ) & 15) * 17;
          texel[1] = (( x + y ^ 127 ) & 31) * 8;
          texel[2] = (( x + y ^ 127 ) & 63) * 4;
          texel[3] = 255;

          texels[pos] = texel;
        }
      }

      newTexture(textures.color);
      glBindTexture   (GL_TEXTURE_2D,textures.color);
      glTexStorage2D  (GL_TEXTURE_2D, mipMapLevels(size), GL_RGBA8, size,size);
      glTexSubImage2D (GL_TEXTURE_2D,0,0,0,size,size,GL_RGBA,GL_UNSIGNED_BYTE, &texels[0]);
      glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 8.0f);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glGenerateMipmap(GL_TEXTURE_2D);
      glBindTexture   (GL_TEXTURE_2D,0);

      // this sample requires use of bindless texture
      texturesADDR.color = glGetTextureHandleARB(textures.color);
      glMakeTextureHandleResidentARB(texturesADDR.color);
    }

    { // Scene Geometry

      geometry::Box<Vertex>     box;
      newBuffer(buffers.box_ibo);
      glNamedBufferStorageEXT(buffers.box_ibo, box.getTriangleIndicesSize(), &box.m_indicesTriangles[0], 0);
      newBuffer(buffers.box_vbo);
      glNamedBufferStorageEXT(buffers.box_vbo, box.getVerticesSize(), &box.m_vertices[0], 0);

      if (bindlessVboUbo){
        glGetNamedBufferParameterui64vNV(buffers.box_ibo, GL_BUFFER_GPU_ADDRESS_NV, &buffersADDR.box_ibo);
        glGetNamedBufferParameterui64vNV(buffers.box_vbo, GL_BUFFER_GPU_ADDRESS_NV, &buffersADDR.box_vbo);
        glMakeNamedBufferResidentNV(buffers.box_ibo,GL_READ_ONLY);
        glMakeNamedBufferResidentNV(buffers.box_vbo,GL_READ_ONLY);
      }


      geometry::Sphere<Vertex>  sphere;
      newBuffer(buffers.sphere_ibo);
      glNamedBufferStorageEXT(buffers.sphere_ibo, sphere.getTriangleIndicesSize(), &sphere.m_indicesTriangles[0], 0);
      newBuffer(buffers.sphere_vbo);
      glNamedBufferStorageEXT(buffers.sphere_vbo, sphere.getVerticesSize(), &sphere.m_vertices[0], 0);

      if (bindlessVboUbo){
        glGetNamedBufferParameterui64vNV(buffers.sphere_ibo, GL_BUFFER_GPU_ADDRESS_NV, &buffersADDR.sphere_ibo);
        glGetNamedBufferParameterui64vNV(buffers.sphere_vbo, GL_BUFFER_GPU_ADDRESS_NV, &buffersADDR.sphere_vbo);
        glMakeNamedBufferResidentNV(buffers.sphere_ibo,GL_READ_ONLY);
        glMakeNamedBufferResidentNV(buffers.sphere_vbo,GL_READ_ONLY);
      }


      // Scene objects
      newBuffer(buffers.objects_ubo);
      glBindBuffer(GL_UNIFORM_BUFFER, buffers.objects_ubo);
      glBufferData(GL_UNIFORM_BUFFER, uboAligned(sizeof(ObjectData)) * numObjects, NULL, GL_STATIC_DRAW);
      if (bindlessVboUbo){
        glGetNamedBufferParameterui64vNV(buffers.objects_ubo, GL_BUFFER_GPU_ADDRESS_NV, &buffersADDR.objects_ubo);
        glMakeNamedBufferResidentNV(buffers.objects_ubo,GL_READ_ONLY);
      }
      
      objects.reserve(numObjects);
      for (int i = 0; i < numObjects; i++){
        ObjectData  ubodata;

        vec3  pos( frand()* float(grid), frand()* float(grid), frand()* float(grid/2) );

        float scale = globalscale/float(grid);
        scale += (frand()) * 0.25f;

        pos -=  vec3( grid/2, grid/2, grid/4);
        pos /=  float(grid) / globalscale;

        float angle = frand() * 180.f;

        ubodata.worldMatrix =  nv_math::translation_mat4(pos) *
          nv_math::scale_mat4(vec3(scale)) *
          nv_math::rotation_mat4_x(angle);
        ubodata.worldMatrixIT = nv_math::transpose(nv_math::invert(ubodata.worldMatrix));
        ubodata.texScale.x = rand() % 2 + 1.0f;
        ubodata.texScale.y = rand() % 2 + 1.0f;
        ubodata.color      = vec4(frand(),frand(),frand(),1.0f);

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
        
        objects.push_back(info);
      }

      glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }

    { // Scene UBO
      newBuffer(buffers.scene_ubo);
      glBindBuffer(GL_UNIFORM_BUFFER, buffers.scene_ubo);
      glBufferData(GL_UNIFORM_BUFFER, sizeof(SceneData), NULL, GL_DYNAMIC_DRAW);
      glBindBuffer(GL_UNIFORM_BUFFER, 0);
      if (bindlessVboUbo){
        glGetNamedBufferParameterui64vNV(buffers.scene_ubo, GL_BUFFER_GPU_ADDRESS_NV, &buffersADDR.scene_ubo);
        glMakeNamedBufferResidentNV(buffers.scene_ubo,GL_READ_ONLY);
      }
    }


    return true;
  }


  bool Sample::initCommandList()
  {

    hwsupport = init_NV_command_list(NVPWindow::sysGetProcAddress) ? true : false;
    nvtokenInitInternals(hwsupport,bindlessVboUbo);
    cmdlist.statesystem.init();

    {
      cmdlist.statesystem.generate(1,&cmdlist.stateid_draw);
      cmdlist.statesystem.generate(1,&cmdlist.stateid_draw_geo);
    }
    if (hwsupport){
      glCreateStatesNV(1,&cmdlist.stateobj_draw);
      glCreateStatesNV(1,&cmdlist.stateobj_draw_geo);

      glGenBuffers(1,&cmdlist.tokenBuffer);
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
      for (size_t i = 0; i < objects.size(); i++){
        const ObjectInfo& obj = objects[i];
        
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

    if (hwsupport){
      // upload the tokens once, so we can reuse them efficiently
      glNamedBufferStorageEXT(cmdlist.tokenBuffer, cmdlist.tokenData.size(), &cmdlist.tokenData[0], 0);

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
    
    if (cmdlist.state.programIncarnation != cmdlist.captured.programIncarnation)
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
      if (hwsupport){
        glBufferAddressRangeNV(GL_VERTEX_ATTRIB_ARRAY_ADDRESS_NV,0,0,0);
        glBufferAddressRangeNV(GL_ELEMENT_ARRAY_ADDRESS_NV,0,0,0);
        glBufferAddressRangeNV(GL_UNIFORM_BUFFER_ADDRESS_NV,UBO_OBJECT,0,0);
        glBufferAddressRangeNV(GL_UNIFORM_BUFFER_ADDRESS_NV,UBO_SCENE,0,0);
      }

      // let's create the first stateobject
      glUseProgram( progManager.get( programs.draw_scene) );

      if (hwsupport){
        glStateCaptureNV(cmdlist.stateobj_draw, GL_TRIANGLES);
      }


      StateSystem::State state;
      state.getGL(); // this is a costly operation
      cmdlist.statesystem.set( cmdlist.stateid_draw, state, GL_TRIANGLES);
      

      // The statesystem also provides an alternative approach
      // to getGL, by manipulating the state data directly.
      // When no getGL is called the state data matches the default
      // state of OpenGL.

      state.program.program = progManager.get( programs.draw_scene_geo );
      cmdlist.statesystem.set( cmdlist.stateid_draw_geo, state, GL_TRIANGLES);
      if (hwsupport){
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

    if (hwsupport && (
        cmdlist.state.programIncarnation != cmdlist.captured.programIncarnation ||
        cmdlist.state.fboIncarnation     != cmdlist.captured.fboIncarnation))
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



  bool Sample::begin()
  {
    TwInit(TW_OPENGL_CORE,NULL);
    TwWindowSize(m_window.m_viewsize[0],m_window.m_viewsize[1]);

    if (!GLEW_ARB_bindless_texture){
      nvprintfLevel(LOGLEVEL_ERROR,"This sample requires ARB_bindless_texture");
      return false;
    }

    bindlessVboUbo = GLEW_NV_vertex_buffer_unified_memory && sysExtensionSupported("GL_NV_uniform_buffer_unified_memory");

    bool validated(true);

    GLuint defaultVAO;
    glGenVertexArrays(1, &defaultVAO);
    glBindVertexArray(defaultVAO);

    validated = validated && initProgram();
    validated = validated && initFramebuffers(m_window.m_viewsize[0],m_window.m_viewsize[1]);
    validated = validated && initScene();
    validated = validated && initCommandList();

    TwBar *bar = TwNewBar("mainbar");
    TwDefine(" GLOBAL contained=true help='OpenGL samples.\nCopyright NVIDIA Corporation 2014' ");
    TwDefine(" mainbar position='0 0' size='350 200' color='0 0 0' alpha=128 valueswidth=170 ");
    TwDefine((std::string(" mainbar label='") + PROJECT_NAME + "'").c_str());

    TwEnumVal enumVals[] = {
      {DRAW_STANDARD,"standard"},
      {DRAW_TOKEN_EMULATED,"nvcmdlist emulated"},
      {DRAW_TOKEN_BUFFER,"nvcmdlist buffer"},
      {DRAW_TOKEN_LIST,"nvcmdlist list"},
    };
    TwType algorithmType = TwDefineEnum("algorithm", enumVals, hwsupport ? 4 : 2);
    TwAddVarRW(bar, "mode", algorithmType,  &tweak.mode,     " label='draw mode' ");
    TwAddVarRW(bar, "shrink", TW_TYPE_FLOAT,    &sceneUbo.shrinkFactor,     " label='shrink' max=1 step=0.1 ");
    TwAddVarRW(bar, "lightdir", TW_TYPE_DIR3F,  &tweak.lightDir,     " label='lightdir' ");

    tweak.lightDir = normalize(vec3(-1,1,1));

    m_control.m_sceneOrbit      = vec3(0.0f);
    m_control.m_sceneDimension  = float(grid) * 0.2f;
    m_control.m_viewMatrix      = nv_math::look_at(m_control.m_sceneOrbit - vec3(0,0,-m_control.m_sceneDimension), m_control.m_sceneOrbit, vec3(0,1,0));

    sceneUbo.shrinkFactor = 0.5f;

    return validated;
  }

  void Sample::think(double time)
  {
    m_control.processActions(m_window.m_viewsize,
      nv_math::vec2f(m_window.m_mouseCurrent[0],m_window.m_mouseCurrent[1]),
      m_window.m_mouseButtonFlags, m_window.m_wheel);

    int width   = m_window.m_viewsize[0];
    int height  = m_window.m_viewsize[1];

    if (m_window.onPress(KEY_R)){
      progManager.reloadPrograms();
      cmdlist.state.programIncarnation++;
    }
    if (!progManager.areProgramsValid()){
      waitEvents();
      return;
    }

    {
      NV_PROFILE_SECTION("Setup");
      sceneUbo.viewport = uvec2(width,height);

      nv_math::mat4 projection = nv_math::perspective(45.f, float(width)/float(height), 0.1f, 1000.0f);
      nv_math::mat4 view = m_control.m_viewMatrix;

      sceneUbo.viewProjMatrix = projection * view;
      sceneUbo.viewProjMatrixI = nv_math::invert(sceneUbo.viewProjMatrix);
      sceneUbo.viewMatrix = view;
      sceneUbo.viewMatrixI = nv_math::invert(view);
      sceneUbo.viewMatrixIT = nv_math::transpose(sceneUbo.viewMatrixI );
      sceneUbo.wLightPos = vec4(tweak.lightDir * float(grid),1.0f);
      sceneUbo.time = float(time);

      glNamedBufferSubDataEXT(buffers.scene_ubo,0,sizeof(SceneData),&sceneUbo);

      glBindFramebuffer(GL_FRAMEBUFFER, fbos.scene);
      glViewport(0, 0, width, height);

      nv_math::vec4   bgColor(0.2,0.2,0.2,0.0);
      glClearColor(bgColor.x,bgColor.y,bgColor.z,bgColor.w);
      glClearDepth(1.0);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    }

    {
      NV_PROFILE_SECTION("Draw");

      switch(tweak.mode){
      case DRAW_STANDARD:
        drawStandard();
        break;
      case DRAW_TOKEN_EMULATED:
        drawTokenEmulation();
        break;
      case DRAW_TOKEN_BUFFER:
        drawTokenBuffer();
        break;
      case DRAW_TOKEN_LIST:
        drawTokenList();
        break;
      }
    }

    {
      NV_PROFILE_SECTION("Blit");
      glBindFramebuffer(GL_READ_FRAMEBUFFER, fbos.scene);
      glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
      glBlitFramebuffer(0,0,width,height,
        0,0,width,height,GL_COLOR_BUFFER_BIT, GL_NEAREST);
    }

    {
      NV_PROFILE_SECTION("TwDraw");
      TwDraw();
    }
  }

  void Sample::resize(int width, int height)
  {
    TwWindowSize(width,height);
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
    for (int i = 0; i < objects.size(); i++){
      const ObjectInfo&  obj = objects[i];
      GLuint usedProg = progManager.get( obj.program);

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
      updateCommandListState();
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
      updateCommandListState();
    }

    glCallCommandListNV(cmdlist.tokenCmdList);

  }

  void Sample::drawTokenEmulation()
  {
    if (bindlessVboUbo){
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

    if (bindlessVboUbo){
      glDisableClientState(GL_VERTEX_ATTRIB_ARRAY_UNIFIED_NV);
      glDisableClientState(GL_ELEMENT_ARRAY_UNIFIED_NV);
      glDisableClientState(GL_UNIFORM_BUFFER_UNIFIED_NV);

#if _DEBUG
      glDebugMessageControlARB(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
#endif
    }
  }

}

using namespace basiccmdlist;

int sample_main(int argc, const char** argv)
{
  Sample sample;
  return sample.run(
    PROJECT_NAME,
    argc, argv,
    SAMPLE_SIZE_WIDTH, SAMPLE_SIZE_HEIGHT,
    SAMPLE_MAJOR_VERSION, SAMPLE_MINOR_VERSION);
}

void sample_print(int level, const char * fmt)
{

}
