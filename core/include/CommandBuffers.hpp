#pragma once
#include "queueFamilies.hpp"
#include <vector>
#include <vulkan/vulkan.h>

class CommandBuffers {
  public:
    CommandBuffers(VkDevice                 pDevice,
                   std::vector<VkImageView> pImageViews,
                   std::vector<VkImage>     pImages,
                   VkPipeline               pPipeline,
                   VkExtent2D               extent,
                   const QueueFamilyIndices indices);

    ~CommandBuffers();

    // copy and move destruction
    CommandBuffers(const CommandBuffers&)            = delete;
    CommandBuffers& operator=(const CommandBuffers&) = delete;
    CommandBuffers(CommandBuffers&&)                 = delete;
    CommandBuffers& operator=(CommandBuffers&&)      = delete;

    const std::vector<VkCommandBuffer> getCmdBuffers() const;
    const VkCommandPool                getCmdPool() const;

  private:
    std::vector<VkCommandBuffer> commandBuffers;
    VkCommandPool                commandPool = VK_NULL_HANDLE;
    VkDevice                     device      = VK_NULL_HANDLE;
};
