#include "copyBuffer.hpp"
#include <format>
#include <stdexcept>
#include <vk_enum_string_helper.h>

namespace {
// Destroys the temporary pool when the function exists, even if it throws
struct PoolGuard {
    VkDevice      device;
    VkCommandPool pool;

    ~PoolGuard() {
        vkDestroyCommandPool(device, pool, nullptr);
    }
};
} // namespace

void copyBuffer(VkDevice     pDevice,
                VkQueue      pQueue,
                uint32_t     pQueueFamilyIndex,
                VkBuffer     pSrc,
                VkBuffer     pDst,
                VkDeviceSize pSize) {
    // short-lived pool just for this copy
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    poolInfo.queueFamilyIndex = pQueueFamilyIndex;

    VkCommandPool pool = VK_NULL_HANDLE;
    VkResult      res  = vkCreateCommandPool(pDevice, &poolInfo, nullptr, &pool);
    if (res != VK_SUCCESS) {
        throw std::runtime_error(
            std::format("copybuffer: pool failed: {}", string_VkResult(res)));
    }
    PoolGuard guard(pDevice, pool);

    // one command buffer from that pool
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = pool;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    res                 = vkAllocateCommandBuffers(pDevice, &allocInfo, &cmd);
    if (res != VK_SUCCESS) {
        throw std::runtime_error(
            std::format("copybuffer: allocate failed: {}", string_VkResult(res)));
    }
    // record: begin, copy, end
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);

    VkBufferCopy region{};
    region.srcOffset = 0;
    region.dstOffset = 0;
    region.size      = pSize;
    vkCmdCopyBuffer(cmd, pSrc, pDst, 1, &region);

    vkEndCommandBuffer(cmd);

    // submit and wait until the gpu has finished the copy
    VkSubmitInfo submitInfo{};
    submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &cmd;
    vkQueueSubmit(pQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(pQueue);
};
