# 7. Tracy profiling: CPU zones and GPU timestamp zones

Files: `core/include/Profiling.hpp`, `core/include/GpuProfiler.hpp`, `core/src/GpuProfiler.cpp`,
`core/src/Renderer.cpp`, `core/src/CommandBuffers.cpp`, `core/src/App.cpp`, `CMakeLists.txt`,
`deps/tracy` (git submodule, v0.14.1, BSD-3-Clause)
Longer reference: `documentation/profiling.md`

## Big picture

A **profiler** shows where time goes. Tracy has two halves:

```
tannery (client, linked into the engine)                 Tracy viewer (separate program)
  PROFILE_SCOPE("draw")       --+
  PROFILE_GPU_SCOPE(...)        +- TracyClient -- localhost TCP -->  timeline, stats, frame graph
  PROFILE_FRAME()             --+
```

The engine records **zones** (named time spans) and **frame marks**; the viewer connects and draws
them. The viewer must be **the same version** as the client (the protocol changes between
releases), which is why both are built from `deps/tracy`.

Two kinds of zone:

| | Measures | How |
|---|---|---|
| **CPU zone** | how long the CPU spent in a scope | timestamps taken on the CPU |
| **GPU zone** | how long the **GPU** spent on work | timestamps the GPU writes itself into a query pool |

## Steps

### Step 1: build wiring

```cmake
option(FORGE3D_PROFILING "Instrument the engine with Tracy zones" ON)
set(TRACY_ENABLE ${FORGE3D_PROFILING} CACHE BOOL "" FORCE)
set(TRACY_ON_DEMAND ON CACHE BOOL "" FORCE)       # collect nothing until the viewer connects
set(TRACY_ONLY_LOCALHOST ON CACHE BOOL "" FORCE)  # never listen on the network
add_subdirectory(deps/tracy EXCLUDE_FROM_ALL)
...
target_link_libraries(tannery PRIVATE glfw Vulkan::Vulkan Tracy::TracyClient)
```

| Option | Why |
|---|---|
| `FORGE3D_PROFILING=OFF` | every macro compiles to nothing: zero cost in a shipping build |
| `TRACY_ON_DEMAND` | no data is gathered until a viewer connects, so overhead is near zero otherwise |
| `TRACY_ONLY_LOCALHOST` | Tracy's default listens on **all network interfaces**; for an engine you might sell, off by default |

### Step 2: one include, a few macros (`Profiling.hpp`)

```cpp
#include <vulkan/vulkan.h> // must come before TracyVulkan.hpp
#include <tracy/Tracy.hpp>
#include <tracy/TracyVulkan.hpp>

#define PROFILE_SCOPE(name)   ZoneScopedN(name)       // CPU: until end of scope; string literal only
#define PROFILE_FRAME()       FrameMark                // end of a rendered frame
#define PROFILE_GPU_SCOPE(ctx, cmd, name) \
    TracyVkNamedZone(ctx, PROFILE_CONCAT(gpuZone_, __LINE__), cmd, name, true)
#define PROFILE_GPU_COLLECT(ctx, cmd) TracyVkCollect(ctx, cmd)
```

