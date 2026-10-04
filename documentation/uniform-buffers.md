# Uniform Buffers and Descriptor Sets

**Status:** COMPLETE (roadmap step 4b). A quad spins under a perspective camera, driven by per-frame uniform data; builds and runs.

## Overview

A uniform buffer is a small GPU buffer the CPU rewrites every frame so shaders can read changing
data (here, model/view/proj matrices). A **descriptor set** is the connector between a shader's
binding number and a specific buffer ("a wall of numbered sockets with cables plugged in").

- **Layout** (`VkDescriptorSetLayout`): the plan of the wall (binding 0 = uniform buffer, vertex
  stage). The baked pipeline needs it at creation.
- **Pool** (`VkDescriptorPool`): fixed-size reservation (`maxSets` and per-type `descriptorCount`).
- **Sets**: one per frame slot, allocated from the pool with the layout.
- **Cables** (`vkUpdateDescriptorSets`): plug slot i's uniform buffer into set i, once at startup.

## What was implemented

- `Buffer::mappedData()` / `write()`: persistently mapped host-visible buffers
  (`VMA_ALLOCATION_CREATE_MAPPED_BIT`), `memcpy` + `vmaFlushAllocation`.
- `UniformData.hpp`: `CameraUBO { mat4 model, view, proj }` (192 bytes, mirrors the shader block).
- `FrameDescriptors`: layout, pool, one set and one uniform buffer per frame slot, `update(slot, data)`.
- `Pipeline` takes the `VkDescriptorSetLayout` for its pipeline layout.
- `triangle.vert`: `layout(binding = 0) uniform CameraUBO {...} ubo;`, position =
  `proj * view * model * vec4(pos, 0, 1)`.
- `CommandBuffers` redesigned: one command buffer per **frame slot**, created once, re-recorded each
  frame by `record(frameIndex, image, imageView, ...)`. It no longer depends on the swapchain.
- `Renderer`: swapchain and `SyncObjects` are rebuilt on resize; `FrameDescriptors`, `Pipeline` and
  `CommandBuffers` live for the whole program. `imagesInFlight` is gone.
- `App` owns the camera (`makeCamera`) and passes a `CameraUBO` to `Renderer::drawFrame`.
- CMake: `GLM_FORCE_DEPTH_ZERO_TO_ONE` so GLM matches Vulkan's 0..1 depth range; `proj[1][1] *= -1`
  accounts for Vulkan's downward Y.

## Key ideas

- **Indexing rule:** `currentFrame` (chosen by us) picks the command buffer, descriptor set, uniform
  buffer and fence. `imageIndex` (chosen by the driver) picks the render target and render-complete
  semaphore.
- **Why one uniform buffer per frame slot:** with 2 frames in flight the CPU writes the next frame's data
  while the GPU may still read the previous frame's. The slot's fence wait guarantees the GPU is done
  with that slot's buffer before it is overwritten.
- **Memory:** host-visible (system RAM, CPU-mappable) is right for small data rewritten every frame;
  device-local (VRAM) with a staging copy is right for large, stable data (meshes, textures).
- **Reset a fence only when a submit that will re-signal it is certain** (after a successful acquire).

## Mistakes worth remembering

- A `check()` helper that threw unconditionally (missing the `if (res != VK_SUCCESS)`).
- Missing `sType` on `VkDescriptorSetLayoutCreateInfo`.
- A destructor declared but never defined (link error), again.
- `maxSets` counts sets; the per-type `descriptorCount` counts descriptors across all sets.
- Inconsistent naming after a rename: the class, header, source, CMake entry and users must all agree.

## Next

Depth buffer, then textures (a second descriptor type in the same pool and layout), then model loading
and lighting.
