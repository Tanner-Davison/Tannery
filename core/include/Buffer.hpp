#pragma once
#include <vk_mem_alloc.h>

class Buffer {
  public:
    Buffer(VmaAllocator             pAllocator,
           VkDeviceSize             pSize,
           VkBufferUsageFlags       pUsage,
           VmaAllocationCreateFlags pFlags);

    // Deleted Copy and Move
    Buffer(const Buffer&)            = delete;
    Buffer& operator=(const Buffer&) = delete;
    Buffer(Buffer&&)                 = delete;
    Buffer& operator=(Buffer&&)      = delete;

    ~Buffer();

    // Getters
    VkBuffer      handle() const;
    VmaAllocation allocationHandle() const;
    VkDeviceSize  sizeBytes() const;

  private:
    VmaAllocator  allocator;
    VkBuffer      buffer     = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkDeviceSize  size;
};
