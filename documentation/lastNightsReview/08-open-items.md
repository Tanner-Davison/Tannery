# 8. Open items and next lessons

## Unanswered question: `draw indexed` has fewer samples

In the GPU statistics from 2026-10-08:

| GPU zone | Counts |
|---|---|
| `gpu frame` | 6,080 |
| `render` | 6,080 |
| `draw indexed` | **5,521** |

They should be equal: every frame draws once. About 9% of the `draw indexed` zones (559) are
missing. **Cause not identified.** Candidates:

- Tracy dropped very short nested zones whose start and end timestamps came back equal or in the
  wrong order (a 40 us zone inside another is the likeliest to hit this).
- Frames still in flight when the capture stopped (this would only explain a handful, not 559).

How to investigate: open the **Messages** tab in the Tracy viewer (it reports GPU timing problems
there), then try wrapping a larger piece of work in the zone to see whether the gap disappears.
It doesn't affect the conclusions about the loop being vsync-bound, because the other zones are
complete.

## Next lesson candidates

### Present modes (`FIFO`, `MAILBOX`, `IMMEDIATE`)

The swapchain's **present mode** decides what happens when a frame finishes before the monitor is
ready:

| Mode | Behaviour | Trade-off |
|---|---|---|
| `FIFO` | queue frames, wait for vsync (the guaranteed-available mode) | no tearing, capped at refresh rate, adds latency |
| `MAILBOX` | keep only the newest finished frame, present at vsync | no tearing, uncapped rendering, uses more power |
| `IMMEDIATE` | present right away, even mid-scan | lowest latency, can tear |

Why it would help now: it would let us measure the engine **uncapped** and check the estimate
that the CPU (about 0.15 ms) and GPU (about 0.12 ms) per frame allow several thousand fps. This is
a new concept, so it would be taught first, with an overview.

### GPU zones for texture upload and mip generation

`Texture` uses `immediateSubmit`, which has its own command buffer, so those operations have no
GPU zones. Adding them would show how long mip generation takes and would teach how to profile
load-time work.

### Other items parked in the docs

| Item | Where it lives |
|---|---|
| Calibrated GPU timestamps (`VK_EXT_calibrated_timestamps`) | `documentation/profiling.md` |
| Thread names once a job system exists (`tracy::SetThreadName`) | `documentation/profiling.md` |
| Memory tracking hooked to VMA allocations | `documentation/profiling.md` |
| Anisotropic filtering (needs the `samplerAnisotropy` device feature) | `documentation/textures.md` |
| Persisting key bindings (`rebind` exists, nothing saves it) | `documentation/camera.md` |
| `glfwWaitEvents()` while unfocused (the loop currently renders at full speed) | `documentation/camera.md` |
| Quaternion orientation (Euler angles can't blend cameras cleanly) | `documentation/camera.md` |

## Housekeeping

- `core/src/CommandBuffers.cpp` has a few oddly indented argument lines from a block edit; the
  formatter will fix them.
- `rebind` is unused; `Settings` doesn't store bindings.
- `fixedUpdate` is an empty hook until the engine has physics.

## The review process itself

For each of files 1 to 7, the exercises marked "Rewrite it yourself" are the part that turns
reading into skill. A good order if you are short on time:

1. File 1 (the loop), since it is the only new **Vulkan** concept from the night.
2. File 3 (`update()` and the capture state machine), the most subtle code.
3. File 2 (`forward()` and `move()`), the math everything else leans on.
4. Files 4 to 6 by reading; they are general C++.
5. File 7: add the one zone from its exercise.
