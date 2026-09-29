# Swapchain Recreation

**Status:** in progress (roadmap step 2 of 4: frames in flight ✔ → swapchain recreation → vertex buffers → index/uniform buffers).

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
