# 9. GLSL shaders: what they are, the syntax, how they connect, how they grow

Files: `data/shaders/triangle.vert`, `data/shaders/triangle.frag`, `CMakeLists.txt` (shader build),
`core/src/shaderModule.cpp`, `core/src/Pipeline.cpp`, `core/include/Vertex.hpp`,
`core/include/UniformData.hpp`, `core/src/FrameDescriptors.cpp`, `core/src/MaterialDescriptors.cpp`

(The language is **GLSL**, the OpenGL Shading Language. Vulkan can't read GLSL text directly; it
reads a compiled binary form called **SPIR-V**.)

---

## Part 1: The big picture

### What a shader is

A **shader** is a small program that runs **on the GPU**, thousands of times in parallel, once per
piece of data. Your engine's `Pipeline` has exactly two stages, so you have two shaders:

| Stage | File | Runs once per... | Job |
|---|---|---|---|
| **Vertex shader** | `triangle.vert` | **vertex** (your quads have 8) | Turn a vertex into a screen position; pass data along |
| *(fixed function)* | built into the GPU | triangle | **Rasterizer**: finds which pixels each triangle covers and **interpolates** the vertex outputs across them |
| **Fragment shader** | `triangle.frag` | **pixel** the triangle covers | Decide that pixel's color |

```
vertex buffer --> VERTEX SHADER --> rasterizer --> FRAGMENT SHADER --> color attachment (screen image)
 (Vertex structs)  once per vertex   (fixed hardware)  once per pixel     (and the depth test)
```

### Why they exist

The CPU could compute every pixel itself, but a GPU has thousands of tiny cores. Shaders are the
way you tell all those cores **what to do** with your data. Everything visual in the engine that
isn't a fixed hardware step (projection, texturing, later lighting) lives in a shader.

### The life of a shader, source to screen

```
data/shaders/triangle.vert   (text you write: GLSL)
        |   CMake runs:  glslangValidator -V triangle.vert -o build/shaders/triangle.vert.spv
        v
build/shaders/triangle.vert.spv   (SPIR-V: the compiled binary Vulkan accepts)
        |   readFile(...)             core/src/shaderModule.cpp
        |   vkCreateShaderModule(...) core/src/shaderModule.cpp  -> VkShaderModule
        v
VkPipelineShaderStageCreateInfo { stage = VERTEX, module, pName = "main" }   core/src/Pipeline.cpp
        |   vkCreateGraphicsPipelines(...)
        v
VkPipeline  --->  vkCmdBindPipeline + vkCmdDrawIndexed (every frame, CommandBuffers::record)
```

Key point: **shaders are compiled at build time, loaded at startup, baked into the pipeline.** You
can't swap a shader between draws; you bind a different *pipeline*.

---

## Part 2: Where shaders connect to the engine (the contracts)

A shader doesn't stand alone. Every `layout(...)` line in it is a **contract** with C++ code that
must say the same thing. When one side changes and the other doesn't, you get garbage or a
validation error. These are all the contracts in the engine today:

```
                     C++ side                                         GLSL side
 +-------------------------------------------------+   +---------------------------------------------+
 | Vertex.hpp  getAttributeDescription()           |   | triangle.vert                               |
 |   {location 0, R32G32B32, offsetof(pos)}        |<->|   layout(location = 0) in vec3 inPosition;  |
 |   {location 1, R32G32B32, offsetof(color)}      |<->|   layout(location = 1) in vec3 inColor;     |
 |   {location 2, R32G32,    offsetof(uv)}         |<->|   layout(location = 2) in vec2 inUV;        |
 +-------------------------------------------------+   +---------------------------------------------+
 | UniformData.hpp  struct CameraUBO {3 x mat4}    |<->|   layout(binding = 0) uniform CameraUBO {   |
 | FrameDescriptors: set 0, binding 0,             |   |     mat4 model; mat4 view; mat4 proj; } ubo;|
 |   UNIFORM_BUFFER, VERTEX stage                  |   |                                             |
 +-------------------------------------------------+   +---------------------------------------------+
 | MaterialDescriptors: set 1, binding 0,          |<->| triangle.frag                               |
 |   COMBINED_IMAGE_SAMPLER, FRAGMENT stage        |   |   layout(set = 1, binding = 0)              |
 |                                                 |   |     uniform sampler2D texSampler;           |
 +-------------------------------------------------+   +---------------------------------------------+
 | color attachment = swapchain image              |<->|   layout(location = 0) out vec4 outColor;   |
 +-------------------------------------------------+   +---------------------------------------------+

 vertex shader outputs  --(rasterizer interpolates)-->  fragment shader inputs
   layout(location = 0) out vec3 fragColor;                layout(location = 0) in vec3 fragColor;
   layout(location = 1) out vec2 fragUV;                   layout(location = 1) in vec2 fragUV;
```

