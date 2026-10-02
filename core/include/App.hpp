#pragma once

#include "Allocator.hpp"
#include "Buffer.hpp"
#include "CommandBuffers.hpp"
#include "LogicalDevice.hpp"
#include "Pipeline.hpp"
#include "Surface.hpp"
#include "Swapchain.hpp"
#include "SyncObjects.hpp"
#include "VulkanInstance.hpp"
#include "Window.hpp"
#include "queueFamilies.hpp"
#include "swapchainSupport.hpp"
#include <memory>

class App {
  public:
    App(int width, int height, const char* title);
    ~App() = default;

    void run();
    // copy && move constructor deletions
    App(const App&)            = delete;
    App& operator=(const App&) = delete;
    App(App&&)                 = delete;
    App& operator=(App&&)      = delete;

  private:
    // helpers
    static VkPhysicalDevice        pickPhysicalDevice(VkInstance instance);
    static QueueFamilyIndices      pickQueueFamilies(VkPhysicalDevice physicalDevice,
                                                     VkSurfaceKHR     surface);
    static SwapchainSupport        pickSwapchainSupport(VkPhysicalDevice physicalDevice,
                                                        VkSurfaceKHR     surface);
    static std::unique_ptr<Buffer> createVertexBuffer(VmaAllocator pAllocator,
                                                      VkDevice     pDevice,
                                                      VkQueue      pQueue,
                                                      uint32_t     pQueueFamilyIndex);
    static std::unique_ptr<Buffer> createDeviceLocalBuffer(VmaAllocator pAllocator,
                                                           VkDevice     pDevice,
                                                           VkQueue      pQueue,
                                                           uint32_t     pQueueFamilyIndex,
                                                           const void*  pData,
                                                           VkDeviceSize pSize,
                                                           VkBufferUsageFlags pUsage);
    static std::unique_ptr<Buffer> createIndexBuffer(VmaAllocator pAllocator,
                                                     VkDevice     pDevice,
                                                     VkQueue      pQueue,
                                                     uint32_t     pQueueFamilyIndex);
    bool                           framebufferResized = false;
    std::vector<VkFence>           imagesInFlight;
    void                           recreateSwapchain();
    void                           drawFrame();
    uint32_t                       currentFrame = 0;

    Window                          window;
    VulkanInstance                  instance;
    Surface                         surface;
    VkPhysicalDevice                physicalDevice;
    QueueFamilyIndices              indices;
    LogicalDevice                   device;
    Allocator                       allocator;
    std::unique_ptr<Buffer>         vertexBuffer;
    std::unique_ptr<Buffer>         indexBuffer;
    SwapchainSupport                support;
    std::unique_ptr<Swapchain>      swapchain;
    std::unique_ptr<SyncObjects>    syncObjects;
    std::unique_ptr<Pipeline>       pipeline;
    std::unique_ptr<CommandBuffers> commandBuffers;
};
