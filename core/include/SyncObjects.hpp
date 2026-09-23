#pragma once
#include <vector>
#include <vulkan/vulkan.h>

class SyncObjects {
  public:
    SyncObjects(VkDevice pDevice, uint32_t pImageCount);

    // delete copy and moved constructors
    SyncObjects(const SyncObjects&)                   = delete;
    SyncObjects& operator=(const SyncObjects&)        = delete;
    SyncObjects(SyncObjects&&)                        = delete;
    SyncObjects&             operator=(SyncObjects&&) = delete;
    VkSemaphore              getImageAvailableSemaphore() const;
    std::vector<VkSemaphore> getRenderCompleteSemaphores() const;
    VkFence                  getFence() const;
    ~SyncObjects();

  private:
    VkDevice                 device                  = VK_NULL_HANDLE;
    VkSemaphore              imageAvailableSemaphore = VK_NULL_HANDLE;
    std::vector<VkSemaphore> renderCompleteSemaphores;
    VkFence                  fence = VK_NULL_HANDLE;
};
