#include "GraphicsContext.hpp"
#include "copyBuffer.hpp"
#include "physicalDevice.hpp"
#include "vk_enum_string_helper.h"
#include <format>
#include <stdexcept>

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
    copyBuffer(device.handle(),
               device.GraphicsQueueHandle(),
               indices.graphicsFamilyIndex.value(),
               staging.handle(),
               deviceBuffer->handle(),
               size);
    return deviceBuffer;
}