Call sites use **only** these macros. Nothing else in the engine includes Tracy, so the profiler
can be swapped or removed in one place. `PROFILE_CONCAT(gpuZone_, __LINE__)` makes a unique
variable name per zone from the line number (so **two zones can't share a source line**).

### Step 3: CPU zones (`App::run`)

```cpp
{
    PROFILE_SCOPE("events");
    glfwPollEvents();
}
...
{
    PROFILE_SCOPE("update");
    ... fixed ticks and camera ...
}
{
    PROFILE_SCOPE("draw");
    renderer.drawFrame(makeCamera());
}
PROFILE_FRAME();
```

A zone is a **scope object**: it starts timing when constructed and stops when destroyed at the
closing brace. The extra `{ }` blocks exist to end the zone exactly where you want.

### Step 4: nested CPU zones (`Renderer::drawFrame`)

```cpp
VkResult res;
{
    PROFILE_SCOPE("wait for frame fence");
    res = vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
}
...
{
    PROFILE_SCOPE("acquire swapchain image");
    res = vkAcquireNextImageKHR(device, swapchain->handle(), UINT64_MAX, ..., &imageIndex);
}
{ PROFILE_SCOPE("update uniforms");  descriptors.update(currentFrame, camera); }
{ PROFILE_SCOPE("record commands");  commandBuffer = commandBuffers.record(...); }
{ PROFILE_SCOPE("queue submit");     res = vkQueueSubmit(...); }
{ PROFILE_SCOPE("queue present");    res = vkQueuePresentKHR(...); }
```

`res` is declared **outside** the braces so the result survives after the zone ends. The tree:

```
draw
 +- wait for frame fence      CPU blocked until the GPU finished this slot's last frame
 +- acquire swapchain image   can block on vsync / the presentation engine
 +- update uniforms
 +- record commands
 +- queue submit
 +- queue present
```

### Step 5: what GPU zones are

GPU work is **recorded** into a command buffer by the CPU and **executed later** by the GPU. A CPU
timer around `record()` would only measure recording. A GPU zone puts a **timestamp command** into
the buffer, so the GPU records the time itself when it reaches that point:

```
record time (CPU)           GPU executes later
 PROFILE_GPU_SCOPE  ->  vkCmdWriteTimestamp(start) ... work ... vkCmdWriteTimestamp(end)
                                  |                                   |
                                  +----- stored in a query pool ------+
 PROFILE_GPU_COLLECT -> reads back finished timestamps and sends them to the viewer
```

### Step 6: creating the GPU context (`GpuProfiler`)

```cpp
VkCommandPoolCreateInfo poolInfo{};
poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
poolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
poolInfo.queueFamilyIndex = pContext.queueFamilies().graphicsFamilyIndex.value();
... allocate one command buffer from that pool ...

// "Uncalibrated" form: uses the GPU's own timestamp clock (VK_TIME_DOMAIN_DEVICE_EXT)
context = TracyVkContext(pContext.physicalDeviceHandle(), device, pContext.graphicsQueue(), cmd);
TracyVkContextName(context, "Graphics", 8);

vkDestroyCommandPool(device, pool, nullptr); // also frees `cmd`
```

Tracy records, **submits and waits** on the command buffer you give it while it sets up: it resets
a query pool and takes a first timestamp to line the GPU clock up with the CPU clock. Hence the
throwaway pool (with `RESET_COMMAND_BUFFER` so it can begin the same buffer several times). The
`GpuProfiler` is owned by `Renderer`, created just before `CommandBuffers`, which borrow its
handle (`TracyVkCtx`).

"Uncalibrated" means durations are accurate but the GPU clock may drift against the CPU clock
over a long session; the calibrated variant needs `VK_EXT_calibrated_timestamps` enabled.

### Step 7: GPU zones in the command buffer (`CommandBuffers::record`)

```cpp
check(vkBeginCommandBuffer(cmd, &beginInfo), "vkBeginCommandBuffer");
{
    PROFILE_GPU_SCOPE(profileCtx, cmd, "gpu frame");
    ... barrier, rendering info, depth barrier ...
    {
        PROFILE_GPU_SCOPE(profileCtx, cmd, "render");
        vkCmdBeginRendering(cmd, &renderingInfo);
        ... bind pipeline, descriptor sets, vertex/index buffers, viewport, scissor ...
        {
            PROFILE_GPU_SCOPE(profileCtx, cmd, "draw indexed");
            vkCmdDrawIndexed(cmd, mesh.indexCount(), 1, 0, 0, 0);
        }
        vkCmdEndRendering(cmd);
    }
    ... present barrier ...
}

// Outside every zone and the render pass: read back finished timestamps
PROFILE_GPU_COLLECT(profileCtx, cmd);

check(vkEndCommandBuffer(cmd), "vkEndCommandBuffer");
```

**The four rules:**

1. The command buffer must be **recording** when a zone opens *and* when it closes. That is why
   the body is wrapped in a block: the `gpu frame` zone has to end **before**
   `vkEndCommandBuffer`.
2. `PROFILE_GPU_COLLECT` goes **after** the zones have closed and **outside** the render pass,
   because it also records `vkCmdResetQueryPool` for the queries it consumed, and that is not
   allowed inside a render pass.
3. Two zones may not share a source line.
4. The queue family must support timestamps (`timestampValidBits > 0`); graphics queues do.

GPU results arrive **late**: the GPU is still working on frames the CPU has already moved on from,
so `COLLECT` reads whatever has finished, and GPU bars show a frame or two behind.

### Step 8: reading a real capture

Recorded 2026-10-08 on a 240 Hz monitor (4,284 frames in 17.88 s):

| CPU zone | Mean per call |
|---|---|
| `acquire swapchain image` | **3.99 ms** (95.5% of zone time) |
| `wait for frame fence` | 66 us |
| `record commands` | 46 us |
| `queue present` | 32 us |
| everything else | under 25 us each |

| GPU zone | Mean per call |
|---|---|
| `gpu frame` | 118 us (2.84% of the session) |
| `render` | 115 us |
| `draw indexed` | 40 us |

What it says:

- Frame time was **4.17 ms = 1 / 240 Hz** (an earlier capture on a 120 Hz display was 8.31 ms =
  1 / 120 Hz). Frame time equal to the refresh period is the signature of a **vsync-paced** loop.
- `wait for frame fence` was tiny: the CPU never waited on the GPU, so the **GPU is not the
  bottleneck**.
- `acquire swapchain image` is the loop sleeping until the display releases a buffer.
- Real work: about 0.15 ms of CPU and 0.12 ms of GPU in a 4.17 ms frame. "240 fps" says nothing
  about headroom; the zones do.
- Inside the GPU's 118 us, `draw indexed` is only about a third of `render`; the rest is clearing
  and loading attachments, descriptor binds and viewport setup. For this scene, clearing costs
  more than drawing.

## How to use it

```bash
./deps/tracy/profiler/build/tracy-profiler     # start the viewer, then run the engine, then Connect
```

Statistics window: the **Instrumentation** button shows CPU zones, **GPU** shows GPU zones.
"Self only" excludes child zones. `Collect` (red) is Tracy's own cost for GPU profiling (about
2.4 us per frame).

