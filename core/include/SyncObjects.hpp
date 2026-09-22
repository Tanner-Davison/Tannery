#pragma once
#include <vulkan/vulkan.h>

class SyncObjects {
  public:
    SyncObjects(VkDevice pDevice);

    // delete copy and moved constructors
    SyncObjects(const SyncObjects&)            = delete;
    SyncObjects& operator=(const SyncObjects&) = delete;
    SyncObjects(SyncObjects&&)                 = delete;
    SyncObjects& operator=(SyncObjects&&)      = delete;
    VkSemaphore  getImageAvailableSemaphore() const;
    VkSemaphore  getRenderCompleteSemaphore() const;
    VkFence      getFence() const;
    ~SyncObjects();

  private:
    VkDevice    device                  = VK_NULL_HANDLE;
    VkSemaphore imageAvailableSemaphore = VK_NULL_HANDLE;
    VkSemaphore renderCompleteSemaphore = VK_NULL_HANDLE;
    VkFence     fence                   = VK_NULL_HANDLE;
};
