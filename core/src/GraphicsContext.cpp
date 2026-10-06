#include "GraphicsContext.hpp"
#include "physicalDevice.hpp"
#include "vk_enum_string_helper.h"
#include <format>
#include <stdexcept>

namespace {
// Destroys the temporary pool when the function exits, even if it throws
struct PoolGuard {
    VkDevice      device;
    VkCommandPool pool;

    ~PoolGuard() {
        vkDestroyCommandPool(device, pool, nullptr);
    }
};
} // namespace

GraphicsContext::GraphicsContext(const Window& window, const char* appName)
    : instance(appName)
    , surface(instance.handle(), window.handle())
    , physicalDevice(pickPhysicalDevice(instance.handle()))
    , indices(pickQueueFamilies(physicalDevice, surface.handle()))
    , device(physicalDevice, indices)
    , allocator(instance.handle(), physicalDevice, device.handle()) {}

VkPhysicalDevice GraphicsContext::pickPhysicalDevice(VkInstance instance) {
    VkPhysicalDevice physicalDevice = getPhysicalDevice(instance);
    if (physicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("Failed to find suitable device");
    }
    return physicalDevice;
}

QueueFamilyIndices GraphicsContext::pickQueueFamilies(VkPhysicalDevice physicalDevice,
                                                      VkSurfaceKHR     surface) {
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice, surface);
    if (!indices.isComplete()) {
        throw std::runtime_error("Missing Graphics or Present family index!");
    }
    return indices;
}

VkInstance GraphicsContext::instanceHandle() const {
    return instance.handle();
}

VkSurfaceKHR GraphicsContext::surfaceHandle() const {
    return surface.handle();
}

VkPhysicalDevice GraphicsContext::physicalDeviceHandle() const {
    return physicalDevice;
}

VkDevice GraphicsContext::deviceHandle() const {
    return device.handle();
}

VkQueue GraphicsContext::graphicsQueue() const {
    return device.GraphicsQueueHandle();
}

VkQueue GraphicsContext::presentQueue() const {
    return device.PresentQueueHandle();
}

VmaAllocator GraphicsContext::allocatorHandle() const {
    return allocator.handle();
}

const QueueFamilyIndices& GraphicsContext::queueFamilies() const {
    return indices;
}

void GraphicsContext::waitIdle() const {
    vkDeviceWaitIdle(device.handle());
}

void GraphicsContext::immediateSubmit(
    const std::function<void(VkCommandBuffer)>& record) const {
    // short-lived pool just for this submission
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    poolInfo.queueFamilyIndex = indices.graphicsFamilyIndex.value();

    VkCommandPool pool = VK_NULL_HANDLE;
    VkResult      res  = vkCreateCommandPool(device.handle(), &poolInfo, nullptr, &pool);
    if (res != VK_SUCCESS) {
        throw std::runtime_error(
            std::format("immediateSubmit: pool failed: {}", string_VkResult(res)));
    }
    PoolGuard guard(device.handle(), pool);

    // one command buffer from that pool
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = pool;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    res                 = vkAllocateCommandBuffers(device.handle(), &allocInfo, &cmd);
    if (res != VK_SUCCESS) {
        throw std::runtime_error(
            std::format("immediateSubmit: allocate failed: {}", string_VkResult(res)));
    }

    // record: begin, caller's commands, end
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);

    record(cmd);

    vkEndCommandBuffer(cmd);

    // submit and wait until the GPU has finished
    VkSubmitInfo submitInfo{};
    submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &cmd;
    res = vkQueueSubmit(device.GraphicsQueueHandle(), 1, &submitInfo, VK_NULL_HANDLE);
    if (res != VK_SUCCESS) {
        throw std::runtime_error(
            std::format("immediateSubmit: submit failed: {}", string_VkResult(res)));
    }
    vkQueueWaitIdle(device.GraphicsQueueHandle());
}

std::unique_ptr<Buffer> GraphicsContext::createDeviceLocalBuffer(
    const void*        data,
    VkDeviceSize       size,
    VkBufferUsageFlags usage) const {
    // 1. CPU-visible staging buffer, filled with the data
    Buffer   staging(allocator.handle(),
                   size,
                   VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                   VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
    VkResult res = vmaCopyMemoryToAllocation(allocator.handle(),
                                             data,
                                             staging.allocationHandle(),
                                             0,
                                             size);
    if (res != VK_SUCCESS) {
        throw std::runtime_error(
            std::format("Staging upload failed. VkError: {}", string_VkResult(res)));
    }

    // 2. GPU-local destination buffer
    auto deviceBuffer = std::make_unique<Buffer>(allocator.handle(),
                                                 size,
                                                 usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                 0);

    // 3. Copy and wait; `staging` is destroyed on return, after the GPU is done with it
    immediateSubmit([&](VkCommandBuffer cmd) {
        VkBufferCopy region{};
        region.srcOffset = 0;
        region.dstOffset = 0;
        region.size      = size;
        vkCmdCopyBuffer(cmd, staging.handle(), deviceBuffer->handle(), 1, &region);
    });
    return deviceBuffer;
}
