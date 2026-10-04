#pragma once

#include "Allocator.hpp"
#include "GraphicsContext.hpp"

class DepthImage {
  public:
    DepthImage(const GraphicsContext& pContext, VkExtent2D pExtent);

    // deleted copy and move constructers
    DepthImage(const DepthImage&)            = delete;
    DepthImage& operator=(const DepthImage&) = delete;
    DepthImage(DepthImage&&)                 = delete;
    DepthImage& operator=(DepthImage&&)      = delete;

    ~DepthImage();

    VkImage     getDepthImage() const;
    VkImageView getDepthImageView() const;
    VkFormat    getDepthFormat() const;

  private:
    static constexpr VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
    VkDevice                  device      = VK_NULL_HANDLE;
    VkImage                   image       = VK_NULL_HANDLE;
    VmaAllocator              allocator   = VK_NULL_HANDLE;
    VmaAllocation             allocation  = VK_NULL_HANDLE;
    VkImageView               imageView   = VK_NULL_HANDLE;
};
