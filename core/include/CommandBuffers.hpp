#pragma once
#include "Mesh.hpp"
#include "Profiling.hpp"
#include "queueFamilies.hpp"
#include <vector>
#include <vulkan/vulkan.h>

class CommandBuffers {
  public:
    // pProfileCtx: from GpuProfiler (nullptr when profiling is compiled out); must outlive this
    CommandBuffers(VkDevice                  pDevice,
                   const QueueFamilyIndices& indices,
                   uint32_t                  frameCount,
                   TracyVkCtx                pProfileCtx);
    ~CommandBuffers();

    // copy and move destruction
    CommandBuffers(const CommandBuffers&)            = delete;
    CommandBuffers& operator=(const CommandBuffers&) = delete;
    CommandBuffers(CommandBuffers&&)                 = delete;
    CommandBuffers& operator=(CommandBuffers&&)      = delete;

    VkCommandBuffer record(uint32_t         frameIndex,
                           VkImage          image,
                           VkImageView      imageView,
                           VkImageView      pDepthImageView,
                           VkImage          pDepthImage,
                           VkExtent2D       extent,
                           VkPipeline       pipeline,
                           VkPipelineLayout pipelineLayout,
                           VkDescriptorSet  frameSet,    // set 0: camera
                           VkDescriptorSet  materialSet, // set 1: texture + sampler
                           const Mesh&      mesh);

  private:
    std::vector<VkCommandBuffer> commandBuffers;
    VkCommandPool                commandPool = VK_NULL_HANDLE;
    VkDevice                     device      = VK_NULL_HANDLE;
    TracyVkCtx                   profileCtx  = nullptr; // borrowed: GPU zone timestamps
};
