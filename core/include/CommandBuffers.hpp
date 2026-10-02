#pragma once
#include "Mesh.hpp"
#include "queueFamilies.hpp"
#include <vector>
#include <vulkan/vulkan.h>

class CommandBuffers {
  public:
    CommandBuffers(VkDevice                        pDevice,
                   const std::vector<VkImageView>& pImageViews,
                   const std::vector<VkImage>&     pImages,
                   VkPipeline                      pPipeline,
                   VkExtent2D                      extent,
                   const QueueFamilyIndices&       indices,
                   const Mesh&                     mesh);

    ~CommandBuffers();

    // copy and move destruction
    CommandBuffers(const CommandBuffers&)            = delete;
    CommandBuffers& operator=(const CommandBuffers&) = delete;
    CommandBuffers(CommandBuffers&&)                 = delete;
    CommandBuffers& operator=(CommandBuffers&&)      = delete;

    VkCommandBuffer     getCmdBuffer(uint32_t imageIndex) const;
    const VkCommandPool getCmdPool() const;

  private:
    std::vector<VkCommandBuffer> commandBuffers;
    VkCommandPool                commandPool = VK_NULL_HANDLE;
    VkDevice                     device      = VK_NULL_HANDLE;
};
