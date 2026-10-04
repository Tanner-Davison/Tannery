# Tannery — High-Level Architecture Overview

Quick reference: what each component is, what it owns, and what it depends on.
For the *why* behind decisions and the bugs hit along the way, see
[`../project-notes.md`](../project-notes.md). For how the layers were split out of the
original single `App` class, see [`../architecture-refactor.md`](../architecture-refactor.md).

---

## 1. Big picture: three layers and a resource

```
 App                      "the program": window, main loop, resize callback
  │ uses
  ▼
 Renderer                 "how to draw a frame": swapchain, per-frame sync, pipeline,
  │                        command buffers, drawFrame, resize handling
  │ uses
  ▼
 GraphicsContext          "the GPU": instance, surface, device, queues, allocator
  │ uses                   (lives for the whole program)
  ▼
 Vulkan / GLFW / VMA

 Mesh                     a resource: vertex buffer + index buffer + index count
                          (created through GraphicsContext, drawn by Renderer)
```

**Dependency rule:** arrows only point down. `GraphicsContext` never knows a `Renderer`
exists, and `Renderer` never knows about `App`. This is what lets one layer change without
touching the others.

**Ownership in `App`** (declaration order is the dependency order; destroyed in reverse):

```cpp
Window          window;     // first created, last destroyed
GraphicsContext context;    // GPU setup, whole-program lifetime
Mesh            mesh;       // needs the context's allocator; must outlive the renderer's frames
Renderer        renderer;   // borrows context, window, mesh; rebuilt parts live inside it
```

Each Vulkan-owning class follows the same RAII shape: the constructor creates the handle or
`throw`s `std::runtime_error`; the destructor destroys it (null-checked); copy and move are
`= delete`d, since each object owns a unique GPU resource.

---

## 2. Components at a glance

| Layer | Component | Kind | Owns | Depends on |
|---|---|---|---|---|
| App | `App` | class | `Window`, `GraphicsContext`, `Mesh`, `Renderer` | (top) |
| Platform | `Window` | RAII class | `GLFWwindow*` | (nothing) |
| Renderer | `Renderer` | class | `Swapchain`, `SyncObjects` (rebuilt on resize, `unique_ptr`s); `FrameDescriptors`, `Pipeline`, `CommandBuffers` (whole-program lifetime); `currentFrame` | `GraphicsContext`, `Window`, `Mesh` |
| Context | `GraphicsContext` | class | `VulkanInstance`, `Surface`, physical device, `QueueFamilyIndices`, `LogicalDevice`, `Allocator` | `Window` (for the surface) |
| Context | `VulkanInstance` | RAII class | `VkInstance`, debug messenger | (nothing) |
| Context | `Surface` | RAII class | `VkSurfaceKHR` | Instance, Window |
| Context | `LogicalDevice` | RAII class | `VkDevice`, graphics and present `VkQueue` | Physical device, Queue families |
| Context | `Allocator` | RAII class | `VmaAllocator` | Instance, Physical device, Device |
| Resource | `Mesh` | class | vertex `Buffer`, index `Buffer`, index count | `GraphicsContext`, `Vertex` |
| Resource | `Buffer` | RAII class | `VkBuffer` + `VmaAllocation` | Allocator |
| Renderer | `Swapchain` | RAII class | `VkSwapchainKHR`, image views | Device, Surface, Support, Window |
| Renderer | `SyncObjects` | RAII class | semaphores, fences | Device, Swapchain image count |
| Renderer | `Pipeline` | RAII class | `VkPipeline`, `VkPipelineLayout` | Device, Swapchain extent and format, SPIR-V shaders, `Vertex` |
| Renderer | `CommandBuffers` | RAII class | `VkCommandPool`, one `VkCommandBuffer` per frame slot | Device, queue families (images, views, pipeline and mesh arrive as `record()` parameters) |
| Renderer | `FrameDescriptors` | RAII class | descriptor set layout, pool, one set and one mapped uniform `Buffer` per frame slot | `GraphicsContext` |
| Data | `CameraUBO` (`UniformData.hpp`) | header-only struct | (nothing) | GLM; mirrors the shader's uniform block |
| Helpers | `physicalDevice`, `queueFamilies`, `swapchainSupport`, `shaderModule`, `copyBuffer` | free functions | (nothing) | various |
| Helpers | `Vertex` | header-only struct | (nothing) | GLM; vertex layout descriptions |

