#pragma once
#include "GraphicsContext.hpp"
#include "Sampler.hpp"
#include "Texture.hpp"

// Descriptor set 1: what a surface looks like (its texture + sampler).
// Set 0 (FrameDescriptors) changes every frame; this changes per object/material.
class MaterialDescriptors {
  public:
    MaterialDescriptors(const GraphicsContext& pContext,
                        const Texture&         pTexture,
                        const Sampler&         pSampler);

    // Removed copy and move constructors
    MaterialDescriptors(const MaterialDescriptors&)            = delete;
    MaterialDescriptors& operator=(const MaterialDescriptors&) = delete;
    MaterialDescriptors(MaterialDescriptors&&)                 = delete;
    MaterialDescriptors& operator=(MaterialDescriptors&&)      = delete;

    ~MaterialDescriptors();

    // Handles
    VkDescriptorSetLayout layoutHandle() const;
    VkDescriptorSet       setHandle() const;

  private:
    void                  destroy();
    VkDevice              device = VK_NULL_HANDLE;
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    VkDescriptorPool      pool   = VK_NULL_HANDLE;
    VkDescriptorSet       set    = VK_NULL_HANDLE; // freed with the pool
};
