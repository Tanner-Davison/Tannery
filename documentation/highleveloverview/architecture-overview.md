# Tannery — High-Level Architecture Overview

Quick reference: what each component is, what it owns, and what it depends on.
For the *why* behind decisions and the bugs hit along the way, see
[`../project-notes.md`](../project-notes.md).

---

## 1. Big picture

`main()` builds one `App`. `App` owns every other object as a member field.
Each Vulkan-owning class follows the same RAII shape:

- **Constructor** creates the handle, or `throw`s `std::runtime_error`.
- **Destructor** destroys it (null-checked).
- **Copy and move** are all `= delete`d, since each object owns a unique GPU resource.

`App` declares its members in dependency order. C++ constructs in declaration
order and destroys in reverse, so teardown order is correct automatically.

```
Window → VulkanInstance → Surface → (physicalDevice, indices) → LogicalDevice
       → (support) → Swapchain → SyncObjects → Pipeline → CommandBuffers
```

Each layer depends only on things above it.

---

## 2. Dependency chain at a glance

| # | Component | Kind | Owns | Depends on |
|---|-----------|------|------|------------|
| 1 | `Window` | RAII class | `GLFWwindow*` | (nothing) |
| 2 | `VulkanInstance` | RAII class | `VkInstance`, `VkDebugUtilsMessengerEXT` | (nothing) |
| 3 | `Surface` | RAII class | `VkSurfaceKHR` | Instance, Window |
| 4 | Physical device selection | free function | (nothing, hardware is enumerated) | Instance |
| 5 | Queue families | free function + struct | (nothing, query result) | Physical device, Surface |
| 6 | `LogicalDevice` | RAII class | `VkDevice`, graphics and present `VkQueue` | Physical device, Queue families |
| 7 | Swapchain support | free function + struct | (nothing, query result) | Physical device, Surface |
| 8 | `Swapchain` | RAII class | `VkSwapchainKHR`, image views | Device, Surface, Support, Window |
| 9 | `SyncObjects` | RAII class | semaphores, fence | Device, Swapchain image count |
| 10 | `Pipeline` | RAII class | `VkPipeline`, `VkPipelineLayout` | Device, Swapchain extent and format, SPIR-V shaders |
| 11 | `CommandBuffers` | RAII class | `VkCommandPool`, `VkCommandBuffer`s | Device, Swapchain images and views, Pipeline |
| 12 | `App::drawFrame()` | method | (nothing) | All of the above |

---

## 3. Component reference

### 1. `Window` — `Window.hpp/.cpp`
**Responsibility:** create and destroy the OS window through GLFW.
- Initializes GLFW, creates a `GLFWwindow*` with no client API (`GLFW_NO_API`), since Vulkan draws to it and not OpenGL.
- Exposes `handle()` for the surface and for framebuffer-size queries.
- The first member of `App`, so it is created first and destroyed last.

### 2. `VulkanInstance` — `VulkanInstance.hpp/.cpp`
**Responsibility:** the Vulkan library's entry point for this process.
- Builds `VkApplicationInfo` and `VkInstanceCreateInfo`, requesting Vulkan 1.3.
- Enables the instance extensions GLFW requires for windowing.
- Enables `VK_LAYER_KHRONOS_validation` and the `VK_EXT_debug_utils` messenger (`debugCallbackVulkan.hpp`), which routes validation messages to our callback.
- Has Apple-only portability extensions behind `#ifdef __APPLE__`.

### 3. `Surface` — `Surface.hpp/.cpp`
**Responsibility:** the link between Vulkan and the window (`VkSurfaceKHR`).
- Wraps `glfwCreateWindowSurface`, so the app never branches on platform (Wayland here).
- Stores the `VkInstance` only so it can destroy the surface.

### 4. Physical device selection — `physicalDevice.hpp/.cpp`
**Responsibility:** pick which GPU to use.
- `getPhysicalDevice(instance)` enumerates hardware with `vkEnumeratePhysicalDevices` (count-then-array).
- Not an RAII class, because a `VkPhysicalDevice` is enumerated hardware and is never created or destroyed by the app.
- Currently the RTX 3090.

### 5. Queue families — `queueFamilies.hpp/.cpp`
**Responsibility:** find which queue families can do graphics and present.
- `QueueFamilyIndices` holds `std::optional<uint32_t>` for the graphics and present family indices, plus `isComplete()`.
- `findQueueFamilies` queries `vkGetPhysicalDeviceQueueFamilyProperties` and `vkGetPhysicalDeviceSurfaceSupportKHR`.
- The two indices may be equal (they are on this GPU) or different on other hardware, so the code never assumes either.

### 6. `LogicalDevice` — `LogicalDevice.hpp/.cpp`
**Responsibility:** the live handle (`VkDevice`) that all GPU work goes through.
- Creates one `VkDeviceQueueCreateInfo` per *unique* queue family, using a `std::set` because Vulkan forbids duplicates.
- Enables the `VK_KHR_swapchain` device extension.
- Chains `VkPhysicalDeviceDynamicRenderingFeatures` (`dynamicRendering = VK_TRUE`) onto `pNext`, which is what allows `vkCmdBeginRendering`.
- Fetches and exposes `GraphicsQueueHandle()` and `PresentQueueHandle()`.

### 7. Swapchain support — `swapchainSupport.hpp/.cpp`
**Responsibility:** ask what the surface and GPU pair can do before creating a swapchain.
- `SwapchainSupport` holds capabilities (a single struct), formats, and present modes (both arrays), plus `isComplete()`.
- `getSwapchainSupportDetails` fills it.
- A query result only, so it is not RAII.

