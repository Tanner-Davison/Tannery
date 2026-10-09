#pragma once
#include "GraphicsContext.hpp"
#include "Profiling.hpp"

// Owns the Tracy Vulkan context: a timestamp query pool on the graphics queue plus the
// calibration that maps GPU clock ticks onto the CPU timeline. Create once, after the device;
// destroy before it. Does nothing (handle() == nullptr) when profiling is compiled out.
class GpuProfiler {
  public:
    explicit GpuProfiler(const GraphicsContext& pContext);
    ~GpuProfiler();

    GpuProfiler(const GpuProfiler&)            = delete;
    GpuProfiler& operator=(const GpuProfiler&) = delete;
    GpuProfiler(GpuProfiler&&)                 = delete;
    GpuProfiler& operator=(GpuProfiler&&)      = delete;

    // Pass to PROFILE_GPU_SCOPE / PROFILE_GPU_COLLECT
    TracyVkCtx handle() const;

  private:
    TracyVkCtx context = nullptr;
};
