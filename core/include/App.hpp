#pragma once

#include "CommandBuffers.hpp"
#include "FrameBuffers.hpp"
#include "LogicalDevice.hpp"
#include "Pipeline.hpp"
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
    FrameBuffers              frameBuffers;
    Pipeline                  pipeline;
    CommandBuffers            commandBuffers;
};

/*What does a VkFramebuffer actually do? */

/* 1. Binds Real Memory to Shaders: Shaders output data to attachment index 0, 1, etc. The
 * VkFramebuffer ensures that index 0 points to a real memory allocation (like a swapchain
 * image).
 *
 * 2. Defines the Render Canvas Dimensions: When creating a VkFramebufferCreateInfo, you
 * explicitly supply the width, height, and layers. This dictates the execution boundaries of
 * your drawing operations.
 *
 * 3. Facilitates Multiple Buffering (Swapchains): Because a swapchain
 * gives you multiple images to prevent screen tearing, you typically have to create an array
 * of VkFramebuffer objects—one for every single image in your swapchain. When rendering a
 * frame, you look up the active swapchain image index and bind the corresponding VkFramebuffer
 */
