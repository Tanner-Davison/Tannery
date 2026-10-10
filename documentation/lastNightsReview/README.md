# Last Night's Review (2026-10-08)

Last night most of this code was written by Claude because you were tired. This folder is the
review: one file per topic, in the order they build on each other. Read one, then try the
**Check yourself** questions *before* opening the answer key.

## The files

| # | File | Topic | New Vulkan? | Rewrite it yourself? |
|---|---|---|---|---|
| 1 | [01-mipmaps.md](01-mipmaps.md) | Mip generation: per-level layouts, the blit loop | Yes | Yes: the loop |
| 2 | [02-camera.md](02-camera.md) | `Camera`: `forward()`, `move()`, pitch clamp, `lookAt`, projection | No (math) | Yes: `forward()` and `move()` |
| 3 | [03-camera-controller.md](03-camera-controller.md) | `CameraController`: actions to movement, mouse, cursor capture state machine | No | Yes: `update()` |
| 4 | [04-input-map-and-settings.md](04-input-map-and-settings.md) | `InputMap` (action mapping) and `Settings` (persistence) | No | Understand only |
| 5 | [05-fixed-timestep.md](05-fixed-timestep.md) | `FixedTimestep` and why the camera is not in it | No | Yes: `advance()` |
| 6 | [06-unit-tests.md](06-unit-tests.md) | `tests/engine_tests.cpp`: read one, add one | No | Add one test |
| 7 | [07-tracy-profiling.md](07-tracy-profiling.md) | Tracy: CPU zones, GPU timestamp zones | Yes (timestamps) | Add one zone |
| 8 | [08-open-items.md](08-open-items.md) | Unanswered questions and next lessons | Mixed | n/a |
| 9 | [09-glsl-shaders.md](09-glsl-shaders.md) | GLSL shaders: syntax, how they connect to the engine (the contracts), how they grow | Yes | Edit a shader (Part 6) |
| 10 | [10-pipeline-command-buffers-sync.md](10-pipeline-command-buffers-sync.md) | `Pipeline` (how), `CommandBuffers` (what), `SyncObjects` (when), and one frame through all three | Review of earlier work | Trace one frame (Exercise 3) |

## Who wrote what

| Piece | Written by |
|---|---|
| `Camera.hpp` design (members, `move`/`rotate` split) | You (reviewed and adjusted together) |
| `Camera::forward()` | You |
| mip blit loop (`vkCmdBlitImage`, `VkImageBlit`, the barrier fields) | You, from a snippet typed in the CLI; Claude fixed the **order** of the blocks |
| mip create-info edits (`mipLevels`, usage flags, view `levelCount`, sampler LOD) | Claude (repeat syntax) |
| `Camera.cpp` (the rest), `CameraController`, `InputMap`, `Settings`, `FixedTimestep` | Claude |
| Tests, Tracy integration (CPU and GPU zones), `GpuProfiler` | Claude |
| All docs | Claude |

The pieces in the last two rows are the **review debt**.

## How to use these files

1. Read the file's **Big picture** and look at the diagram.
2. Go through the **Steps**, with the code open in the editor next to it.
3. Answer **Check yourself** in your own words, out loud or on paper.
4. Open the answer key and compare. If an answer was off, re-read that step, not the whole file.
5. Do the **Rewrite it** exercise for the files marked "Yes" above: delete the function, close the
   file, and write it again from your understanding. Yours does not have to match.

## Added later the same day

Files 9 (GLSL shaders) and 10 (`Pipeline` / `CommandBuffers` / `SyncObjects`) were added at your
request. File 10 is the "how a frame actually works" file; read it after 1 and 9.

File 9 (GLSL shaders): It is best read **after** file 2 (coordinate
spaces) and before file 7, since the shaders are where the camera math actually runs.

## One-page cheat sheet

```
Shaders (build time -> startup)
  data/shaders/*.vert|.frag --glslangValidator -V--> build/shaders/*.spv --vkCreateShaderModule--> VkPipeline
  contracts: vertex in <-> Vertex.hpp | UBO block <-> CameraUBO + FrameDescriptors |
             sampler2D <-> MaterialDescriptors | vertex out <-> fragment in | out 0 -> swapchain image

Texture upload (load time, once)
  copy -> level 0 -> [for each level: DST->SRC barrier, blit, SRC->SHADER_READ barrier] -> last level

Each frame (App::run)
  glfwPollEvents()                     callbacks fire here
  dt = min(now - last, 0.1)
  fixedClock.advance(dt) -> N x fixedUpdate    (empty for now)
  cameraController.update(window, camera, input, dt)
        actions -> direction -> Camera::move
        mouse delta -> Camera::rotate        (skipped while captured is false or framesToSkip > 0)
  renderer.drawFrame(makeCamera())     camera.viewMatrix() -> CameraUBO -> GPU
        CPU zones:  wait fence | acquire | update uniforms | record | submit | present
        GPU zones:  gpu frame > render > draw indexed
```

## Controls (for testing)

`W A S D` move, `E`/`Space` up, `Q`/`Shift` down, mouse to look, scroll changes speed,
`[` `]` change mouse sensitivity, click to recapture the cursor, `Esc` quits.