### 8. `Swapchain` — `Swapchain.hpp/.cpp`
**Responsibility:** the queue of images that get presented to the window.
- Chooses settings from the support data:
  - **Format:** prefers `B8G8R8A8_SRGB` with `SRGB_NONLINEAR`.
  - **Present mode:** prefers `MAILBOX`, falls back to `FIFO` (guaranteed to exist).
  - **Extent:** uses the surface's extent, or clamps the GLFW framebuffer size when the surface says "you choose".
- Creates the `VkSwapchainKHR`, using `EXCLUSIVE` sharing when the graphics and present families match and `CONCURRENT` otherwise.
- Retrieves the `VkImage`s (owned by the swapchain) and creates and owns one `VkImageView` per image.
- Exposes format, extent, image count, images, and image views for later stages.

### 9. `SyncObjects` — `SyncObjects.hpp/.cpp`
**Responsibility:** CPU-GPU and GPU-GPU synchronization for the render loop.
- `MAX_FRAMES_IN_FLIGHT` (2) is a `public static constexpr`, shared with `App`.
- **Per frame slot** (indexed by `currentFrame`): one `VkFence` (created **signaled**, so the first wait doesn't block forever) and one image-available semaphore. The fence proves the slot's previous submit finished, which also proves its semaphore is free to signal again.
- **Per swapchain image** (indexed by `imageIndex`): one render-complete semaphore. The presentation engine waits on it and the CPU can't observe that wait, so it follows the image count.
- Getters take an index and return one handle by value (bounds-checked with `.at()`).
- Uses a local guard object so a failure part-way through construction does not leak.

### 10. `Pipeline` — `Pipeline.hpp/.cpp`
**Responsibility:** the compiled graphics pipeline state (the "recipe" for a draw).
- Loads SPIR-V from disk with `readFile` and `createShaderModule` (`shaderModule.hpp/.cpp`). Shader modules are transient, so they are destroyed via a function-scoped `ShaderModuleGuard`.
- Configures the fixed-function state: vertex input (empty, since positions are hardcoded in `triangle.vert`), input assembly, viewport and scissor, rasterizer, multisampling, and color blending.
- Owns an empty `VkPipelineLayout` (no descriptors or push constants yet).
- Chains `VkPipelineRenderingCreateInfo` with the color attachment format onto the create info. There is no render pass object, so `renderPass = VK_NULL_HANDLE`.
- **Pipelines are baked:** changing the viewport, shaders, or blending means a different pipeline, unless the state is declared dynamic.

### 11. `CommandBuffers` — `CommandBuffers.hpp/.cpp`
**Responsibility:** record the drawing commands once, up front.
- One `VkCommandPool` for the graphics family, plus one `VkCommandBuffer` per swapchain image, allocated in a single batched call.
- Per buffer, records:
  1. A barrier from `UNDEFINED` to `COLOR_ATTACHMENT_OPTIMAL`.
  2. `vkCmdBeginRendering` with a `VkRenderingAttachmentInfo` (clear/store on that image's view).
  3. `vkCmdBindPipeline` and `vkCmdDraw(3, 1, 0, 0)`.
  4. `vkCmdEndRendering`.
  5. A barrier from `COLOR_ATTACHMENT_OPTIMAL` to `PRESENT_SRC_KHR`.
- The destructor destroys only the pool, which frees the buffers.
- Reads the swapchain's images and views without owning them.

### 12. `App` — `App.hpp/.cpp`
**Responsibility:** the orchestrator. It owns everything, wires the dependencies, and runs the loop.
- Constructor helpers: `pickPhysicalDevice`, `pickQueueFamilies`, `pickSwapchainSupport`.
- Owns `currentFrame`, the frame-slot counter (wraps at `MAX_FRAMES_IN_FLIGHT - 1`).
- `run()` loops on `glfwPollEvents()` and `drawFrame()`, then calls `vkDeviceWaitIdle` once after the loop, so the GPU is idle before the destructors run.
- **Member order is load-bearing.** Reordering the fields can break construction or teardown order.

---

## 4. The frame, step by step (`App::drawFrame`)

```
vkWaitForFences        fences[currentFrame]: this slot's previous work is done
vkResetFences          re-arm that fence
vkAcquireNextImageKHR  signals imageAvailable[currentFrame]; returns imageIndex
vkQueueSubmit          runs commandBuffers[imageIndex]
                         waits:   imageAvailable[currentFrame] @ COLOR_ATTACHMENT_OUTPUT
                         signals: renderComplete[imageIndex] + fences[currentFrame]
vkQueuePresentKHR      shows the image
                         waits:   renderComplete[imageIndex]
currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT
```

`currentFrame` is chosen by us; `imageIndex` is chosen by the driver.

---

## 5. Known gaps and likely next steps

| Gap | Effect | Direction |
|-----|--------|-----------|
| Positions hardcoded in the shader | No real geometry | Vertex buffer, then staging buffer, index buffer, and UBO with descriptor sets |
| No depth buffer, textures, or model loading | Nothing 3D yet | Follows the vertex-buffer work |

Longer-term roadmap (static mesh → skeletal animation → blending/IK → Jolt cloth)
is in [`../project-notes.md`](../project-notes.md).

---

## 6. Conventions

- Designated initializers for every Vulkan struct, in declaration order.
- Every handle is initialized to `VK_NULL_HANDLE`.
- `std::optional` for "not found yet".
- `p`-prefixed constructor parameters where they would shadow a member.
- **A class only destroys what it created.**
- Lowercase-named files (`physicalDevice`, `queueFamilies`, `swapchainSupport`, `shaderModule`) are free-function helpers. Capitalized files are RAII classes.
