#include "DepthImage.hpp"
#include "vk_enum_string_helper.h"
#include <format>
#include <stdexcept>

DepthImage::DepthImage(const GraphicsContext& pContext, VkExtent2D pExtent)
    : device(pContext.deviceHandle())
    , allocator(pContext.allocatorHandle()) {
    VkImageCreateInfo imageCreateInfo{};
    imageCreateInfo.sType     = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO; // flat grid
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format    = this->depthFormat;               // one 32-bit float per pixel
    imageCreateInfo.extent = {pExtent.width, pExtent.height, 1}; // same size and the swapchain
    imageCreateInfo.mipLevels   = 1;                             // nomip chain
    imageCreateInfo.arrayLayers = 1;                             // no layers
    imageCreateInfo.samples     = VK_SAMPLE_COUNT_1_BIT;         // no multisampling
    imageCreateInfo.tiling      = VK_IMAGE_TILING_OPTIMAL;       // GPU-friendly layout
    imageCreateInfo.usage =
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;           // Rendering Write depth here
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // contents don't matter yet

    VmaAllocationCreateInfo allocCreateInfo{};
    allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocCreateInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

    VkResult res = (vmaCreateImage(this->allocator,
                                   &imageCreateInfo,
                                   &allocCreateInfo,
                                   &this->image,
                                   &this->allocation,
                                   nullptr));
    if (res != VK_SUCCESS) {
        throw std::runtime_error(
            std::format("Failed to allocate Depth Buffer create Image. VKError: {}",
                        string_VkResult(res)));
    }

    VkImageViewCreateInfo imageViewCreateInfo{};
    imageViewCreateInfo.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewCreateInfo.image    = this->image;
    imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D; // treat it as a flat 2D grid
    imageViewCreateInfo.format   = this->depthFormat;
    imageViewCreateInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;
    imageViewCreateInfo.subresourceRange.baseMipLevel   = 0; // start at the first mip level
    imageViewCreateInfo.subresourceRange.levelCount     = 1; // one mip level
    imageViewCreateInfo.subresourceRange.baseArrayLayer = 0; // start at the first layer
    imageViewCreateInfo.subresourceRange.layerCount     = 1;

    res = vkCreateImageView(this->device, &imageViewCreateInfo, nullptr, &imageView);
    if (res != VK_SUCCESS) {
        vmaDestroyImage(this->allocator, this->image, this->allocation);
        throw std::runtime_error(
            std::format("Failed to create Image View in DepthImage: VkError: {}",
                        string_VkResult(res)));
    }
}

DepthImage::~DepthImage() {
    vkDestroyImageView(this->device, this->imageView, nullptr);
    vmaDestroyImage(this->allocator, this->image, this->allocation);
}

VkImage DepthImage::getDepthImage() const {
    return this->image;
};

VkImageView DepthImage::getDepthImageView() const {
    return this->imageView;
};

VkFormat DepthImage::getDepthFormat() const {
    return this->depthFormat;
};
