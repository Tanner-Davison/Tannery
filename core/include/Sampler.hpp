#pragma once

#include "GraphicsContext.hpp"

// Reading rules for textures (filtering, wrapping). Holds no pixels; one Sampler can be
// shared by many Textures.
class Sampler {
  public:
    explicit Sampler(const GraphicsContext& pContext);

    ~Sampler();
    // deleted copy and move
    Sampler(const Sampler&)            = delete;
    Sampler& operator=(const Sampler&) = delete;
    Sampler(Sampler&&)                 = delete;
    Sampler& operator=(Sampler&&)      = delete;

    VkSampler handle() const;

  private:
    VkDevice  device  = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
};
