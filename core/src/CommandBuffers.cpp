#include "CommandBuffers.hpp"
#include "vk_enum_string_helper.h"
#include <format>
#include <stdexcept>

namespace {
void check(VkResult res, const char* what) {
    if (res != VK_SUCCESS) {
        throw std::runtime_error(
            std::format("{} failed. VkError: {}", what, string_VkResult(res)));
    }
}
} // namespace

CommandBuffers::CommandBuffers(VkDevice                  pDevice,
                               const QueueFamilyIndices& indices,
                               uint32_t                  frameCount)
    : device(pDevice) {
    VkCommandPoolCreateInfo commandPoolInfo{};
    commandPoolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolInfo.queueFamilyIndex = indices.graphicsFamilyIndex.value();

    // CREATE COMMAND POOL
    check(vkCreateCommandPool(this->device, &commandPoolInfo, nullptr, &commandPool),
          "vkCreateCommandPool");

    commandBuffers.resize(frameCount);

    // ALLocate Command Buffers
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = this->commandPool;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

    check(vkAllocateCommandBuffers(this->device, &allocInfo, this->commandBuffers.data()),
          "vkAllocatCommandBuffers");
}

CommandBuffers::~CommandBuffers() {
    vkDestroyCommandPool(this->device, this->commandPool, nullptr);
}

VkCommandBuffer CommandBuffers::record(uint32_t         frameIndex,
                                       VkImage          image,
                                       VkImageView      imageView,
                                       VkExtent2D       extent,
                                       VkPipeline       pipeline,
                                       VkPipelineLayout pipelineLayout,
                                       VkDescriptorSet  descriptorSet,
                                       const Mesh&      mesh) {
    VkCommandBuffer cmd = commandBuffers.at(frameIndex);
    check(vkResetCommandBuffer(cmd, 0), "vkResetCommandBuffer");

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; // submited once
    check(vkBeginCommandBuffer(cmd, &beginInfo), "vkBeginCommandBuffer");

    VkImageMemoryBarrier barrierBefore{};
    barrierBefore.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrierBefore.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
    barrierBefore.newLayout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrierBefore.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrierBefore.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrierBefore.image               = image;
    barrierBefore.subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    barrierBefore.srcAccessMask       = 0;
    barrierBefore.dstAccessMask       = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         0,
                         0,
                         nullptr,
                         0,
                         nullptr,
                         1,
                         &barrierBefore);

    const VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}}; // opaque black

    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView   = imageView;
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
    vkCmdBeginRendering(cmd, &renderingInfo);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdBindDescriptorSets(cmd,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipelineLayout,
                            0,
                            1,
                            &descriptorSet,
                            0,
                            nullptr);

    VkBuffer     vertexBuffers[] = {mesh.vertexBufferHandle()};
    VkDeviceSize offsets[]       = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(cmd, mesh.indexBufferHandle(), 0, VK_INDEX_TYPE_UINT16);

    VkViewport viewport{0.0f, 0.0f, (float)extent.width, (float)extent.height, 0.0f, 1.0f};
    VkRect2D   scissor{{0, 0}, extent};
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // index count, instance count, firstIndex, vertexOffset, firstInstance
    vkCmdDrawIndexed(cmd, mesh.indexCount(), 1, 0, 0, 0);
    vkCmdEndRendering(cmd);

    VkImageMemoryBarrier barrierAfter{};
    barrierAfter.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrierAfter.oldLayout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrierAfter.newLayout           = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barrierAfter.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrierAfter.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrierAfter.image               = image;
    barrierAfter.subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    barrierAfter.srcAccessMask       = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrierAfter.dstAccessMask       = 0;
    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                         0,
                         0,
                         nullptr,
                         0,
                         nullptr,
                         1,
                         &barrierAfter);

    check(vkEndCommandBuffer(cmd), "vkEndCommandBuffer");
    return cmd;
}