| # | Contract | GLSL side | C++ side | What must match |
|---|---|---|---|---|
| 1 | **Vertex inputs** | `layout(location = N) in ...` in the vertex shader | `Vertex::getAttributeDescription()` | location numbers, and the type (`vec3` <-> `R32G32B32_SFLOAT`, `vec2` <-> `R32G32_SFLOAT`) |
| 2 | **Stage interface** | vertex `out` <-> fragment `in` | (none, shader to shader) | location numbers and types |
| 3 | **Uniform buffer** | the `uniform CameraUBO {...}` block | `CameraUBO` struct, `FrameDescriptors` | member order and types (std140 layout), `set`/`binding`, the stage flag |
| 4 | **Texture** | `uniform sampler2D` | `MaterialDescriptors` | `set`, `binding`, descriptor type, stage flag |
| 5 | **Output** | `layout(location = 0) out vec4` | the color attachment | location 0 is attachment 0 (the swapchain image) |
| 6 | **Which sets, in which order** | `set = 0`, `set = 1` | the array passed to `Pipeline` (`{descriptors.layoutHandle(), mat.layoutHandle()}` in `Renderer.cpp`) | the array index is the set number |

### Walk through each contract

**1. Vertex inputs.** The GPU reads vertex bytes out of the buffer. The attribute description
says "location 2 is two floats, starting at `offsetof(Vertex, uv)` bytes into each vertex." The
shader says `layout(location = 2) in vec2 inUV;`. Same number, same shape.

```cpp
// Vertex.hpp
attrs[2] = {2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv)};   // {location, binding, format, offset}
```
```glsl
// triangle.vert
layout(location = 2) in vec2 inUV;
```

**2. Stage interface.** Whatever the vertex shader `out`s at location 1 (`fragUV`) arrives at the
fragment shader's `in` at location 1. The names don't have to match; the **locations** do. In
between, the rasterizer **interpolates**: for a pixel in the middle of a triangle, `fragUV` is a
blend of the three vertices' UVs, weighted by how close the pixel is to each. That is how four
corner UVs become a smooth gradient over the quad.

