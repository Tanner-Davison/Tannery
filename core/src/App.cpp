#include "App.hpp"
#include "physicalDevice.hpp"
#include <filesystem>
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
    , syncObjects(device.handle(), swapchain.imageCountHandle())
    , pipeline(device.handle(),
               swapchain.extentHandle(),
               std::filesystem::path(SHADER_DIR) / "triangle.vert.spv",
               std::filesystem::path(SHADER_DIR) / "triangle.frag.spv",
               swapchain.formatHandle().format)
    , commandBuffers(device.handle(),
                     swapchain.imageViewsHandle(),
                     swapchain.imagesHandle(),
                     pipeline.pipelineHandle(),
                     swapchain.extentHandle(),
                     indices) {}

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

void App::drawFrame() const {
    VkFence fence = syncObjects.getFence();
    vkWaitForFences(this->device.handle(), 1, &fence, VK_TRUE, UINT64_MAX);
    vkResetFences(this->device.handle(), 1, &fence);
    std::vector<VkSemaphore> renderCompleteSemaphores =
        syncObjects.getRenderCompleteSemaphores();
    uint32_t imageIndex;
    vkAcquireNextImageKHR(this->device.handle(),
                          swapchain.handle(),
                          UINT64_MAX,
                          syncObjects.getImageAvailableSemaphore(),
                          VK_NULL_HANDLE,
                          &imageIndex);

    VkSemaphore          waitSemaphores[] = {syncObjects.getImageAvailableSemaphore()};
    VkPipelineStageFlags waitStages[]     = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSemaphore          signalSemaphore  = renderCompleteSemaphores[imageIndex];
    std::vector<VkCommandBuffer> _commandBuffers = commandBuffers.getCmdBuffers();

    VkSubmitInfo submitInfo{};
    submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount   = 1;
    submitInfo.pWaitSemaphores      = waitSemaphores;
    submitInfo.pWaitDstStageMask    = waitStages;
    submitInfo.commandBufferCount   = 1;
    submitInfo.pCommandBuffers      = &_commandBuffers[imageIndex];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores    = &signalSemaphore;

    vkQueueSubmit(device.GraphicsQueueHandle(), 1, &submitInfo, fence);

    VkSwapchainKHR   swapchains[] = {this->swapchain.handle()};
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = &signalSemaphore;
    presentInfo.swapchainCount     = 1;
    presentInfo.pSwapchains        = swapchains;
    presentInfo.pImageIndices      = &imageIndex;
    presentInfo.pResults           = nullptr;

    vkQueuePresentKHR(device.PresentQueueHandle(), &presentInfo);
};

void App::run() {
    while (!glfwWindowShouldClose(window.handle())) {
        glfwPollEvents();

        drawFrame();
    }
    // Ensures my classes destructors all run before we exit
    vkDeviceWaitIdle(this->device.handle());
}
