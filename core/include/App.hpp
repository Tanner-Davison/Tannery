#pragma once

#include "LogicalDevice.hpp"
#include "RenderPass.hpp"
#include "Surface.hpp"
#include "Swapchain.hpp"
#include "VulkanInstance.hpp"
#include "Window.hpp"
#include "queueFamilies.hpp"
#include "swapchainSupport.hpp"

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
    static VkPhysicalDevice   pickPhysicalDevice(VkInstance instance);
    static QueueFamilyIndices pickQueueFamilies(VkPhysicalDevice physicalDevice,
                                                VkSurfaceKHR     surface);
    static SwapchainSupport   pickSwapchainSupport(VkPhysicalDevice physicalDevice,
                                                   VkSurfaceKHR     surface);
    Window                    window;
    VulkanInstance            instance;
    Surface                   surface;
    VkPhysicalDevice          physicalDevice;
    QueueFamilyIndices        indices;
    LogicalDevice             device;
    SwapchainSupport          support;
    Swapchain                 swapchain;
    RenderPass                renderPass;
};