---

## 3. Component reference

### App — `App.hpp/.cpp` (about 35 lines)
**Responsibility:** the program itself, and nothing about Vulkan.
- Owns the `Window`, `GraphicsContext`, `Mesh`, and `Renderer`.
- Defines the quad's vertices and indices, builds the `Mesh` from them.
- Registers the GLFW framebuffer-size callback and forwards it to `Renderer::onFramebufferResized()`.
- `run()` loops on `glfwPollEvents()` and `renderer.drawFrame()`, then `context.waitIdle()` before destructors run.

### GraphicsContext — `GraphicsContext.hpp/.cpp`
**Responsibility:** everything about the GPU that lives for the whole program and does not depend on the window size.
- Creates, in dependency order: `VulkanInstance` → `Surface` → physical device → queue families → `LogicalDevice` → `Allocator`.
- Exposes handles through accessors (`deviceHandle()`, `graphicsQueue()`, `allocatorHandle()`, `queueFamilies()`, ...).
- `createDeviceLocalBuffer(data, size, usage)`: the staging-upload path (CPU-visible staging `Buffer` filled with `vmaCopyMemoryToAllocation`, GPU-local destination `Buffer`, `copyBuffer` with a wait, staging destroyed on return).
- `waitIdle()` wraps `vkDeviceWaitIdle`.

### Mesh — `Mesh.hpp/.cpp`
**Responsibility:** one drawable shape on the GPU.
- Holds a vertex `Buffer`, an index `Buffer`, and `indexCount()` (taken from `indices.size()`, so the count travels with the data).
- Uploads through `GraphicsContext::createDeviceLocalBuffer`; rejects empty data.

### Renderer — `Renderer.hpp/.cpp`
**Responsibility:** turning a `Mesh` into frames, and surviving resizes.
- Owns everything that depends on the swapchain, held as `unique_ptr`s so `recreateSwapchain()` can rebuild them.
- `drawFrame()`: the per-frame sequence (see section 4), with every `VkResult` checked.
- `recreateSwapchain()`: wait out a 0x0 framebuffer, `waitIdle`, re-query `SwapchainSupport`, reset dependents, build the new swapchain with `oldSwapchain`, rebuild sync objects and command buffers, reset `imagesInFlight`. The pipeline survives because viewport and scissor are dynamic state.
- `buildCommandBuffers()`: the single place that knows how to construct `CommandBuffers` (startup and resize).
- `imagesInFlight[imageIndex]` holds the fence of the frame that last used each swapchain image, because the command buffers are per-image.

### Window — `Window.hpp/.cpp`
- Initializes GLFW (forcing X11 via `glfwInitHint` on Linux for smooth resizing; see project notes), creates a `GLFWwindow*` with no client API, enables resizing and `GLFW_SCALE_TO_MONITOR`.

### VulkanInstance, Surface, LogicalDevice, Allocator, Buffer
- **VulkanInstance:** Vulkan 1.3 instance, GLFW's required extensions, validation layer and debug messenger. Apple portability extensions behind `#ifdef __APPLE__`.
- **Surface:** wraps `glfwCreateWindowSurface`.
- **LogicalDevice:** one `VkDeviceQueueCreateInfo` per unique queue family; `VK_KHR_swapchain`; dynamic rendering feature chained on `pNext`.
- **Allocator:** `vmaCreateAllocator`/`vmaDestroyAllocator` (`vma.cpp` is the only translation unit with `VMA_IMPLEMENTATION`).
- **Buffer:** `vmaCreateBuffer`/`vmaDestroyBuffer` with usage and VMA allocation flags passed in.

