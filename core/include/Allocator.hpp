#pragma once
#include <vk_mem_alloc.h>

class Allocator {
  public:
    Allocator(VkInstance pInstance, VkPhysicalDevice pPhysicalDevice, VkDevice pDevice);
    ~Allocator();
    // Deleted copy and move
    Allocator(const Allocator&)            = delete;
    Allocator& operator=(const Allocator&) = delete;
    Allocator(Allocator&&)                 = delete;
    Allocator& operator=(Allocator&&)      = delete;
    // Getter
    VmaAllocator handle() const;

  private:
    VmaAllocator allocator = VK_NULL_HANDLE;
};
