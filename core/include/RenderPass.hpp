#pragma once
#include <vulkan/vulkan.h>

class RenderPass {
  public:
    RenderPass(VkDevice pDevice, VkSurfaceFormatKHR pSurfaceFormat);

    // copy && move constructors
    RenderPass(const RenderPass&)            = delete;
    RenderPass& operator=(const RenderPass&) = delete;
    RenderPass(RenderPass&&)                 = delete;
    RenderPass& operator=(RenderPass&&)      = delete;
    // Destructor
    ~RenderPass();

    VkRenderPass handle() const;

  private:
    VkRenderPass       renderPass = VK_NULL_HANDLE;
    VkDevice           device     = VK_NULL_HANDLE;
    VkSurfaceFormatKHR surfaceFormat;
};