**3. Uniform buffer.** `ubo` is one block of memory shared by every vertex in the draw call (it's
"uniform" because it doesn't vary per vertex). `CameraUBO` in C++ must have the **same members in
the same order**, because the bytes are copied raw (`FrameDescriptors::update` copies
`sizeof(CameraUBO)` bytes). `FrameDescriptors` gave this binding `VK_SHADER_STAGE_VERTEX_BIT`, so
only the vertex shader may read it. If the fragment shader tried, that would break the contract.

**4. Texture.** `sampler2D texSampler` is the image view **plus** the sampler (filtering, wrapping,
mip rules) bundled together as one `COMBINED_IMAGE_SAMPLER` descriptor. `set = 1, binding = 0`
means: the second descriptor set bound with `vkCmdBindDescriptorSets`, first slot. Its binding in
`MaterialDescriptors` is `FRAGMENT` stage.

**5. Output.** `outColor` at location 0 is written to color attachment 0, which `record()` pointed
at the current swapchain image view.

### The set / binding / location numbers, in one table

| Number | Means | Example here |
|---|---|---|
| `location` | which **attribute** (per-vertex data) or which **stage-to-stage / output slot** | vertex `in` 0,1,2; `out` 0,1; fragment `out` 0 |
| `set` | which **descriptor set** was bound (`firstSet + index`) | frame set = 0, material set = 1 |
| `binding` | which **slot inside that set** | 0 in both sets |

`set` defaults to 0 if you leave it out, which is why `triangle.vert` only writes `binding = 0`.

### Update frequency, and why the two sets are separate

```
set 0 (per frame)    camera UBO       changes every frame        bound once per frame
set 1 (per material) texture+sampler  changes per object/material bound per draw
```

Grouping bindings by how often they change means the frequent change (camera) doesn't force
re-binding the infrequent one (texture). That's why `CommandBuffers::record` binds both sets in
one call but a larger engine would bind set 0 once and swap set 1 per object.

---

## Part 3: The syntax, line by line

### The vertex shader

```glsl
#version 450
```
GLSL version 4.50. It must be the first line. 450 is the usual choice for Vulkan.

```glsl
layout(binding = 0) uniform CameraUBO {
  mat4 model;
  mat4 view;
  mat4 proj;
} ubo;
```
- `layout(...)` is a **qualifier** that attaches a number to the declaration (here which slot).
- `uniform` means read-only data that's the same for every invocation of this draw.
- `CameraUBO { ... }` is a **block** with three 4x4 matrices. `ubo` is the instance name, so
  members are read as `ubo.proj`, `ubo.view`, ...

```glsl
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inUV;
```
`in` = a per-vertex input read from the vertex buffer. `vec3` = three floats.

```glsl
layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragUV;
```
`out` = a value passed on to the next stage (interpolated across the triangle).

```glsl
void main() {
  gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
  fragColor = inColor;
  fragUV = inUV;
}
```
- `main()` is the entry point (matches `pName = "main"` in `Pipeline.cpp`).
- `gl_Position` is a **built-in output**: the vertex's **clip-space position**. Every vertex
  shader must write it.
- `vec4(inPosition, 1.0)` builds a 4-component vector from a `vec3` plus `w = 1.0`. The `w = 1`
  means "this is a point" (so translation applies).
- **Matrix order reads right to left**: first `model` (local to world), then `view` (world to
  camera), then `proj` (camera to clip). It's the "coordinate spaces ladder" from file 2.

### The fragment shader

```glsl
#version 450

// set 1 = per-material (MaterialDescriptors), binding 0 = the texture + its sampler
layout(set = 1, binding = 0) uniform sampler2D texSampler;

layout(location = 0) in vec3 fragColor; // still passed through; unused now that we sample
layout(location = 1) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

void main() {
  outColor = texture(texSampler, fragUV);
}
```
- `sampler2D` is the texture handle. `texture(sampler, uv)` reads it: it takes the UV, looks up
  the texels (with the sampler's filtering, wrapping and **mip selection**) and returns a `vec4`
  (r, g, b, a).
- `fragColor` is read but unused; the interface is still valid, and the compiler may drop it.
- `outColor` is a `vec4`: red, green, blue, alpha.

### The types and operators you'll use constantly

| Thing | Example | Notes |
|---|---|---|
| Vectors | `vec2`, `vec3`, `vec4` | floats. `ivec` = ints, `uvec` = unsigned |
| Matrices | `mat3`, `mat4` | **column-major**, same as GLM, so a `glm::mat4` copies straight in |
| Construct | `vec4(v3, 1.0)`, `vec3(1.0)` | `vec3(1.0)` is `(1,1,1)` |
| Swizzle | `v.xyz`, `c.rgb`, `v.xx`, `v.zyx` | `xyzw` = `rgba` = `stpq`; pick any, in any order |
| Arithmetic | `a + b`, `a * 0.5`, `m * v` | vector ops are component-wise; `mat * vec` is a transform |
| Built-in functions | `normalize`, `dot`, `cross`, `length`, `mix`, `clamp`, `pow`, `max`, `min` | `mix(a, b, t)` is a blend |
| Texture | `texture(tex, uv)` | returns `vec4` |
| Flow | `if`, `for`, `return` | works, but divergence costs performance on a GPU |
| Built-in inputs | `gl_VertexIndex`, `gl_FragCoord` | Vulkan GLSL uses `gl_VertexIndex` (OpenGL's `gl_VertexID` doesn't exist) |
| Built-in output | `gl_Position` | required in the vertex shader |

Rule of thumb: **GLSL has no pointers, no classes, no heap, no recursion.** It's C-like math on
small vectors.

### Vulkan GLSL vs the OpenGL GLSL most tutorials show

| OpenGL GLSL | Vulkan GLSL (what you write) |
|---|---|
| uniforms can float free | uniforms must be in a **block** with `set`/`binding` (or be samplers) |
| `gl_VertexID` | `gl_VertexIndex` |
| `glGetUniformLocation` at runtime | none: you pick `set`/`binding` and match them in C++ |
| compiled by the driver at runtime | compiled **offline** to SPIR-V (`glslangValidator -V`) |

### The one memory-layout trap: std140

A uniform block's memory layout follows **std140** rules, which are **not** the same as the C++
struct layout in general. Your `CameraUBO` is three `mat4`s, which is safe: each is 64 bytes, 16-byte
aligned, no padding. The trouble starts when you use `vec3`: in std140 a `vec3` is 12 bytes but must
**start on a 16-byte boundary**, while a `glm::vec3` in C++ only needs 4-byte alignment.

```
GLSL block:   vec3 a;  vec3 b;           C++ struct:   glm::vec3 a;  glm::vec3 b;

std140 offsets:   a = 0..11, b = 16..27   (b is pushed to the next 16-byte boundary)
C++ offsets:      a = 0..11, b = 12..23   (b follows immediately)

-> the shader reads b from byte 16, but C++ wrote it at byte 12: garbage, with no error.
```

Rule of thumb: keep blocks to `mat4`, `vec4` and scalars, or add padding on purpose. The comment
in `UniformData.hpp` ("std140: three mat4s, 16-byte aligned") is the reason that struct works.

---

## Part 4: Compiling and loading (the build side)

### CMake compiles them for you

```cmake
set(SHADER_SOURCES data/shaders/triangle.vert data/shaders/triangle.frag)
...
foreach(SHADER ${SHADER_SOURCES})
    ... set(OUTPUT_SPV "${CMAKE_BINARY_DIR}/shaders/${SHADER_NAME}.spv")
    add_custom_command(
        OUTPUT "${OUTPUT_SPV}"
        COMMAND ${GLSLANG_VALIDATOR} -V "${SHADER_PATH}" -o "${OUTPUT_SPV}"
        DEPENDS "${SHADER_PATH}"
        COMMENT "Compiling shader ${SHADER_NAME}")
endforeach()
add_custom_target(Shaders DEPENDS ${SHADER_BINARIES})
add_dependencies(${PROJECT_NAME} Shaders)
```

- `-V` = "Vulkan semantics, output SPIR-V".
- `DEPENDS` on the source means **editing a shader and building recompiles just that shader**; you
  don't have to touch C++.
- `SHADER_DIR` (a compile definition) points at `build/shaders/`, where `Renderer.cpp` loads
  `triangle.vert.spv` and `triangle.frag.spv`.

### Two kinds of errors

| When | Who reports | What it looks like |
|---|---|---|
| **Build time** | `glslangValidator` | A GLSL syntax or type error with a file and line number (like a C++ compiler) |
| **Run time** | Vulkan validation layers | A **mismatch between shader and C++** (contracts above): the shader is valid GLSL, but the pipeline, descriptors or vertex description disagree with it |

Because the second kind is a "contract" error, your first move on a validation message about
descriptors or vertex input is: *which row of the contract table is this?*

### Loading

```cpp
// shaderModule.cpp
VkShaderModuleCreateInfo createInfo{};
createInfo.codeSize = code.size();                                       // bytes of SPIR-V
createInfo.pCode    = reinterpret_cast<const uint32_t*>(code.data());   // SPIR-V is 32-bit words
vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule);

// Pipeline.cpp: the module is only a container; this says which stage and which function
vertStageInfo.stage  = VK_SHADER_STAGE_VERTEX_BIT;
vertStageInfo.module = vertModule;
vertStageInfo.pName  = "main";
```

The `ShaderModuleGuard` in `Pipeline.cpp` destroys the `VkShaderModule` when the constructor ends:
once `vkCreateGraphicsPipelines` has run, the pipeline holds its own copy and the module is no
longer needed.

---

## Part 5: How this grows (and why it won't stay "a new shader every time")

You're right that today it **feels** like each new thing needs a new shader file. That's because
there is only one thing in the engine so far. In practice a shader is written per **algorithm**,
not per object. Here is what changes in which layer:

| You want to... | New shader? | What actually changes |
|---|---|---|
| A different **texture** on an object | **No** | A new `MaterialDescriptors` set pointing at another image. Same pipeline, same shader. |
| A different **color / tint / roughness** | **No** | A new value in a uniform buffer (or push constant). Same shader. |
| A **new object** with a different mesh | **No** | Bind its vertex/index buffers and draw again. |
| Objects at **different positions** | **No** | Per-object transform (see below). Same shader. |
| **Lighting** | Same shader, grown | Add normals as a vertex input, light data as a uniform, and the math to the fragment shader. |
| **Transparency, wireframe** | Same shader | A different *pipeline state* (blending, polygon mode), not different GLSL. |
| A **shadow pass** | **Yes** | A different algorithm: depth-only vertex shader into a separate image. |
| A **skybox**, **UI**, **post-processing** (blur, bloom, tone mapping) | **Yes** | Each is a genuinely different job. |
| **Compute** work (mip generation, culling, particles) | **Yes** | A compute shader: a third kind, no vertices at all. |

So **a game with hundreds of materials typically has tens of shaders.** Most "new looks" are data
(uniforms and textures), not code.

### A limitation that exists in the engine today

`model` is inside the **per-frame** camera UBO (`CameraUBO { model, view, proj }`), so every draw in
a frame uses the **same** model matrix. That's fine for one pair of crossing quads and is the first
thing to outgrow. The next step is moving `model` out to a **push constant** (a few bytes you
write straight into the command buffer per draw):

```glsl
layout(push_constant) uniform Push { mat4 model; } push;
gl_Position = ubo.proj * ubo.view * push.model * vec4(inPosition, 1.0);
```

Then `ubo` only holds `view` and `proj`, and each object supplies its own `model` with
`vkCmdPushConstants`. (This also needs a `VkPushConstantRange` in the pipeline layout: a contract
#7 for the table above.)

### The growth path, in the order it usually happens

| Step | What | Why it removes the "annoying" part |
|---|---|---|
| 1 | **Push constants** for per-object data (model matrix, tint) | Many objects share one shader and one pipeline |
| 2 | **A material concept**: a small C++ struct bundling *shader pair + pipeline + descriptor layout + parameters* | Making a new material is filling a struct, not writing Vulkan setup code. This is what `MaterialDescriptors` is growing into |
| 3 | **Compile every file in `data/shaders/` automatically** (a CMake glob) | Adding a shader stops requiring an edit to `SHADER_SOURCES` |
| 4 | **Shared code with `#include`** (`GL_GOOGLE_include_directive`) | Lighting functions, the camera block and common structs are written once instead of copied into each shader |
| 5 | **Variants by `#define` or specialization constants** | One source file produces "with normal map" / "without" versions instead of two near-identical files |
| 6 | **Hot reload**: recompile and recreate the pipeline while the app runs | Shader iteration takes seconds, no restart |
| 7 | **Reflection** (reading the `set`/`binding`/`location` back out of the SPIR-V, e.g. with SPIRV-Reflect) | Descriptor layouts and vertex input get **generated from the shader**, so the contract table in Part 2 stops being something you maintain by hand |
| 8 | **Pipeline cache** | Faster startup once there are many pipelines |
| 9 | **More stages**: compute (culling, mips), shadow, post-process passes, then a render graph to order them | The engine's frame becomes a graph of passes |

`★ Insight ─────────────────────────────────────`
- Steps 2, 5 and 7 are the ones that make shaders feel less like boilerplate: they move the work
  from "write Vulkan setup for every new thing" to "describe the thing in data". Today every
  contract in Part 2 is hand-matched, which is why a new shader feels like a chore; reflection and
  a material struct are how real engines remove that.
- None of that is needed yet. Doing it with one shader pair would be building machinery for a
  problem you haven't hit. The trigger for step 1 is the **second object that needs its own
  transform**; for step 2, the **second material**.
`─────────────────────────────────────────────────`

---

## Part 6: Try it (no C++ needed)

Shaders are the fastest thing in the engine to experiment with: edit, rebuild (CMake recompiles only
the shader), run.

1. **See the UVs as color.** In `triangle.frag` change `main` to `outColor = vec4(fragUV, 0.0, 1.0);`
   The quad shows red growing to the right and green growing downward. That *is* the interpolation.
2. **Use the unused vertex color.** `outColor = texture(texSampler, fragUV) * vec4(fragColor, 1.0);`
   The texture is tinted by the per-vertex colors, blended across the quad.
3. **Invert the texture.** `outColor = vec4(1.0 - texture(texSampler, fragUV).rgb, 1.0);`
4. **Break a contract on purpose** (then undo it): change `inUV`'s location from 2 to 3 in
   `triangle.vert`. The build succeeds (valid GLSL) but at run time something disagrees with
   `Vertex.hpp`. Read what the validation layer says and match it to a row of the contract table.

## Check yourself

1. How many times does the vertex shader run for a draw of one quad, and what about the fragment
   shader?
2. `fragUV` in the fragment shader is not any vertex's UV exactly. What produced it, and which part
   of the pipeline does that?
3. The vertex shader says `layout(location = 2) in vec2 inUV;`. Which line of C++ must agree, and
   what must it say?
4. What are `set` and `binding` for `texSampler`, and which C++ file defines the matching layout?
5. Why would the fragment shader fail to read `ubo` today?
6. You want a second cube with a different texture. Do you need a new shader? What do you create?
7. You want two cubes at different positions. What in the current engine stops you, and what is the
   usual fix?
8. A build succeeds but the validation layer complains about a descriptor. Which kind of error is
   that, and who reported it?

<details>
<summary>Answer key</summary>

1. The vertex shader runs once per vertex (the GPU may reuse results for repeated indices; your
   quad has 4 unique vertices, 8 across both quads). The fragment shader runs once for **every
   pixel** the quad covers.
2. The **rasterizer** (fixed-function hardware between the two stages) interpolates the vertex
   shader's `out` values across the triangle, weighted by each pixel's position.
3. `Vertex::getAttributeDescription()` in `Vertex.hpp`: `attrs[2] = {2, 0,
   VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv)}`: location 2, two 32-bit floats.
4. `set = 1`, `binding = 0`. `MaterialDescriptors.cpp` defines the layout (a
   `COMBINED_IMAGE_SAMPLER`, `FRAGMENT` stage), and the order of layouts passed to `Pipeline` in
   `Renderer.cpp` makes it set 1.
5. The camera binding in `FrameDescriptors.cpp` has `stageFlags = VK_SHADER_STAGE_VERTEX_BIT`, so
   only the vertex stage is allowed to access it.
6. No. Create another `Texture` and another `MaterialDescriptors` set (and a second mesh draw).
   The same pipeline and shader are reused.
7. `model` is part of the **per-frame** UBO, so all draws share one model matrix. The usual fix is
   a push constant (or a per-object dynamic UBO) carrying each object's `model`.
8. A **run-time contract mismatch** between valid GLSL and the C++ descriptor setup, reported by
   the Vulkan validation layers. A GLSL syntax error would have been reported at build time by
   `glslangValidator`.

</details>
