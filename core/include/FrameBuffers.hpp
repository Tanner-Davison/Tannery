#pragma once
#include <vector>
#include <vulkan/vulkan.h>

class FrameBuffers {
  public:
    FrameBuffers(VkRenderPass             pRenderpass,
                 VkDevice                 pDevice,
                 std::vector<VkImageView> pImageViews,
                 VkExtent2D               pExtendHandle);
    ~FrameBuffers();
    // Copy and Move Constructors  deleted
    FrameBuffers(const FrameBuffers&)            = delete;
    FrameBuffers& operator=(const FrameBuffers&) = delete;
    FrameBuffers(FrameBuffers&&)                 = delete;
    FrameBuffers& operator=(FrameBuffers&&)      = delete;

    std::vector<VkFramebuffer> handle() const;

  private:
    std::vector<VkFramebuffer> frameBuffers;
    VkDevice                   device;
};
