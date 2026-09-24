#include "CommandBuffers.hpp"
#include <stdexcept>

CommandBuffers::CommandBuffers(VkDevice                 pDevice,
                               std::vector<VkImageView> pImageViews,
                               std::vector<VkImage>     pImages,
                               VkPipeline               pPipeline,
                               VkExtent2D               extent,
                               const QueueFamilyIndices indices)
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
    // resize command buffer pool to image views size
    commandBuffers.resize(pImageViews.size());
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

    // RECORDING LOOP
    for (size_t i = 0; i < commandBuffers.size(); i++) {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.pInheritanceInfo = nullptr;
        beginInfo.flags            = 0;
        beginInfo.pNext            = nullptr;

        vkBeginCommandBuffer(commandBuffers[i], &beginInfo);

        VkImageMemoryBarrier barrierBefore{};
        barrierBefore.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrierBefore.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
        barrierBefore.newLayout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrierBefore.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrierBefore.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrierBefore.image               = pImages[i];
        barrierBefore.subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        barrierBefore.srcAccessMask       = 0;
        barrierBefore.dstAccessMask       = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        vkCmdPipelineBarrier(commandBuffers[i],
                             VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                             VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                             0,
                             0,
                             nullptr,
                             0,
                             nullptr,
                             1,
                             &barrierBefore);
        VkRenderingAttachmentInfo colorAttachment{};
        colorAttachment.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAttachment.imageView   = pImageViews[i];
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp     = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.clearValue  = clearColor;
        VkRenderingInfo renderingInfo{};
        renderingInfo.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderingInfo.renderArea.offset    = {0, 0};
        renderingInfo.renderArea.extent    = extent;
        renderingInfo.layerCount           = 1;
        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachments    = &colorAttachment;
        vkCmdBeginRendering(commandBuffers[i], &renderingInfo);
        vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pPipeline);
        vkCmdDraw(commandBuffers[i], VERTEXCOUNT, INSTANCECOUNT, FIRSTVERTEX, FIRSTINSTANCE);
        vkCmdEndRendering(commandBuffers[i]);

        VkImageMemoryBarrier barrierAfter{};
        barrierAfter.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrierAfter.oldLayout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrierAfter.newLayout           = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barrierAfter.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrierAfter.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrierAfter.image               = pImages[i];
        barrierAfter.subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        barrierAfter.srcAccessMask       = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrierAfter.dstAccessMask       = 0;

        vkCmdPipelineBarrier(commandBuffers[i],
                             VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                             VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                             0,
                             0,
                             nullptr,
                             0,
                             nullptr,
                             1,
                             &barrierAfter);
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
