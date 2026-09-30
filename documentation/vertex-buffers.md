# Vertex Buffers

**Status:** in progress (roadmap step 3 of 4: frames in flight ✔ → swapchain recreation ✔ → vertex buffers → index/uniform buffers).

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