## Gotchas

- Zone names must be string literals.
- The `draw` zone's time is dominated by the vsync wait; do **not** read it as GPU cost.
- If `FORGE3D_PROFILING=OFF`, `TracyVkCtx` is `void*` and all macros vanish, so the code compiles
  both ways.
- GPU zones for the texture upload / mip loop don't exist (that code uses `immediateSubmit`).

## Check yourself

1. Why can't a CPU timer around `commandBuffers.record(...)` tell you how long the GPU takes?
2. Why does `PROFILE_GPU_COLLECT` come after the `gpu frame` block closes and before
   `vkEndCommandBuffer`?
3. A capture shows `wait for frame fence` = 5 ms and `acquire swapchain image` = 0.1 ms. What does
   that suggest, compared with last night's capture?
4. Why is `res` declared outside the `{ PROFILE_SCOPE(...) ... }` block in `drawFrame`?
5. What does `TRACY_ON_DEMAND` buy you?

<details>
<summary>Answer key</summary>

1. `record()` only writes commands into a buffer; the GPU executes them later. The CPU timer
   measures recording. A GPU zone puts timestamp commands in the buffer so the GPU measures
   itself.
2. The zone's closing timestamp must be written while the buffer is still recording, so the zone
   has to end before `vkEndCommandBuffer`; and collecting records a query-pool reset that must be
   outside the render pass. Both are inside the recording window but after the zones.
3. A long fence wait means the CPU is waiting for the GPU to finish an earlier frame, i.e. the
   **GPU** is the bottleneck. Last night it was the opposite: tiny fence wait, long acquire, which
   means the display's vsync was pacing the loop.
4. So its value is still in scope after the zone's block ends and can be checked for errors.
5. Nothing is collected and nothing is sent until a viewer connects, so overhead is close to zero
   the rest of the time.

</details>

## Exercise

Add a CPU zone named `"build camera"` around the `makeCamera()` call in `App::run`. Rebuild, run
with the viewer connected, and find it in the Statistics window. Then guess its mean time before
you look.
