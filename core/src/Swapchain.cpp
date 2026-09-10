#include "Swapchain.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>

Swapchain::Swapchain(VkDevice                  pLogicalDevice,
                     VkSurfaceKHR              surface,
                     const SwapchainSupport&   support,
                     GLFWwindow*               pWindow,
                     const QueueFamilyIndices& indices)
    : deviceHandle(pLogicalDevice) {
    this->surfaceFormat = chooseSwapSurfaceFormat(support);
    this->presentMode   = chooseSwapPresentMode(support);
    this->extent        = chooseSwapExtent(pWindow, support.capabilities);

    uint32_t capabilityImageCount = support.capabilities.minImageCount + 1;

    /*This makes sure we do not exceed the capable max image count*/
    if (support.capabilities.maxImageCount > 0 &&
        capabilityImageCount > support.capabilities.maxImageCount) {
        capabilityImageCount = support.capabilities.maxImageCount;
    }
    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface          = surface;
    createInfo.minImageCount    = capabilityImageCount;
    createInfo.imageFormat      = surfaceFormat.format;
    createInfo.imageColorSpace  = surfaceFormat.colorSpace;
    createInfo.imageExtent      = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32_t queueFamilyIndices[] = {indices.graphicsFamilyIndex.value(),
                                     indices.presentFamilyIndex.value()};

    if (indices.graphicsFamilyIndex != indices.presentFamilyIndex) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        /* VK_SHARING_MODE_CONCURRENT: Images can be used across multiple queue families
         * without explicit ownership transfers.*/
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices   = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        /* VK_SHARING_MODE_EXCLUSIVE image is owned by one queue family at a time and ownership
         * must be explicitly transferred before using it in another queue family. This option
         * offers the best performance. */
        createInfo.queueFamilyIndexCount = 0; // optional
        createInfo.pQueueFamilyIndices   = nullptr;
    }
    createInfo.preTransform = support.capabilities.currentTransform;
    /* We can specify that a certain transform should be applied to images in the swap chain if
     * it is supported (supportedTransforms in capabilities), like a 90 degree clockwise
     * rotation or horizontal flip. To specify that you do not want any transformation, simply
     * specify the current transformation.*/
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    /* The compositeAlpha field specifies if the alpha channel should be used for blending with
     * other windows in the window system. You'll almost always want to simply ignore the alpha
     * channel, hence VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR.*/
    createInfo.presentMode  = presentMode;
    createInfo.clipped      = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(pLogicalDevice, &createInfo, nullptr, &this->swapchain) !=
        VK_SUCCESS) {
        throw std::runtime_error("Error: Failed to create swapchain");
    }

    uint32_t imagesCount;
    vkGetSwapchainImagesKHR(this->deviceHandle, this->swapchain, &imagesCount, nullptr);

    this->images.resize(imagesCount);

    vkGetSwapchainImagesKHR(this->deviceHandle,
                            this->swapchain,
                            &imagesCount,
                            this->images.data());

    VkImageViewCreateInfo createImageViewInfo{};
    createImageViewInfo.sType                       = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createImageViewInfo.viewType                    = VK_IMAGE_VIEW_TYPE_2D;
    createImageViewInfo.format                      = this->surfaceFormat.format;
    createImageViewInfo.components.a                = VK_COMPONENT_SWIZZLE_IDENTITY;
    createImageViewInfo.components.b                = VK_COMPONENT_SWIZZLE_IDENTITY;
    createImageViewInfo.components.g                = VK_COMPONENT_SWIZZLE_IDENTITY;
    createImageViewInfo.components.r                = VK_COMPONENT_SWIZZLE_IDENTITY;
    createImageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    createImageViewInfo.subresourceRange.baseMipLevel   = 0;
    createImageViewInfo.subresourceRange.levelCount     = 1;
    createImageViewInfo.subresourceRange.baseArrayLayer = 0;
    createImageViewInfo.subresourceRange.layerCount     = 1;

    // resize imageViews to match the count of images;
    this->imageViews.resize(this->images.size());

    // create an Image View for every image we have
    for (size_t i = 0; i < this->images.size(); i++) {
        createImageViewInfo.image      = this->images[i];
        VkResult createImageViewResult = vkCreateImageView(this->deviceHandle,
                                                           &createImageViewInfo,
                                                           nullptr,
                                                           &this->imageViews[i]);
        if (createImageViewResult != VK_SUCCESS) {
            throw std::runtime_error("Error: Failed to create Image view");
        }
    }
};

