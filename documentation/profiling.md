# Profiling with Tracy

Tracy is a frame profiler. The engine records **zones** (named time spans) and **frame marks**;
a separate **viewer** program connects over a local socket and draws them on a timeline.

License: BSD-3-Clause (compatible with commercial resale). Pinned as a git submodule:
`deps/tracy` at **v0.14.1**.

## How it fits together

```
tannery (client)                          Tracy viewer (separate program)
  PROFILE_SCOPE("draw")      ──┐
  PROFILE_GPU_SCOPE(...)       ├─ TracyClient ── localhost TCP ──►  timeline, stats, frame graph
  PROFILE_FRAME()            ──┘
```

- **Call sites** use only the `PROFILE_*` macros from `core/include/Profiling.hpp`. They map to
  Tracy's `ZoneScopedN`, `FrameMark`, `TracyVkNamedZone` and `TracyVkCollect`. Nothing else in
  the engine includes Tracy, so it can be swapped or removed in one place.
- **CPU zones** measure how long the CPU spent in a scope.
- **GPU zones** measure how long the GPU spent on work recorded into a command buffer. They are
  timestamps the GPU writes itself, so they show *when the GPU actually ran it*, not when the CPU
  recorded it.

## The zones

CPU (`App::run` in `core/src/App.cpp`, `Renderer::drawFrame` in `core/src/Renderer.cpp`):

```
events                              glfwPollEvents
update                              fixed ticks + camera
draw                                renderer.drawFrame
 ├─ wait for frame fence            CPU blocked until the GPU finished this slot's last frame
 ├─ acquire swapchain image         vkAcquireNextImageKHR (can block on vsync/presentation)
 ├─ update uniforms                 camera UBO copy
 ├─ record commands                 CommandBuffers::record
 ├─ queue submit                    vkQueueSubmit
 └─ queue present                   vkQueuePresentKHR
```

GPU (`CommandBuffers::record` in `core/src/CommandBuffers.cpp`), on the "Graphics" queue:

```
gpu frame                           whole command buffer
 └─ render                          vkCmdBeginRendering .. vkCmdEndRendering
     └─ draw indexed                vkCmdDrawIndexed
```

How to read them together: the **`wait for frame fence`** zone is the CPU waiting for the
GPU. If it is long, the GPU is the bottleneck (compare it with the GPU `gpu frame` bar). If it
is near zero while `acquire swapchain image` or `queue present` is long, the loop is limited by
the display's refresh (vsync), not by either processor.

### Worked example: reading a vsync-bound capture

Captured 2026-10-08 on a 240 Hz monitor (4,284 frames, 17.88 s):

| Zone | Mean per call |
|---|---|
| `acquire swapchain image` | 3.99 ms (95.5% of zone time) |
| `wait for frame fence` | 66 µs |
| `record commands` | 46 µs |
| `queue present` | 32 µs |
| everything else | under 25 µs each |

- Frame time was 4.17 ms = 1 / 240 Hz. An earlier capture on a 120 Hz display gave 8.31 ms =
  1 / 120 Hz. **Frame time equal to the refresh period is the signature of a vsync-paced loop.**
- `wait for frame fence` was tiny, so the CPU was never waiting on the GPU: the GPU was not the
  bottleneck. The long `acquire` is the loop sleeping until the display released an image.
- Real CPU work was about 0.15 ms of a 4.17 ms frame. "240 fps" says nothing about headroom;
  the zones do. To measure uncapped performance, change the swapchain present mode
  (`MAILBOX` / `IMMEDIATE`); not done yet.

Same session, GPU view (Statistics, **GPU** radio button):

| GPU zone | Mean per call | Counts |
|---|---|---|
| `gpu frame` | 118.28 µs (2.84% of the session) | 6,080 |
| `render` | 115.27 µs | 6,080 |
| `draw indexed` | 39.59 µs | 5,521 |

- The GPU was busy ~118 µs of each 4,170 µs frame (~2.8%). With the CPU also idle, the loop is
  waiting on vsync; both processors have large headroom.
- `render` is ~97% of `gpu frame` (barriers outside it cost ~3 µs). `draw indexed` is about a
  third of `render`; the rest is attachment clears/loads, descriptor binds and viewport setup.
  For a scene this small, clearing the screen costs more than drawing it.
- **Open question:** `draw indexed` has fewer samples (5,521) than the zones around it (6,080).
  Cause not identified (candidates: Tracy dropping very short nested zones with equal/inverted
  timestamps; frames still in flight at capture end). Check the viewer's **Messages** tab.

## Build settings (CMakeLists.txt)

