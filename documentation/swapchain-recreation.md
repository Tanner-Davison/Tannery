# Swapchain Recreation

**Status:** COMPLETE (roadmap step 2 of 4: frames in flight ✔ → swapchain recreation ✔ → vertex buffers → index/uniform buffers).

## What was implemented

- `Swapchain`, `SyncObjects`, `Pipeline` and `CommandBuffers` are `std::unique_ptr` members of `App`
  (declaration order unchanged, so teardown order is unchanged). `unique_ptr` has no per-access
  runtime overhead beyond one pointer load; one allocation per recreation.
- `App::recreateSwapchain()`: wait out a `0x0` framebuffer (`glfwWaitEvents`), `vkDeviceWaitIdle`,
  re-query `SwapchainSupport` (the cached `currentExtent` is stale after a resize), then reset and
  rebuild dependents in order. The old swapchain must be destroyed first because no `oldSwapchain`
  is passed.
- Viewport and scissor are dynamic pipeline state (`VkPipelineDynamicStateCreateInfo`) and recorded
  per command buffer (`vkCmdSetViewport` / `vkCmdSetScissor`), so the pipeline survives a resize
  and is not rebuilt.
- `drawFrame`: acquire returning `OUT_OF_DATE` recreates and skips the frame; the fence is reset
  only after acquire succeeds (otherwise a failed acquire strands an unsignaled fence and the next
  wait hangs); present returning `OUT_OF_DATE`/`SUBOPTIMAL` recreates.
- A GLFW framebuffer-size callback (via `glfwSetWindowUserPointer`) sets a `frameBufferResized`
  flag, checked at the top of `drawFrame`. Needed because Wayland/NVIDIA does not reliably return
  `OUT_OF_DATE` on resize.
- `GLFW_RESIZABLE` had to be enabled in `Window.cpp`.

## Findings

- Measured: recreate about 7 ms, acquire about 8 ms (one vsync interval). No blocking in the
  frame loop.
- **Root cause of the laggy resize: native Wayland.** With the same renderer, forcing X11
  (`glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11)` before `glfwInit`, running via XWayland) made
  live resizing fast and responsive. On native Wayland the window only takes its new size after the
  client commits a buffer at that size, which on this GNOME + GLFW 3.4 setup lagged about half a
  second. Tradeoff: forcing X11 makes `glfwInit` fail where no X server/XWayland exists.
- Not done (optional): pass `oldSwapchain`, rebuild `SyncObjects` only when the image count
  changes, skip recreation when the extent is unchanged.

## Mistakes worth remembering

- Rebuilding the pipeline on every recreate; making viewport/scissor dynamic only helps once the
  rebuild is actually deleted.
- Relying on `OUT_OF_DATE`/`SUBOPTIMAL` alone; some platforms never return them on resize.
- Handling the resize flag after present (one stale-size frame per step) instead of before draw.

## The problem

A swapchain is created for one specific surface size and state. When the window is resized,
minimized, or the surface otherwise changes, the swapchain can become unusable or mismatched:

- `vkAcquireNextImageKHR` / `vkQueuePresentKHR` return `VK_ERROR_OUT_OF_DATE_KHR`
  (the swapchain can no longer be used and **must** be recreated), or
- `VK_SUBOPTIMAL_KHR` (still works, but no longer matches the surface well).

The current `drawFrame` ignores every `VkResult`, so a resize is unhandled.

## What depends on the swapchain

Anything built from the swapchain's images, format, or extent has to be rebuilt with it:

| Object | Depends on |
|---|---|
| `VkSwapchainKHR` | surface capabilities, framebuffer size |
| `VkImageView`s | the swapchain's images |
| `CommandBuffers` (pre-recorded per image) | image views, images, extent |
| `Pipeline` | extent (baked into the viewport/scissor) and format |
| render-complete semaphores | swapchain image count |

Questions to work through: which of these truly must be rebuilt, and which can be kept if the
format is unchanged? What could make the pipeline not need rebuilding?

## Things to think about

1. **Minimized window:** the framebuffer size is `0×0`. A swapchain can't have a zero extent.
   What should the app do while minimized?
2. **In-flight work:** the old swapchain's images may still be in use by the GPU. What must
   you wait for before destroying anything?
3. **Fence already reset:** in `drawFrame` the fence is reset *before* acquire. If acquire
   fails and you return early, what state is the fence in? What happens on the next frame?
4. **`oldSwapchain`:** `VkSwapchainCreateInfoKHR::oldSwapchain` exists for a reason.
5. **Who detects the resize:** the acquire/present result, a GLFW framebuffer-size callback, or both?

## Reading

- Vulkan Tutorial, "Swap chain recreation": https://vulkan-tutorial.com/Drawing_a_triangle/Swap_chain_recreation
- Spec: `vkAcquireNextImageKHR`, `vkQueuePresentKHR` return codes; `VkSwapchainCreateInfoKHR` (`oldSwapchain`)
- GLFW: `glfwSetFramebufferSizeCallback`, `glfwGetFramebufferSize`, `glfwWaitEvents`
- Vulkan Cookbook (2nd ed.): the swapchain creation and recreation recipes
