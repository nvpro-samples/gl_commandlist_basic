# gl commandlist basic

In this sample the **NV_command_list** extension is used to render a basic scene and texturing is performed via **ARB_bindless_texture**.

> **Note:** The NV_command_list extension is officially shipping with 347.88. The appropriate functions used in this sample can also be found in some older drivers (for example 347.09 and higher), however the performance for all driver/hardware combinations may not be representative there. The [spec](http://www.opengl.org/registry/specs/NV/command_list.txt) is available, and feedback is welcome and should be sent to Christoph Kubisch <ckubisch@nvidia.com>, Tristan Lorach <tlorach@nvidia.com>, or Pierre Boudier <pboudier@nvidia.com>. Additional information can be found in this [slide deck](http://www.slideshare.net/tlorach/opengl-nvidia-commandlistapproaching-zerodriveroverhead) from SIGGRAPH Asia 2014, as well as the latest GTC 2015 [presentation](http://on-demand.gputechconf.com/gtc/2015/video/S5135.html).

This new extension is built around bindless GPU pointers/handles and three more technologies, which allow rendering scenes with many state changes and hundreds of thousands of drawcalls with extremely low CPU time:

- *Tokenized Rendering*:
  - Evolution of the "MultiDrawIndirect" mechanism in OpenGL
   - Commands are encoded into binary data (tokens), instead of issuing classic gl calls. This allows the driver or the GPU to efficiently iterate over a stream of many commands in one or multiple sequences: **glDrawCommands( ...tokenbuffer, offsets[], sizes[], numSequences)**
   - The tokens are stored in regular OpenGL buffers and can be re-used across frames, or manipulated by the GPU itself. Latency-free occlusion culling can be implemented this way (a special _terminate sequence_ token exists).
   - Next to draw calls, the tokens cover the most frequent state changes (vertex, index, uniform-buffers) and a few basic scalar changes (blend color, polygonoffset, stencil ref...).
   - As tokens only reference data (for example uniform buffers), their content is still free to change - you can change vertex positions or matrices freely (which is different from classic display lists).
   - To get an idea of what is currently possible check the **nvtoken.cpp/hpp** files, which also showcase how the tokenstream could be decoded into classic OpenGL calls.

```cpp
// The tokens are tightly-packed structs and most common tokens are 16 bytes.
// Below you will find the token definition to update a UBO binding. Compared 
// to standard UBOs, tokens update the binding per stage.


  UniformAddressCommandNV  
  {
    GLuint header;      // glGetCommandHeaderNV(GL_UNIFORM_ADDRESS_COMMAND_NV)
    GLushort   index;   // in glsl: layout(binding=INDEX,commandBindableNV) uniform ...
    GLushort   stage;   // glGetStageIndexNV(GL_VERTEX_SHADER)
    GLuint64   address; // glGetNamedBufferParameterui64vNV(buffer,
                        //   GL_BUFFER_GPU_ADDRESS, &address);
  } cmd;


// The mentioned glGets should not be done at encode time.
```

- *StateObjects*:
   - Costly validation in the driver can often happen late at draw-call time or at other unexpected times, potentially causing unstable framerates. Monolithic state-objects, as they are common in other new graphics apis, allow pre-validation and reuse of the core rendering state (FBO, program, blending...).
   - Full control over when validation happens via **glCaptureState(stateobj, primitiveBaseMode)**, uses the current GL state's setup, no other new special api, which eases integration.
   - Very efficient state switching between different stateobjects: **glDrawCommandsStates(..., stateobjects[], fbos[], numSequences)**
   - A stateobject can be reused with compatible fbos (same internal formats, but different textures/sizes).
   - To get an idea what the stateobject captures (or how to emulate it) check **statesystem.cpp/hpp**.

- *Pre-compiled Command List Object*:
   - StateObjects and client-side tokens can be pre-compiled into a special object.
   - Allows further driver optimization (faster stateobject transitions) at the loss of flexibility (rendering from tokenbuffer allows buffer to change as well as stateobjects/FBOs).

#### Performance

The sample renders 1024 objects, each using a sphere or box IBO/VBO pairing, with either a shader using geometry shader as well or not (just as example for some state switching, around 500 toggles between these two per frame). Each object references a range within a big UBO that stores per-object data like matrix, color and texture. On the console output window the performance of CPU and GPU can be seen in detail (be aware that CPU timings may be skewed if the driver runs in dual-core mode).

The output should look something like this:
``` 
  Timer Frame;   GL   1333; CPU   2408; (microseconds, avg 758)
  Timer Setup;   GL     21; CPU     42; (microseconds, avg 758)
  Timer Draw;    GL    857; CPU   1752; (microseconds, avg 758)
  Timer Blit;    GL     59; CPU     54; (microseconds, avg 758)
  Timer TwDraw;  GL    389; CPU    551; (microseconds, avg 758)
```
Here some preliminary example results for *Timer Draw* on a win7-64, i7-860, Quadro K5000 system

draw mode | GPU time | CPU time (microseconds)
------------ | ------------- | -------------
standard | 850 | 1750
nvcmdlist emulated | 830 | 1500
nvcmdlist buffer | 775 | 30
nvcmdlist list | 775 | <1

One can see that by classic API usage the scene is CPU bound, as more time is spent there, than on the graphics card (using ARB_timer_query functionality), despite the already very well optimized Quadro drivers. Only through the native use of the NV_command_list do we more or less eliminate the CPU constraint and become GPU bound. One could argue that by better state sorting (which is still good and improves GPU time) and batching techniques CPU performance could be improved, but this may add complexities in the application. Here each object can have its own resource set and be modified independently.

The gained performance in emulation comes from the use of bindless UBO and VBO. The token-buffer technique is slightly slower on CPU than the pre-compiled list, because the 500 stateobject transitions still need to be checked every time. The nvcmdlist techniques essentially only make a single dispatch. The closest to get to this would be with multi-draw-indirect and vertex divisor indexing, but makes shaders more complex by adding parameter indirections and would not allow simple shader or other state changes.

> **New level of AZDO:** An entire scene with state changes (shaders, buffers...) can be dispatched in a few microseconds CPU time, independent of the scene's complexity. Even if the tokens or stateobjects are more dynamic or have to be streamed per-frame the CPU time savings compared to standard API usage will be huge.
> 
> **Why can't display-lists be so fast?** Because they are too unbounded and inherit too much state from the OpenGL context at execution time (unless very specific subsets of commands are used, such as only geometry specification).
> 
> **Explicit Control:** The extension continues a trend in modern API design that gives the developer more explicit control over when certain costs arise, and how to manage data across frames. This also helps the driver pick very efficient paths, and it leverages GPU capabilities such as virtual memory addresses as already provided by other shipping bindless extensions (NV_vertex_buffer_unified_memory, NV_uniform_buffer_unified_memory, NV_shader_buffer_load/store, ARB/NV_bindless_texture) for very fast drawing.
> 
> **GPU bound?** While the extension primarily targets CPU bottlenecks, advanced GPU work creation through GPU-written token-buffers may allow in-frame alterations to _what_ and _how_ geometry is drawn, without costly CPU synchronization. The additional CPU time won may also be used to optimize the scene further, or invested elsewhere.

#### Sample Highlights

Depending on the availability of the extension, the sample allows switching between a standard OpenGL approach for rendering the scene, as well as the new extension in either token-buffer or commandlist-object mode. Inside **basic-nvcommandlist.cpp** you will find:

 - Sample::drawStandard()
 - Sample::drawTokenBuffer()
 - Sample::drawTokenList()
 - Sample::drawTokenEmulation()

As well as initialization and state update functions:

 - Sample::initCommandListMinimal()
 - Sample::updateCommandListStateMinimal()


 - Sample::initCommandList()
 - Sample::updateCommandListState()

The ''Minimal'' functions are used if the emulation layer is disabled via ```#define ALLOW_EMULATION_LAYER 0``` at the top of **basic_nvcommandlist.cpp**. They represent the bare minimum work to do and don't make use of the nvtoken helper classes.

The emulation layer allows you to roughly get an idea of how the glDrawCommands* and glStateCapture work internally, and also aids debugging as the tokens are never error-checked. Customizing this emulation may also be useful as a permanent compatibility layer for driver/hardware combinations that do not run the extension natively.

![sample screenshot](https://github.com/nvpro-samples/gl_commandlist_basic/blob/master/doc/sample.jpg)

#### Building
Ideally, clone this and other interesting [nvpro-samples](https://github.com/nvpro-samples) repositories into a common subdirectory. You will always need [nvpro_core](https://github.com/nvpro-samples/nvpro_core). The nvpro_core is searched either as a subdirectory of the sample, or one directory up.

If you are interested in multiple samples, you can use the [build_all](https://github.com/nvpro-samples/build_all) CMAKE as an entry point. It will also give you options to enable or disable individual samples when creating the solutions.

#### Related Samples
The extension is also used in the [gl commandlist bk3d models](https://github.com/nvpro-samples/gl_commandlist_bk3d_models), [gl occlusion culling](https://github.com/nvpro-samples/gl_occlusion_culling), and [gl cadscene rendertechniques](https://github.com/nvpro-samples/gl_cadscene_rendertechniques) samples. The latter two samples include token-buffer-based occlusion culling and the last also includes token-streaming techniques on real-world scenes.