### Swapchain, SyncObjects
- **Swapchain:** chooses format (`B8G8R8A8_SRGB`), present mode (`MAILBOX`, fallback `FIFO`) and extent; takes an optional `oldSwapchain`; owns its image views.
- **SyncObjects:** `MAX_FRAMES_IN_FLIGHT` (2, `public static constexpr`). Per frame slot (`currentFrame`): a fence (created signaled) and an image-available semaphore. Per swapchain image (`imageIndex`): a render-complete semaphore.

### Pipeline
- Loads SPIR-V, configures fixed-function state, and takes its vertex layout from `Vertex`. Viewport and scissor are dynamic state, so the pipeline does not depend on the window size. Dynamic rendering: attachment format via `VkPipelineRenderingCreateInfo`, no render pass.
- **Pipelines are baked:** changing shaders, vertex layout, topology, rasterizer or blend state means a different pipeline.

### CommandBuffers
- One pool and one command buffer per swapchain image, recorded once: barrier to `COLOR_ATTACHMENT_OPTIMAL`, `vkCmdBeginRendering`, bind pipeline, set viewport and scissor, bind the mesh's vertex and index buffers, `vkCmdDrawIndexed`, `vkCmdEndRendering`, barrier to `PRESENT_SRC_KHR`.
- Takes a `const Mesh&`. Recording is cheap and happens up front; each frame only submits.

---

## 4. The frame, step by step (`Renderer::drawFrame`)

```
(if resize flag set)   recreateSwapchain()
vkWaitForFences        fences[currentFrame]: this slot's previous work is done
vkAcquireNextImageKHR  signals imageAvailable[currentFrame]; returns imageIndex
                       OUT_OF_DATE -> recreate and skip this frame
wait imagesInFlight[imageIndex] if set; imagesInFlight[imageIndex] = this fence
vkResetFences          re-arm this slot's fence (only after acquire succeeded)
vkQueueSubmit          runs commandBuffers[imageIndex]
                         waits:   imageAvailable[currentFrame] @ COLOR_ATTACHMENT_OUTPUT
                         signals: renderComplete[imageIndex] + fences[currentFrame]
vkQueuePresentKHR      waits renderComplete[imageIndex]; OUT_OF_DATE/SUBOPTIMAL -> recreate
currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT
```

`currentFrame` is chosen by us; `imageIndex` is chosen by the driver.

---

## 5. Known gaps and likely next steps

| Gap | Effect | Direction |
|-----|--------|-----------|
| One mesh, one pipeline, hardcoded in `App` | Not a scene yet | Multiple meshes; later a scene/entity layer above `Renderer` |
| No depth buffer, textures, or model loading | Nothing 3D yet | Depth image, textures, glTF loading, lighting |

Longer-term roadmap (static mesh → skeletal animation → blending/IK → Jolt cloth)
is in [`../project-notes.md`](../project-notes.md).

---

## 6. Conventions

- Designated initializers for every Vulkan struct, in declaration order.
- Every handle is initialized to `VK_NULL_HANDLE`.
- `std::optional` for "not found yet".
- `p`-prefixed constructor parameters where they would shadow a member.
- **A class only destroys what it created.**
- `const T&` for inputs that are only read; plain values for handles and numbers.
- Quotes for project headers (including `vk_enum_string_helper.h`), angle brackets for system and library headers (`<vk_mem_alloc.h>`, `<vulkan/vulkan.h>`).
- Lowercase-named files (`physicalDevice`, `queueFamilies`, `swapchainSupport`, `shaderModule`, `copyBuffer`) are free-function helpers. Capitalized files are classes.
- Add every new `.cpp` to `SOURCES` in `CMakeLists.txt` at the moment it is created (a missing entry shows up as a link error, not a compile error).
