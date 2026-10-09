#pragma once

// The engine's only profiling include. Call sites use these macros, never Tracy directly,
// so the profiler can be swapped or removed in one place.
//
// With FORGE3D_PROFILING=OFF (Tracy's TRACY_ENABLE undefined) Tracy turns every macro into
// nothing, so release builds pay no cost. TracyVkCtx is `void*` in that case.
#include <vulkan/vulkan.h> // must come before TracyVulkan.hpp

#include <tracy/Tracy.hpp>
#include <tracy/TracyVulkan.hpp>

#define PROFILE_CONCAT_INNER(a, b) a##b
#define PROFILE_CONCAT(a, b) PROFILE_CONCAT_INNER(a, b)

// ---- CPU ----------------------------------------------------------------------------------

// Times from here to the end of the enclosing scope. `name` must be a string literal.
#define PROFILE_SCOPE(name) ZoneScopedN(name)

// Marks the end of a rendered frame; Tracy uses it to draw the frame-time graph.
#define PROFILE_FRAME() FrameMark

// ---- GPU ----------------------------------------------------------------------------------
// GPU zones are timestamps written *into a command buffer*, so they measure when the GPU
// actually ran that part of the work, not when the CPU recorded it. `ctx` comes from
// GpuProfiler. Rules: `cmd` must be recording when the scope opens AND when it closes, and two
// zones may not share a source line.

// Times the GPU work recorded between here and the end of the enclosing scope.
#define PROFILE_GPU_SCOPE(ctx, cmd, name) \
    TracyVkNamedZone(ctx, PROFILE_CONCAT(gpuZone_, __LINE__), cmd, name, true)

// Once per frame, in a recording command buffer outside any render pass, after the frame's GPU
// zones have closed: reads back finished timestamps and sends them to the viewer.
#define PROFILE_GPU_COLLECT(ctx, cmd) TracyVkCollect(ctx, cmd)
