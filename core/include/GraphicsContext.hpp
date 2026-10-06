#pragma once
#include "Allocator.hpp"
#include "Buffer.hpp"
#include "LogicalDevice.hpp"
#include "Surface.hpp"
#include "VulkanInstance.hpp"
#include "Window.hpp"
#include "queueFamilies.hpp"
#include <functional>
#include <memory>

class GraphicsContext {
  public:
    GraphicsContext(const Window& window, const char* appName);
    ~GraphicsContext() = default;

    GraphicsContext(const GraphicsContext&)            = delete;
    GraphicsContext& operator=(const GraphicsContext&) = delete;
    GraphicsContext(GraphicsContext&&)                 = delete;
    GraphicsContext& operator=(GraphicsContext&&)      = delete;

    VkInstance                instanceHandle() const;
    VkSurfaceKHR              surfaceHandle() const;
    VkPhysicalDevice          physicalDeviceHandle() const;
    VkDevice                  deviceHandle() const;
    VkQueue                   graphicsQueue() const;
    VkQueue                   presentQueue() const;
    VmaAllocator              allocatorHandle() const;
    const QueueFamilyIndices& queueFamilies() const;

    void waitIdle() const;

    // One-shot GPU work: records `record` into a temporary command buffer, submits it to the
    // graphics queue and blocks until it finishes. For uploads at load time, not per frame.
    void immediateSubmit(const std::function<void(VkCommandBuffer)>& record) const;

    // Staging upload: CPU data -> GPU-local buffer (TRANSFER_DST is added for you)
    std::unique_ptr<Buffer> createDeviceLocalBuffer(const void*        data,
                                                    VkDeviceSize       size,
                                                    VkBufferUsageFlags usage) const;

  private:
    static VkPhysicalDevice   pickPhysicalDevice(VkInstance instance);
    static QueueFamilyIndices pickQueueFamilies(VkPhysicalDevice physicalDevice,
                                                VkSurfaceKHR     surface);

    // Declaration order IS the dependency order (destroyed in reverse)
    VulkanInstance     instance;
    Surface            surface;
    VkPhysicalDevice   physicalDevice;
    QueueFamilyIndices indices;
    LogicalDevice      device;
    Allocator          allocator;
};
