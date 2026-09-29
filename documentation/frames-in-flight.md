# Frames in Flight

**Status:** COMPLETE (roadmap step 1 of 4: frames in flight → swapchain recreation → vertex buffers → index/uniform buffers). Builds and runs with no validation errors.

## What was implemented

- `SyncObjects::MAX_FRAMES_IN_FLIGHT` is a `public static constexpr uint32_t` (2), shared by
  `SyncObjects` (how many to create) and `App` (where the slot counter wraps).
- `SyncObjects` holds `MAX_FRAMES_IN_FLIGHT` fences (created signaled) and
  `MAX_FRAMES_IN_FLIGHT` image-available semaphores, plus one render-complete semaphore per
  swapchain image. Getters take an index and return one handle by value (`.at()` bounds-checked).
- `App` has a non-static `currentFrame` member; `drawFrame()` is non-`const` and advances it at
  the end, wrapping at `MAX_FRAMES_IN_FLIGHT - 1`.
- `CommandBuffers::getCmdBuffers(imageIndex)` returns a single handle instead of copying the vector.

## Mistakes worth remembering

- **Looping over all fences inside `drawFrame`** (first as separate loops, then wrapping the
  whole frame). The frame loop already exists in `run()`; alternating slots comes from state that
  persists between calls (`currentFrame`), not from a loop inside one call.
- **Old single `fence` left behind** after adding `fences[]`: the getter returned a null handle and
  the destructor never freed the new fences.
- **Wrap condition compared against `MAX_FRAMES_IN_FLIGHT` instead of `MAX_FRAMES_IN_FLIGHT - 1`**
  (off-by-one); `.at()` turned it into a clear exception instead of silent out-of-bounds reads.
- **`T&` returned from a `const` method** contradicts the `const`; use a value (handles are cheap)
  or a `const T&`.
- **Cleanup-guard gaps** when a constructor throws partway: every created object must be
  registered with the guard immediately.

## Two indices, one rule

`currentFrame` (you choose) indexes anything whose reuse you can prove with your own fence:
fences and image-available semaphores. `imageIndex` (the driver returns it from acquire) indexes
anything tied to a specific swapchain image: render-complete semaphores and the per-image
command buffers.

## The problem

`App::drawFrame()` currently starts with `vkWaitForFences` on a single fence. That fence is
signaled only when the previous frame's GPU work finishes, so every frame is a strict
handoff:

```
CPU: [wait........][record/submit]  [wait........][record/submit]
GPU:               [....render...]                [....render...]
```

The CPU idles while the GPU works, then the GPU idles while the CPU prepares the next frame.
Neither side is ever busy at the same time as the other.

## The idea

Allow the CPU to get up to **N frames ahead** of the GPU (N = `MAX_FRAMES_IN_FLIGHT`, almost
always 2). Each in-flight frame needs its **own copy** of anything the GPU may still be
reading or waiting on while the CPU prepares the next frame:

- a fence (CPU waits on this before reusing the frame's resources)
- an "image available" semaphore (signaled by the presentation engine)
- a command buffer (if it is re-recorded or holds per-frame data)
- later: per-frame uniform buffers and descriptor sets

```
CPU: [frame 0][frame 1][wait][frame 2][wait][frame 3]...
GPU:   [..frame 0..][..frame 1..][..frame 2..]...
```

## Two different counts (the key distinction)

| Resource | Indexed by | Count | Why |
|---|---|---|---|
| image-available semaphore, fence, (per-frame command buffer) | **frame-in-flight index** (`currentFrame`, cycles `0..N-1`) | `MAX_FRAMES_IN_FLIGHT` | Tied to CPU-side reuse: "is this frame slot's previous work done?" |
| render-complete semaphore | **swapchain image index** (`imageIndex`, returned by acquire) | `swapchain image count` | Waited on by the presentation engine, which the app cannot observe. Already implemented this way. |

`currentFrame` is chosen by you (`(currentFrame + 1) % MAX_FRAMES_IN_FLIGHT`).
`imageIndex` is chosen by the driver.

## Reading

- Vulkan Tutorial, "Frames in flight": https://vulkan-tutorial.com/Drawing_a_triangle/Drawing/Frames_in_flight
- Vulkan Guide, swapchain semaphore reuse (the doc your validation error linked earlier):
  https://docs.vulkan.org/guide/latest/swapchain_semaphore_reuse.html
- Vulkan Guide, synchronization: https://docs.vulkan.org/guide/latest/synchronization.html
- Spec: `vkWaitForFences`, `vkResetFences`, `vkQueueSubmit`
- Vulkan Cookbook (2nd ed.): the chapter covering command buffer submission and synchronization

## Notes

- Nothing new to add to the API call dictionary: the calls are the same, but they are now
  used per frame slot instead of once globally.
