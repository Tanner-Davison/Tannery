#include "App.hpp"
#include "physicalDevice.hpp"
#include <stdexcept>

App::App(int width, int height, const char* title)
    : window(width, height, title)
    , instance(title)
    , surface(instance.handle(), window.handle())
    , physicalDevice(pickPhysicalDevice(instance.handle()))
    , indices(pickQueueFamilies(physicalDevice, surface.handle()))
    , device(physicalDevice, indices)
    , support(pickSwapchainSupport(physicalDevice, surface.handle()))
    , swapchain(device.handle(), surface.handle(), support, window.handle(), indices)
    , renderPass(device.handle(), swapchain.formatHandle()) {}

VkPhysicalDevice App::pickPhysicalDevice(VkInstance instance) {
    VkPhysicalDevice physicalDevice = getPhysicalDevice(instance);
    if (physicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("Failed to find suitable device");
    }
    return physicalDevice;
}

QueueFamilyIndices App::pickQueueFamilies(VkPhysicalDevice physicalDevice,
                                          VkSurfaceKHR     surface) {
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice, surface);

    if (!indices.isComplete()) {
        throw std::runtime_error("Missing Graphics or Present family index!");
    }
    return indices;
}

SwapchainSupport App::pickSwapchainSupport(VkPhysicalDevice physicalDevice,
                                           VkSurfaceKHR     surface) {
    SwapchainSupport support = getSwapchainSupportDetails(physicalDevice, surface);
    if (!support.isComplete()) {
        throw std::runtime_error("Swapchain Support failed to setup");
    }
    return support;
}

void App::run() {
    while (!glfwWindowShouldClose(window.handle())) {
        glfwPollEvents();
    }
}
