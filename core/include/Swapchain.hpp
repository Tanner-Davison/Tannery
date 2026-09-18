#pragma once

#include "queueFamilies.hpp"
#include "swapchainSupport.hpp"
#include <GLFW/glfw3.h>
#include <vector>

class Swapchain {
  public:
    // Constructor
    Swapchain(VkDevice                  pLogicalDevice,
              VkSurfaceKHR              surface,
              const SwapchainSupport&   support,
              GLFWwindow*               window,
              const QueueFamilyIndices& indices);

    // Copy && move constructors deletion
    Swapchain(const Swapchain&)            = delete;
    Swapchain& operator=(const Swapchain&) = delete;
    Swapchain(Swapchain&&)                 = delete;
    Swapchain& operator=(Swapchain&&)      = delete;

    // Destructor
    ~Swapchain();

    // Handles
    VkSwapchainKHR     handle() const;
    VkSurfaceFormatKHR formatHandle() const;
    VkPresentModeKHR   presentHandle() const;
    VkExtent2D         extentHandle() const;

  private:
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const SwapchainSupport& support);
    VkPresentModeKHR   chooseSwapPresentMode(const SwapchainSupport& support);
    VkExtent2D         chooseSwapExtent(GLFWwindow*                     window,
                                        const VkSurfaceCapabilitiesKHR& capabilities);

    /*  Unlike the swapchain images themselves, you create and own each imageView yourself
     * (vkCreateImageView/vkDestroyImageView) */
    std::vector<VkImageView> imageViews;
    std::vector<VkImage>     images;
    VkDevice                 deviceHandle = VK_NULL_HANDLE;
    VkSwapchainKHR           swapchain    = VK_NULL_HANDLE;
    VkSurfaceFormatKHR       surfaceFormat;
    VkPresentModeKHR         presentMode;
    VkExtent2D               extent;
};
