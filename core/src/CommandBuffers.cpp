#include "CommandBuffers.hpp"
#include <stdexcept>

CommandBuffers::CommandBuffers(VkDevice                   pDevice,
                               VkRenderPass               renderpass,
                               std::vector<VkFramebuffer> frameBuffers,
                               VkPipeline                 pPipeline,
                               VkExtent2D                 extent,
                               const QueueFamilyIndices   indices)
    : device(pDevice) {
    VkCommandPoolCreateInfo commandPoolInfo{};
    commandPoolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolInfo.queueFamilyIndex = indices.graphicsFamilyIndex.value();
    // CREATE COMMAND POOL
    //
    VkResult poolCreationRes(
        vkCreateCommandPool(this->device, &commandPoolInfo, nullptr, &commandPool));

    if (poolCreationRes != VK_SUCCESS) {
        throw std::runtime_error("Failed to create command pool");
    }
    // resize command buffer pool
    commandBuffers.resize(frameBuffers.size());
    // ALLocate Command Buffers
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = this->commandPool;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());
    VkResult checkAllocated(
        vkAllocateCommandBuffers(this->device, &allocInfo, this->commandBuffers.data()));
    if (checkAllocated != VK_SUCCESS) {
        throw std::runtime_error("Failed to Allocate Command  Buffers");
    }

    constexpr uint32_t     VERTEXCOUNT{3}, INSTANCECOUNT{1}, FIRSTVERTEX{0}, FIRSTINSTANCE{0};
    constexpr VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}}; // opaque black
    for (size_t i = 0; i < commandBuffers.size(); i++) {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.pInheritanceInfo = nullptr;
        beginInfo.flags            = 0;
        beginInfo.pNext            = nullptr;
        vkBeginCommandBuffer(commandBuffers[i], &beginInfo);
        VkRenderPassBeginInfo renderpassInfo{};
        renderpassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderpassInfo.pNext             = nullptr;
        renderpassInfo.renderArea.offset = {0, 0};
        renderpassInfo.renderArea.extent = extent;
        renderpassInfo.renderPass        = renderpass;
        renderpassInfo.framebuffer       = frameBuffers[i];
        renderpassInfo.clearValueCount   = 1;
        renderpassInfo.pClearValues      = &clearColor;
        vkCmdBeginRenderPass(commandBuffers[i], &renderpassInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pPipeline);
        vkCmdDraw(commandBuffers[i], VERTEXCOUNT, INSTANCECOUNT, FIRSTVERTEX, FIRSTINSTANCE);
        vkCmdEndRenderPass(commandBuffers[i]);
        vkEndCommandBuffer(commandBuffers[i]);
    };
}

CommandBuffers::~CommandBuffers() {
    vkDestroyCommandPool(this->device, this->commandPool, nullptr);
}

const std::vector<VkCommandBuffer> CommandBuffers::getCmdBuffers() const {
    return this->commandBuffers;
};

const VkCommandPool CommandBuffers::getCmdPool() const {
    return this->commandPool;
};
