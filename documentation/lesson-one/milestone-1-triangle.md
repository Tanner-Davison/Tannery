# Milestone 1: Hardcoded Triangle

## Goal

Get a single hardcoded triangle rendering on screen using raw Vulkan C API +
GLFW. This exercises the full minimum pipeline: instance → physical device →
logical device → surface/swapchain → render pass → graphics pipeline →
framebuffers → command buffers → sync/present.

## Lesson sequence

1. **Project scaffolding** — `CMakeLists.txt` (find_package for Vulkan/glfw3),
   `main.cpp` skeleton with a GLFW window and a bare render loop (no Vulkan
   calls yet). Confirms the toolchain builds and runs before adding any
   Vulkan complexity.
2. **VkInstance** — the entry point object for the Vulkan library. Requires
   `VkApplicationInfo` + `VkInstanceCreateInfo`, and the GLFW-required
   extension list (`glfwGetRequiredInstanceExtensions`).
3. **Validation layers** — `VK_LAYER_KHRONOS_validation`, debug messenger,
   why they matter (Vulkan does almost no error checking by design — the
   validation layer is where those checks actually live during development).
4. **Physical device + logical device** — enumerating GPUs
   (`vkEnumeratePhysicalDevices`), picking one, querying queue families,
   creating a `VkDevice` and retrieving queue handles.
5. **Surface + swapchain** — `VkSurfaceKHR` (GLFW-created), swapchain
   capabilities/formats/present modes, `VkSwapchainKHR` creation.
6. **Image views + render pass** — how the swapchain images get wrapped for
   use as render targets, and how a render pass describes attachments and
   subpasses.
7. **Graphics pipeline** — shader modules (compiled via `glslang-tools`),
   fixed-function state (vertex input, input assembly, viewport/scissor,
   rasterizer, multisampling, color blending), pipeline layout.
8. **Framebuffers + command buffers** — binding image views to a render
   pass, recording draw commands into a command buffer.
9. **Render loop + sync** — semaphores/fences, `vkAcquireNextImageKHR`,
   `vkQueueSubmit`, `vkQueuePresentKHR`.

## Reference material

