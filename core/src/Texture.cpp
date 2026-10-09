#include "Texture.hpp"

#include "vk_enum_string_helper.h"

#include <stb/stb_image.h>

#include <algorithm>
#include <cmath>
#include <format>
#include <memory>
#include <stdexcept>

namespace {
// Decoded pixels in CPU memory: tightly packed RGBA8 rows, top row first
struct PixelData {
    std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> pixels{nullptr, &stbi_image_free};
    uint32_t width  = 0;
    uint32_t height = 0;

    VkDeviceSize sizeBytes() const { return static_cast<VkDeviceSize>(width) * height * 4; }
};

PixelData loadPixels(const std::filesystem::path& path) {
    int width    = 0;
    int height   = 0;
    int channels = 0;

    // STBI_rgb_alpha = 4
    stbi_uc* raw = stbi_load(path.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);

    if (raw == nullptr) {
        const char* error_msg = stbi_failure_reason();
        throw std::runtime_error(std::format("Error loadPixels: {}, {}", path.string(), error_msg));
    }
    PixelData data;
    data.pixels.reset(raw);
    data.width  = static_cast<uint32_t>(width);
    data.height = static_cast<uint32_t>(height);

    return data;
}
}  // namespace

Texture::Texture(const GraphicsContext& pContext, const std::filesystem::path& pPath)
    : device(pContext.deviceHandle())
    , allocator(pContext.allocatorHandle()) {
    // 1. Decode the file on the CPU
    PixelData pixels = loadPixels(pPath);
    size             = {pixels.width, pixels.height};
    // Halve until 1x1: e.g. 512x256 -> floor(log2(512)) + 1 = 10 levels
    mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(size.width, size.height)))) + 1;

    // 2. CPU-visible staging buffer holding the raw pixels
    Buffer staging(allocator,
                   pixels.sizeBytes(),
                   VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                   VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
    VkResult res = vmaCopyMemoryToAllocation(allocator,
                                             pixels.pixels.get(),
                                             staging.allocationHandle(),
                                             0,
                                             pixels.sizeBytes());
    if (res != VK_SUCCESS) {
        throw std::runtime_error(
            std::format("Texture staging upload failed. VkError: {}", string_VkResult(res)));
    }

    // 3. GPU-local image: a copy target now, sampled by shaders afterwards
    VkImageCreateInfo imageCreateInfo{};
    imageCreateInfo.sType       = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.imageType   = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format      = format;
    imageCreateInfo.extent      = {size.width, size.height, 1};
    imageCreateInfo.mipLevels   = mipLevels;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.samples     = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling      = VK_IMAGE_TILING_OPTIMAL;
    // TRANSFER_SRC too: each mip level is read as the source of the next level's blit
    imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
                          | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo allocCreateInfo{};
    allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;

    res =
        vmaCreateImage(allocator, &imageCreateInfo, &allocCreateInfo, &image, &allocation, nullptr);
    if (res != VK_SUCCESS) {
        throw std::runtime_error(
            std::format("Failed to create texture image. VkError: {}", string_VkResult(res)));
    }

    // 4. Upload: barrier -> vkCmdCopyBufferToImage -> barrier
    pContext.immediateSubmit([&](VkCommandBuffer cmd) {
        // A: UNDEFINED -> TRANSFER_DST, so the copy is allowed to write into the image
        VkImageMemoryBarrier toTransfer{};
        toTransfer.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        toTransfer.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
        toTransfer.newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toTransfer.image               = image;
        // aspect, baseMip, levelCount, baseLayer, layerCount: every level becomes a write
        // target
        toTransfer.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, mipLevels, 0, 1};
        toTransfer.srcAccessMask    = 0;                             // nothing earlier to wait on
        toTransfer.dstAccessMask    = VK_ACCESS_TRANSFER_WRITE_BIT;  // the copy writes
        vkCmdPipelineBarrier(cmd,
                             VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,  // srcStageMask: wait on nothing
                             VK_PIPELINE_STAGE_TRANSFER_BIT,     // dstStageMask: the copy waits
                             0,
                             0,
                             nullptr,
                             0,
                             nullptr,
                             1,
                             &toTransfer);

        // Copy: flat staging bytes -> the image's tiled memory
        VkBufferImageCopy region{};
        region.bufferOffset      = 0;
        region.bufferRowLength   = 0;  // 0 = rows tightly packed (stb's output)
        region.bufferImageHeight = 0;
        region.imageSubresource  = {VK_IMAGE_ASPECT_COLOR_BIT,
                                    0,
                                    0,
                                    1};  // aspect, mip, layer, count
        region.imageOffset       = {0, 0, 0};
        region.imageExtent       = {size.width, size.height, 1};
        vkCmdCopyBufferToImage(cmd,
                               staging.handle(),
                               image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,  // must match barrier A
                               1,
                               &region);

        // Size of the level we are reading from (level i-1) at the top of each pass
        int32_t mipWidth  = static_cast<int32_t>(size.width);
        int32_t mipHeight = static_cast<int32_t>(size.height);

        // Shared by the per-level and last-level barriers; each use sets its own fields
        VkImageMemoryBarrier barrier{};
        barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image               = image;

        for (uint32_t i = 1; i < mipLevels; i++) {
            // 1. Level i-1 is finished: switch it from write target to read source
            barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, i - 1, 1, 0, 1};
            barrier.oldLayout        = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;  // keeps pixels
            barrier.newLayout        = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.srcAccessMask    = VK_ACCESS_TRANSFER_WRITE_BIT;  // the copy/blit wrote it
            barrier.dstAccessMask    = VK_ACCESS_TRANSFER_READ_BIT;   // the blit reads it
            vkCmdPipelineBarrier(cmd,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 0,
                                 0,
                                 nullptr,
                                 0,
                                 nullptr,
                                 1,
                                 &barrier);

            // 2. Blit level i-1 -> level i at half size (never below 1)
            const int32_t nextWidth  = mipWidth > 1 ? mipWidth / 2 : 1;
            const int32_t nextHeight = mipHeight > 1 ? mipHeight / 2 : 1;

            VkImageBlit blit{};
            blit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, i - 1, 0, 1};
            blit.srcOffsets[0]  = {0, 0, 0};
            blit.srcOffsets[1]  = {mipWidth, mipHeight, 1};
            blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, i, 0, 1};
            blit.dstOffsets[0]  = {0, 0, 0};
            blit.dstOffsets[1]  = {nextWidth, nextHeight, 1};

            vkCmdBlitImage(cmd,
                           image,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1,
                           &blit,
                           VK_FILTER_LINEAR);

            // 3. Level i-1 is done for good: hand it to the fragment shader
            barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;  // the blit read it
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;    // texture() will read it
            vkCmdPipelineBarrier(cmd,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                 0,
                                 0,
                                 nullptr,
                                 0,
                                 nullptr,
                                 1,
                                 &barrier);

            // 4. Level i becomes the source for the next pass
            mipWidth  = nextWidth;
            mipHeight = nextHeight;
        }

        // 5. The last level was only ever a blit destination, so it is still TRANSFER_DST
        barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, mipLevels - 1, 1, 0, 1};
        barrier.oldLayout        = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout        = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask    = VK_ACCESS_TRANSFER_WRITE_BIT;  // the blit (or copy) wrote it
        barrier.dstAccessMask    = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmd,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0,
                             0,
                             nullptr,
                             0,
                             nullptr,
                             1,
                             &barrier);
    });

    // 5. View so shaders can read the image as a 2D color texture
    VkImageViewCreateInfo viewCreateInfo{};
    viewCreateInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewCreateInfo.image                           = image;
    viewCreateInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    viewCreateInfo.format                          = format;
    viewCreateInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    viewCreateInfo.subresourceRange.baseMipLevel   = 0;
    viewCreateInfo.subresourceRange.levelCount     = mipLevels;
    viewCreateInfo.subresourceRange.baseArrayLayer = 0;
    viewCreateInfo.subresourceRange.layerCount     = 1;

    res = vkCreateImageView(device, &viewCreateInfo, nullptr, &imageView);
    if (res != VK_SUCCESS) {
        vmaDestroyImage(allocator, image, allocation);
        throw std::runtime_error(
            std::format("Failed to create texture image view. VkError: {}", string_VkResult(res)));
    }
}

Texture::~Texture() {
    vkDestroyImageView(device, imageView, nullptr);
    vmaDestroyImage(allocator, image, allocation);
}

VkImage Texture::imageHandle() const {
    return image;
}

VkImageView Texture::viewHandle() const {
    return imageView;
}

VkExtent2D Texture::extent() const {
    return size;
}
