# Vertex Buffers

**Status:** COMPLETE (roadmap step 3 of 4: frames in flight ✔ → swapchain recreation ✔ → vertex buffers ✔ → index/uniform buffers). Renders a red/green/blue gradient triangle from a real vertex buffer, clean under validation, including after resizes.

## What was implemented

- **VMA** (Vulkan Memory Allocator, MIT, apt `libvulkan-memory-allocator-dev`) chosen over hand-written
  memory allocation. `vma.cpp` is the single translation unit with `VMA_IMPLEMENTATION`;
  `Allocator` (RAII, declared right after `device` in `App`) owns the `VmaAllocator`.
- `Buffer` (RAII): `vmaCreateBuffer` / `vmaDestroyBuffer`, with usage and VMA allocation flags passed in.
- `copyBuffer` (free function): its own transient command pool, one-time command buffer,
  `vkCmdCopyBuffer`, submit, `vkQueueWaitIdle`.
- `App::createVertexBuffer`: CPU-visible staging buffer (`HOST_ACCESS_SEQUENTIAL_WRITE`, filled via
  `vmaCopyMemoryToAllocation`) copied into a GPU-local vertex buffer; staging is destroyed on scope exit.
- `Vertex.hpp`: `pos` (vec2) + `color` (vec3), binding and attribute descriptions.
  `Pipeline` uses them in its vertex input state; `CommandBuffers` records `vkCmdBindVertexBuffers`;
  the shaders read `location 0/1` inputs.
- The `vertexBuffer` member sits after `allocator` and before `commandBuffers`, and its handle is
  passed at **both** places `CommandBuffers` is built (constructor and `recreateSwapchain`).

## Mistakes worth remembering

- `Buffer` first used an uninitialized `allocator` member, never stored `size`, and declared a
  destructor with no definition. None showed up because nothing constructed a `Buffer` yet:
  always instantiate a new class to test it.
- New `.cpp` files not added to CMake `SOURCES` (`Allocator.cpp`, `Buffer.cpp`, `copyBuffer.cpp`)
  cause link errors ("undefined reference"), not compile errors.
- Forgetting the new `CommandBuffers` argument in `recreateSwapchain`.
- `std::format` accepts extra arguments silently: a message with no `{}` drops the `VkResult`.

## Goal

Move the triangle's vertex data out of `triangle.vert` and into a GPU buffer that the pipeline
reads, so the engine can draw arbitrary geometry.

## The data path

```
CPU vertex array ──copy──▶ staging buffer (HOST_VISIBLE | HOST_COHERENT)
                                   │  vkCmdCopyBuffer (in a one-time command buffer)
                                   ▼
                      vertex buffer (DEVICE_LOCAL) ──▶ vertex shader inputs
```

A Vulkan buffer is two objects you bind yourself:

| Object | Role |
|---|---|
| `VkBuffer` | Handle + size + usage flags. Owns no memory. |
| `VkDeviceMemory` | The actual allocation, from a chosen memory type. |
| `vkBindBufferMemory` | Connects the two. |

Memory type selection: `vkGetBufferMemoryRequirements` gives a `memoryTypeBits` mask;
`vkGetPhysicalDeviceMemoryProperties` lists the types; a `findMemoryType` helper picks one that
is allowed by the mask and has the required property flags.

## Stages

1. **Vertex layout** (`Vertex.hpp`): `Vertex` struct, `VkVertexInputBindingDescription`,
   `VkVertexInputAttributeDescription` array.
2. **Buffer helper**: create `VkBuffer`, allocate `VkDeviceMemory`, bind; `findMemoryType`.
3. **Staging upload**: map/copy into a staging buffer, `vkCmdCopyBuffer` into a device-local buffer.
4. **Use it**: pipeline vertex input state, vertex shader `layout(location = n) in ...`,
   `vkCmdBindVertexBuffers` before `vkCmdDraw` in `CommandBuffers`.

## Reading

- Vulkan Tutorial, vertex input description: https://vulkan-tutorial.com/Vertex_buffers/Vertex_input_description
- Vulkan Tutorial, vertex buffer creation: https://vulkan-tutorial.com/Vertex_buffers/Vertex_buffer_creation
- Vulkan Tutorial, staging buffer: https://vulkan-tutorial.com/Vertex_buffers/Staging_buffer
- Spec: `vkCreateBuffer`, `vkAllocateMemory`, `vkBindBufferMemory`, `vkCmdCopyBuffer`,
  `vkCmdBindVertexBuffers`
- Vulkan Cookbook (2nd ed.): the buffer and memory chapter