- The Vulkan Tutorial (https://vulkan-tutorial.com/) — the canonical
  step-by-step walkthrough this milestone loosely follows in raw C API
  style.
- Vulkan Spec / Registry (https://registry.khronos.org/vulkan/) — the
  authoritative source for every struct and function signature.
- GLFW docs (https://www.glfw.org/documentation.html) — window/surface
  creation, input, required-extension query.
- `vulkaninfo` (already installed) — inspect what your actual GPU/driver
  exposes (extensions, queue families, formats).

## Status

- [x] Step 1 — project scaffolding (now split into `src/` + `include/`)
- [x] Step 2 — `VkInstance` (app info, extensions, create/destroy)
- [x] Step 3 — validation layers (`VK_LAYER_KHRONOS_validation`, debug
      messenger chained into instance create/destroy via `pNext`, proxy
      loaders for the extension functions)
- [x] Step 4 — physical device selection (`physicalDevice.cpp`, `queueFamilies.cpp`)
- [x] Step 5 — logical device + queues (`logicalDevice.cpp`: `VkDeviceQueueCreateInfo` →
      `VkDeviceCreateInfo` → `vkCreateDevice`, `vkGetDeviceQueue`; destroy-order bug
      caught in `cleanup.hpp` — device must be destroyed before instance)
- [x] Step 6 — window surface (`surface.cpp`: `glfwCreateWindowSurface`; extended
      `queueFamilies.cpp` with `presentFamilyIndex` + `isComplete()`, and
      `logicalDevice.cpp` to dedupe graphics/present family indices via
      `std::set<uint32_t>` before building `VkDeviceQueueCreateInfo` entries —
      RTX 3090 reports both indices as `0`)
- [x] Step 7 — swapchain (`swapchainSupport.cpp`: `chooseSwapSurfaceFormat`,
      `chooseSwapPresentMode`, `chooseSwapExtent`, and `createSwapchain`
      building `VkSwapchainCreateInfoKHR` + `vkCreateSwapchainKHR`; wired the
      previously-unused `getSwapchainSupportDetails` query into `main.cpp`
      and registered `swapchainSupport.cpp` in `CMakeLists.txt` `SOURCES`
      — it had been written but never actually linked into the build; grew
      `cleanup.hpp` to accept and destroy the `VkSwapchainKHR`, ordered
      before the logical device since the swapchain is its child. Also
      added cross-platform `VK_KHR_portability_enumeration` (instance) /
      `VK_KHR_portability_subset` (device) extension support, gated behind
      `#ifdef __APPLE__`, for eventual MoltenVK compatibility)
> **Note (post-Step 7):** the codebase was restructured from flat
> functions + a manual `cleanup.hpp` into RAII wrapper classes — `Window`,
> `VulkanInstance`, `Surface`, `LogicalDevice`, `Swapchain` — owned by a new
> `App` class, whose member declaration order drives correct
> construction/destruction automatically. `cleanup.hpp` and the lowercase
> `windowHandling.cpp`/`vulkanInstance.cpp`/`logicalDevice.cpp`/`surface.cpp`
> files no longer exist; the Step 1–7 entries above describe the codebase as
> it existed *at that point in the lesson sequence*, not its current
> structure. Full writeup in `project-notes.md` ("Current architecture" and
> concept 14).

- [x] Step 8 — image views + render pass (image views folded directly into
      `Swapchain` rather than a separate wrapper class; render pass got its
      own new `RenderPass` class — `VkAttachmentDescription` for the single
      color attachment, `VkAttachmentReference` + `VkSubpassDescription` for
      the one subpass, `VkSubpassDependency` synchronizing it against
      `VK_SUBPASS_EXTERNAL` at the color-attachment-output stage, assembled
      into `VkRenderPassCreateInfo` → `vkCreateRenderPass`. Wired into `App`
      as the last-declared member, since it depends on both `device` and
      `swapchain.formatHandle()`)
- [x] Step 9 — graphics pipeline
      - [x] shader-compile tooling (`data/shaders/triangle.vert`/`.frag`,
        `glslangValidator` wired into CMake via `find_program`/
        `add_custom_command`/`add_custom_target`)
      - [x] `readFile`/`createShaderModule` free functions
        (`core/include/shaderModule.hpp`, lowercase — free-function helper,
        not an RAII class, per the project's naming convention)
      - [x] new `Pipeline` class scaffolded (`Pipeline.hpp`/`.cpp`),
        constructor takes `VkDevice`, `VkRenderPass`, `VkExtent2D`, and the
        vertex/fragment shader paths
      - [x] both `VkPipelineShaderStageCreateInfo` structs built (vertex +
        fragment)
      - [x] combine into `VkPipelineShaderStageCreateInfo shaderStages[2]`
      - [x] vertex input, input assembly, viewport/scissor, rasterizer,
        multisampling, color blending state structs
      - [x] `VkPipelineLayoutCreateInfo` → `vkCreatePipelineLayout`
      - [x] `VkGraphicsPipelineCreateInfo` → `vkCreateGraphicsPipelines`
      - [x] `ShaderModuleGuard` (function-scoped RAII, see concept 17 in
        `project-notes.md`) — exception-safe shader module cleanup on every
        exit path, replacing manual `vkDestroyShaderModule` calls
      - [x] wire `Pipeline` into `App` (last-declared member; `SHADER_DIR`
        compile definition added to CMake so `App.cpp` can build the
        `.spv` paths at runtime via `std::filesystem::path(SHADER_DIR) /
        "triangle.vert.spv"`)

**Step 9 complete.** Build succeeds with no validation errors; app launches
and stays alive (confirmed via `pgrep`) with no visible window yet — expected
on this machine's Wayland session per the gotcha documented above, since
nothing is drawn/presented until Steps 10-11.
- [x] Step 10 — framebuffers + command buffers (`FrameBuffers` — one
      `VkFramebuffer` per swapchain image, each bound to its own single
      `VkImageView` via a per-index loop; `CommandBuffers` — one
      `VkCommandPool` created once, all command buffers allocated in a
      single batched `vkAllocateCommandBuffers` call, then a per-index loop
      records `vkCmdBeginRenderPass`/`vkCmdBindPipeline`/`vkCmdDraw(3,1,0,0)`/
      `vkCmdEndRenderPass` into each one. Both wired into `App`.)
- [x] Step 11 — render loop + sync (new `SyncObjects` class: one
      `imageAvailableSemaphore`, one signaled-at-creation `VkFence`, and a
      `std::vector<VkSemaphore>` of render-complete semaphores — one per
      swapchain image rather than a single shared instance, indexed by the
      acquired `imageIndex`. Wired into `App` after `swapchain`, since it
      depends on `swapchain.imageCountHandle()`. `App::drawFrame()` runs
      `vkWaitForFences` → `vkResetFences` → `vkAcquireNextImageKHR` →
      `vkQueueSubmit` → `vkQueuePresentKHR` each frame; `App::run()` calls
      it every iteration after `glfwPollEvents()`, and adds a single
      `vkDeviceWaitIdle` after the loop exits so shutdown doesn't destroy
      Vulkan objects the GPU is still using. See "Update (later session):
      Step 11 is complete" in `project-notes.md` for the two validation-layer
      bugs hit and fixed along the way — binary semaphore reuse across
      swapchain images, and a parameter-shadowing bug in the sync-objects
      exception-safety guard.)

## Milestone 1: complete

All 11 steps done — a hardcoded triangle renders on screen via the full
Vulkan pipeline (instance → physical device → logical device →
surface/swapchain → render pass → graphics pipeline → framebuffers →
command buffers → sync/present), confirmed with no validation layer errors
on either the steady-state render loop or app shutdown.