//  DESTRUCTOR
Swapchain::~Swapchain() {
    if (!this->imageViews.empty()) {
        for (const auto& image : imageViews) {
            vkDestroyImageView(this->deviceHandle, image, nullptr);
        }
    }
    if (this->swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(this->deviceHandle, this->swapchain, nullptr);
    }
}

// Retreive member Handles

VkSwapchainKHR Swapchain::handle() const {
    return this->swapchain;
};

VkSurfaceFormatKHR Swapchain::formatHandle() const {
    return this->surfaceFormat;
};

VkPresentModeKHR Swapchain::presentHandle() const {
    return this->presentMode;
};

VkExtent2D Swapchain::extentHandle() const {
    return this->extent;
};

/*_____________ SWAPCHAIN SETTINGS____________________________*/
/* THREE Types of settings to determine when setting up a swapchain
 * -----------------------------------------------------------
 * 1. Surface format    (Color Depth)
 * 2. Presentation mode (conditions for "Swapping" images to the screen)
 * 3. Swap extent       (resolution of images in swapchain)
 */
VkSurfaceFormatKHR Swapchain::chooseSwapSurfaceFormat(const SwapchainSupport& support) {
    constexpr VkFormat        PREFFERED_FORMAT      = VK_FORMAT_B8G8R8A8_SRGB;
    constexpr VkColorSpaceKHR PREFFERED_COLOR_SPACE = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

    for (const auto& availableFormat : support.formats) {
        if (availableFormat.format == PREFFERED_FORMAT &&
            availableFormat.colorSpace == PREFFERED_COLOR_SPACE) {
            return availableFormat;
        }
    }
    return support.formats[0];
}

VkPresentModeKHR Swapchain::chooseSwapPresentMode(const SwapchainSupport& support) {
    constexpr VkPresentModeKHR PREFFERED_PRESENT_MODE = VK_PRESENT_MODE_MAILBOX_KHR;

    for (const auto& presentMode : support.presentationModes) {
        if (presentMode == PREFFERED_PRESENT_MODE) {
            return presentMode;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

/* What's happening: when you're on an Apple M3 Pro with a Retina display, which has a 2x pixel
 * density (also called a HiDPI/scale factor). GLFW's glfwCreateWindow(800, 600, ...) sizes the
 * window in logical/screen coordinates — "800x600" in the units macOS uses for window sizing,
 * which is not the same as actual physical pixels on a Retina screen. On a 2x display, 800
 * logical units = 1600 actual pixels. Same for height: 600 → 1200. That's exactly the doubling
 * you're seeing. */
VkExtent2D Swapchain::chooseSwapExtent(GLFWwindow*                     window,
                                       const VkSurfaceCapabilitiesKHR& capabilities) {
    /*std::numeric_limits<uint32_t>::max() This is just 4294967295 (i.e. UINT32_MAX) the
     * largest value a uint32_t can hold.*/
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    } else {
        int width, height;

        glfwGetFramebufferSize(window, &width, &height);

        VkExtent2D actualExtent = {static_cast<uint32_t>(width),
                                   static_cast<uint32_t>(height)};

        actualExtent.width  = std::clamp(actualExtent.width,
                                        capabilities.minImageExtent.width,
                                        capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height,
                                         capabilities.minImageExtent.height,
                                         capabilities.maxImageExtent.height);

        return actualExtent;
    }
};
