#include "Texture.hpp"
#include "vk_enum_string_helper.h"
#include <format>
#include <memory>
#include <stb/stb_image.h>
#include <stdexcept>

namespace {
// Decoded pixels in CPU memory: tightly packed RGBA8 rows, top row first
struct PixelData {
    std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> pixels{nullptr, &stbi_image_free};
    uint32_t                                             width  = 0;
    uint32_t                                             height = 0;

    VkDeviceSize sizeBytes() const {
        return static_cast<VkDeviceSize>(width) * height * 4;
    }
};

PixelData loadPixels(const std::filesystem::path& path) {
    int width    = 0;
    int height   = 0;
    int channels = 0;

    // STBI_rgb_alpha = 4
    stbi_uc* raw =
        stbi_load(path.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);

    if (raw == nullptr) {
        const char* error_msg = stbi_failure_reason();
        throw std::runtime_error(
            std::format("Error loadPixels: {}, {}", path.string(), error_msg));
    }
    PixelData data;
    data.pixels.reset(raw);
    data.width  = static_cast<uint32_t>(width);
    data.height = static_cast<uint32_t>(height);

    return data;
}
} // namespace

Texture::Texture(const GraphicsContext& pContext, const std::filesystem::path& pPath)
    : device(pContext.deviceHandle())
    , allocator(pContext.allocatorHandle()) {
    // 1. Decode the file on the CPU
    PixelData pixels = loadPixels(pPath);
    size             = {pixels.width, pixels.height};

    // 2. CPU-visible staging buffer holding the raw pixels
    Buffer   staging(allocator,
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
    imageCreateInfo.mipLevels   = 1;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.samples     = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling      = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.usage       = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo allocCreateInfo{};
    allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;

    res = vmaCreateImage(allocator,
                         &imageCreateInfo,
                         &allocCreateInfo,
                         &image,
                         &allocation,
                         nullptr);
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
        toTransfer.subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        toTransfer.srcAccessMask       = 0; // nothing earlier to wait on
        toTransfer.dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT; // the copy writes
        vkCmdPipelineBarrier(
            cmd,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, // srcStageMask: wait on nothing
            VK_PIPELINE_STAGE_TRANSFER_BIT,    // dstStageMask: the copy waits
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
        region.bufferRowLength   = 0; // 0 = rows tightly packed (stb's output)
        region.bufferImageHeight = 0;
        region.imageSubresource  = {VK_IMAGE_ASPECT_COLOR_BIT,
                                    0,
                                    0,
                                    1}; // aspect, mip, layer, count
        region.imageOffset       = {0, 0, 0};
        region.imageExtent       = {size.width, size.height, 1};
        vkCmdCopyBufferToImage(cmd,
                               staging.handle(),
                               image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // must match barrier A
                               1,
                               &region);

        // B: TRANSFER_DST -> SHADER_READ_ONLY, so the fragment shader can sample it
        VkImageMemoryBarrier toShaderRead = toTransfer;
        toShaderRead.oldLayout            = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toShaderRead.newLayout            = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        toShaderRead.srcAccessMask        = VK_ACCESS_TRANSFER_WRITE_BIT; // the copy wrote
        toShaderRead.dstAccessMask        = VK_ACCESS_SHADER_READ_BIT;    // texture() reads
        vkCmdPipelineBarrier(
            cmd,
            VK_PIPELINE_STAGE_TRANSFER_BIT,        // srcStageMask: copy finishes
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, // dstStageMask: sampling waits
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &toShaderRead);
    });

    // 5. View so shaders can read the image as a 2D color texture
    VkImageViewCreateInfo viewCreateInfo{};
    viewCreateInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewCreateInfo.image                           = image;
    viewCreateInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    viewCreateInfo.format                          = format;
    viewCreateInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    viewCreateInfo.subresourceRange.baseMipLevel   = 0;
    viewCreateInfo.subresourceRange.levelCount     = 1;
    viewCreateInfo.subresourceRange.baseArrayLayer = 0;
    viewCreateInfo.subresourceRange.layerCount     = 1;

    res = vkCreateImageView(device, &viewCreateInfo, nullptr, &imageView);
    if (res != VK_SUCCESS) {
        vmaDestroyImage(allocator, image, allocation);
        throw std::runtime_error(
            std::format("Failed to create texture image view. VkError: {}",
                        string_VkResult(res)));
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
