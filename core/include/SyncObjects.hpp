#pragma once
#include <vector>
#include <vulkan/vulkan.h>

class SyncObjects {
  public:
    SyncObjects(VkDevice pDevice, uint32_t pImageCount);

    // delete copy and moved constructors
    SyncObjects(const SyncObjects&)            = delete;
    SyncObjects& operator=(const SyncObjects&) = delete;
    SyncObjects(SyncObjects&&)                 = delete;
    SyncObjects& operator=(SyncObjects&&)      = delete;
    VkSemaphore  getImageAvailableSemaphore(uint32_t currentFrameIndex) const;
    VkSemaphore  getRenderCompleteSemaphore(uint32_t imageIndex) const;
    VkFence      getFence(uint32_t currentFrameIndex) const;
    ~SyncObjects();

    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

  private:
    VkDevice                 device = VK_NULL_HANDLE;
    std::vector<VkSemaphore> imagesAvailableSemaphores;
    std::vector<VkSemaphore> renderCompleteSemaphores;
    std::vector<VkFence>     fences;
};
