#pragma once

#include "GraphicsContext.hpp"
#include <filesystem>

// A sampled 2D image loaded from disk (RGBA8, sRGB), uploaded once at construction
class Texture {
  public:
    Texture(const GraphicsContext& pContext, const std::filesystem::path& pPath);

    ~Texture();
    // deleted copy and move
    Texture(const Texture&)            = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&&)                 = delete;
    Texture& operator=(Texture&&)      = delete;

    VkImage     imageHandle() const;
    VkImageView viewHandle() const;
    VkExtent2D  extent() const;

  private:
    // 4 bytes per pixel, color values stored gamma-encoded (how PNGs are authored)
    static constexpr VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;

    VkDevice      device     = VK_NULL_HANDLE;
    VmaAllocator  allocator  = VK_NULL_HANDLE;
    VkImage       image      = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkImageView   imageView  = VK_NULL_HANDLE;
    VkExtent2D    size{};
    uint32_t      mipLevels = 1; // full chain: floor(log2(max(w, h))) + 1
};
