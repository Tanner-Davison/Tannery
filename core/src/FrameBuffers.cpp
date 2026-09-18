#include "FrameBuffers.hpp"
#include <stdexcept>

FrameBuffers::FrameBuffers(VkRenderPass             pRenderpass,
                           VkDevice                 pDevice,
                           std::vector<VkImageView> pImageViews,
                           VkExtent2D               pExtendHandle)
    : device(pDevice) {
    frameBuffers.resize(pImageViews.size());
    for (size_t i = 0; i < frameBuffers.size(); i++) {
        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.pNext           = nullptr;
        fbInfo.flags           = 0;
        fbInfo.renderPass      = pRenderpass;
        fbInfo.attachmentCount = 1;
        fbInfo.pAttachments    = &pImageViews[i];
        fbInfo.width           = pExtendHandle.width;
        fbInfo.height          = pExtendHandle.height;
        fbInfo.layers          = 1;

        if (vkCreateFramebuffer(this->device, &fbInfo, nullptr, &frameBuffers[i]) !=
            VK_SUCCESS) {
            throw std::runtime_error("Failed to create frame buffers");
        };
    }
};

std::vector<VkFramebuffer> FrameBuffers::handle() const {
    return this->frameBuffers;
}

FrameBuffers::~FrameBuffers() {
    for (auto buffer : frameBuffers) {
        vkDestroyFramebuffer(this->device, buffer, nullptr);
    }
    frameBuffers.clear();
}