| Option | Value | Why |
|---|---|---|
| `FORGE3D_PROFILING` | `ON` (default) | `OFF` compiles every macro to nothing: zero cost |
| `TRACY_ON_DEMAND` | `ON` | Collects nothing until a viewer connects |
| `TRACY_ONLY_LOCALHOST` | `ON` | Never listens on the network (default would listen on all interfaces) |

For a shipping build: `cmake -DFORGE3D_PROFILING=OFF ...`.

## Building the viewer (one time)

The viewer **must be the same version as the client** (the wire protocol changes between
releases). Build it from the same submodule so they always match.

```bash
# Packages the viewer needs that were not already installed (check with: dpkg -s <name>)
sudo apt install libcapstone-dev libxkbcommon-dev libdbus-1-dev pkg-config

# Build (downloads a few small dependencies on first configure)
cmake -S deps/tracy/profiler -B deps/tracy/profiler/build -DCMAKE_BUILD_TYPE=Release
cmake --build deps/tracy/profiler/build -j

# Run it
./deps/tracy/profiler/build/tracy-profiler
```

If the configure step names another missing package, install it and re-run. Paste any error
here and we will read it together. On Wayland the viewer uses the Wayland backend by default
(`LEGACY=OFF`).

## How GPU zones work

A GPU zone is a pair of **timestamps** written by the GPU into a query pool, one when the zone
opens and one when it closes:

```
record time (CPU)          GPU executes later
 PROFILE_GPU_SCOPE  ──►  vkCmdWriteTimestamp(start)  ... work ...  vkCmdWriteTimestamp(end)
                                      │                                     │
                                      └──────── stored in a query pool ─────┘
 PROFILE_GPU_COLLECT ──► reads back the finished timestamps and sends them to the viewer
```

- `GpuProfiler` (`core/include/GpuProfiler.hpp`, `core/src/GpuProfiler.cpp`) owns the Tracy
  Vulkan context. On creation Tracy records, submits and waits on a command buffer to reset the
  query pool and take a first timestamp, which lines the GPU clock up with the CPU clock. We give
  it a throwaway command pool for that; `Renderer` owns the `GpuProfiler` and passes its handle
  to `CommandBuffers`.
- **Uncalibrated**: this uses the GPU's own clock (`VK_TIME_DOMAIN_DEVICE_EXT`). It is accurate
  for durations but can drift against the CPU clock over a long session. The calibrated variant
  needs `VK_EXT_calibrated_timestamps` enabled on the device (not done).
- Results arrive **late**: the GPU is still working on frames the CPU has already moved past, so
  `PROFILE_GPU_COLLECT` reads whatever has finished. GPU bars therefore appear a frame or two
  behind in the viewer.

Rules (also in the `Profiling.hpp` comment):

1. The command buffer must be **recording** when a zone opens *and* when it closes. This is why
   `record()` wraps its body in a block: the `gpu frame` zone has to end before
   `vkEndCommandBuffer`.
2. `PROFILE_GPU_COLLECT` goes **after** the zones have closed and **outside** the render pass,
   because it also records `vkCmdResetQueryPool` for the queries it consumed.
3. Two zones may not share a source line (the macro uses `__LINE__` for unique names).
4. The Vulkan queue family must support timestamps (`timestampValidBits > 0`); graphics queues
   always do.

## Using it

1. Start `tracy-profiler`, then start the engine (or the other way round).
2. Click **Connect** in the viewer (it finds the engine on localhost).
3. The timeline shows each zone per frame; the top bar shows frame times.
4. With `TRACY_ON_DEMAND`, data only flows while the viewer is connected, so closing the viewer
   returns the engine to near-zero overhead.

## Adding a zone

```cpp
#include "Profiling.hpp"

void doExpensiveThing() {
    PROFILE_SCOPE("expensiveThing"); // string literal; ends when the scope ends
    ...
}
```

Zones nest: a `PROFILE_SCOPE` inside another shows as a child bar underneath.

## Not done yet (future work)

- Calibrated GPU timestamps (`TracyVkContextCalibrated`) for long-session accuracy.
- GPU zones for the texture upload and mip generation (`Texture` uses `immediateSubmit`, which
  has its own command buffer).
- Thread names (`tracy::SetThreadName`) once a job system exists.
- Memory tracking (`TracyAlloc`/`TracyFree`) hooked to VMA allocations.

## References

- Tracy manual: PDF attached to each release at https://github.com/wolfpld/tracy/releases
  (download the one for v0.14.1; the repo only has its LaTeX source in `deps/tracy/manual/`)
- https://github.com/wolfpld/tracy
