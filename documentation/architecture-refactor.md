# Architecture Refactor: splitting `App` into layers

**Status:** COMPLETE. Builds, runs clean under validation, and resizing/minimizing still works. `App.cpp` went from about 285 lines to about 35.

## Why

`App` had five jobs: owning the window and loop, setting up the GPU, creating and uploading meshes,
owning everything that depends on the swapchain, and drawing frames and rebuilding on resize. Every
planned feature (descriptors, a camera, lights, many meshes) would have landed in the same class.

## The structure

```
App → Renderer → GraphicsContext → Vulkan/GLFW/VMA        (arrows only point down)
Mesh: a resource created through GraphicsContext and drawn by Renderer
```

- **GraphicsContext**: instance, surface, physical device, queue families, logical device, allocator,
  plus `createDeviceLocalBuffer` (staging upload) and `waitIdle`. Lives for the whole program.
- **Mesh**: vertex buffer + index buffer + index count (taken from `indices.size()`).
- **Renderer**: swapchain, sync objects, pipeline, command buffers, `imagesInFlight`, `currentFrame`,
  `drawFrame`, `recreateSwapchain`, `buildCommandBuffers`.
- **App**: window, the quad data, the resize callback, and the loop.

`App`'s member order (`window`, `context`, `mesh`, `renderer`) is the dependency order, so teardown is
automatically renderer, mesh, context, window.

## Small changes made alongside the move

- `buildCommandBuffers()`: one place constructs `CommandBuffers` (startup and resize), replacing two
  copies that had to stay in sync.
- `CommandBuffers` takes a `const Mesh&` instead of three loose arguments, which also removed the
  `sizeBytes() / sizeof(uint16_t)` expression and the "uint16_t in three places" coupling.
- `vkQueueSubmit` result is now checked (the last unchecked call in the frame).
- `currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT` replaced the `if/else`.
- `Mesh` rejects empty vertex or index data (a zero-size buffer is invalid).

## Mistakes worth remembering

- New `.cpp` file missing from `SOURCES` again (`Renderer.cpp`): a link error ("undefined reference"),
  not a compile error. Add it to CMake when creating the file.
- A duplicated initializer-list entry (`window(...)` twice) from copy-paste.

## Dependency rule to keep

Lower layers never include or know about higher ones. When adding a feature, ask which layer it
belongs to: GPU setup (`GraphicsContext`), drawing logic (`Renderer`), a thing to draw (`Mesh` or
later a scene layer), or program flow (`App`).

## Possible next refinements

- A scene/entity layer above `Renderer` once there are several meshes or objects.
- Per-frame-slot command buffers re-recorded each frame (removes `imagesInFlight`), a natural fit
  with uniform buffers.
- Moving files into folders (`platform/`, `rhi/`, `renderer/`) once the file count justifies it.
