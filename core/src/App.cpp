#include "App.hpp"
#include "copyBuffer.hpp"
#include "physicalDevice.hpp"
#include <filesystem>
#include <memory>
#include <stdexcept>

App::App(int width, int height, const char* title)
    : window(width, height, title)
    , instance(title)
    , surface(instance.handle(), window.handle())
    , physicalDevice(pickPhysicalDevice(instance.handle()))
    , indices(pickQueueFamilies(physicalDevice, surface.handle()))
    , device(physicalDevice, indices)
    , allocator(this->instance.handle(), this->physicalDevice, this->device.handle())
    , vertexBuffer(createVertexBuffer(allocator.handle(),
                                      device.handle(),
                                      device.GraphicsQueueHandle(),
                                      indices.graphicsFamilyIndex.value()))
    , support(pickSwapchainSupport(physicalDevice, surface.handle()))
    , swapchain(std::make_unique<Swapchain>(device.handle(),
                                            surface.handle(),
                                            support,
                                            window.handle(),
                                            indices))
    , syncObjects(
          std::make_unique<SyncObjects>(device.handle(), swapchain->imageCountHandle()))
    , pipeline(
          std::make_unique<Pipeline>(device.handle(),
                                     swapchain->extentHandle(),
                                     std::filesystem::path(SHADER_DIR) / "triangle.vert.spv",
                                     std::filesystem::path(SHADER_DIR) / "triangle.frag.spv",
                                     swapchain->formatHandle().format))
    , commandBuffers(std::make_unique<CommandBuffers>(device.handle(),
                                                      swapchain->imageViewsHandle(),
                                                      swapchain->imagesHandle(),
                                                      pipeline->pipelineHandle(),
                                                      swapchain->extentHandle(),
                                                      indices,
                                                      vertexBuffer->handle())) {
    // Window Resize
    glfwSetWindowUserPointer(window.handle(), this);
    glfwSetFramebufferSizeCallback(window.handle(), [](GLFWwindow* w, int, int) {
        auto* app               = static_cast<App*>(glfwGetWindowUserPointer(w));
        app->frameBufferResized = true;
    });

    imagesInFlight.assign(swapchain->imageCountHandle(), VK_NULL_HANDLE);
}

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

std::unique_ptr<Buffer> App::createVertexBuffer(VmaAllocator pAllocator,
                                                VkDevice     pDevice,
                                                VkQueue      pQueue,
                                                uint32_t     pQueueFamilyIndex) {
    const std::vector<Vertex> vertices = {
        {{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
        {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
        {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
    };
    const VkDeviceSize size = sizeof(Vertex) * vertices.size();
    // 1. CPU-Visible staging buffer, filled with vertices
    Buffer staging(pAllocator,
                   size,
                   VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                   VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
    vmaCopyMemoryToAllocation(pAllocator,
                              vertices.data(),
                              staging.allocationHandle(),
                              0,
                              size);
    // 2. GPU-local vertex buffer
    auto vertexBuffer = std::make_unique<Buffer>(
        pAllocator,
        size,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        0);
    // 3. copy staging into it and wait
    copyBuffer(pDevice,
               pQueue,
               pQueueFamilyIndex,
               staging.handle(),
               vertexBuffer->handle(),
               size);
    return vertexBuffer;
};

void App::drawFrame() {
    if (frameBufferResized) {
        frameBufferResized = false;
        recreateSwapchain();
    }
    VkFence fence = syncObjects->getFence(currentFrame);

    vkWaitForFences(this->device.handle(), 1, &fence, VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;

    VkResult result =
        vkAcquireNextImageKHR(this->device.handle(),
                              swapchain->handle(),
                              UINT64_MAX,
                              syncObjects->getImageAvailableSemaphore(currentFrame),
                              VK_NULL_HANDLE,
                              &imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain();
        return;
    }

    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Failed to acquire swapchain image");
    }
    if (imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
        vkWaitForFences(device.handle(), 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
    }
    imagesInFlight[imageIndex] = fence;

    vkResetFences(this->device.handle(), 1, &fence);

    VkSemaphore renderCompleteSemaphore = syncObjects->getRenderCompleteSemaphore(imageIndex);
    VkSemaphore waitSemaphores[] = {syncObjects->getImageAvailableSemaphore(currentFrame)};
    VkPipelineStageFlags waitStages[]    = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSemaphore          signalSemaphore = renderCompleteSemaphore;
    VkCommandBuffer      _commandBuffer  = commandBuffers->getCmdBuffer(imageIndex);

    VkSubmitInfo submitInfo{};
    submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount   = 1;
    submitInfo.pWaitSemaphores      = waitSemaphores;
    submitInfo.pWaitDstStageMask    = waitStages;
    submitInfo.commandBufferCount   = 1;
    submitInfo.pCommandBuffers      = &_commandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores    = &signalSemaphore;

    vkQueueSubmit(device.GraphicsQueueHandle(), 1, &submitInfo, fence);

    VkSwapchainKHR   swapchains[] = {this->swapchain->handle()};
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = &signalSemaphore;
    presentInfo.swapchainCount     = 1;
    presentInfo.pSwapchains        = swapchains;
    presentInfo.pImageIndices      = &imageIndex;
    presentInfo.pResults           = nullptr;

    VkResult presentResult = vkQueuePresentKHR(device.PresentQueueHandle(), &presentInfo);

    // RESIZE CALLBACK------------
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR ||
        frameBufferResized) {
        frameBufferResized = false;
        recreateSwapchain();
    }
    //-----------------------------

    if (currentFrame == (SyncObjects::MAX_FRAMES_IN_FLIGHT - 1)) {
        currentFrame = 0;
    } else {
        ++currentFrame;
    }
};

void App::run() {
    while (!glfwWindowShouldClose(window.handle())) {
        glfwPollEvents();

        drawFrame();
    }
    // Ensures my classes destructors all run before we exit
    vkDeviceWaitIdle(this->device.handle());
}

void App::recreateSwapchain() {
    // Wait out a minimized window (0x0 framebuffer)
    int width = 0, height = 0;
    glfwGetFramebufferSize(window.handle(), &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window.handle(), &width, &height);
        glfwWaitEvents();
    }

    // GPU must be done with everything we are about to destroy
    vkDeviceWaitIdle(device.handle());

    // 3. Refresh the cached surface capabilities (new currentExtent)
    support = pickSwapchainSupport(this->physicalDevice, surface.handle());

    // 4. Destroy old objects, dependants first
    commandBuffers.reset();
    syncObjects.reset();
    auto oldSwapchain = std::move(swapchain);
    swapchain         = std::make_unique<Swapchain>(device.handle(),
                                            surface.handle(),
                                            support,
                                            window.handle(),
                                            indices,
                                            oldSwapchain->handle());
    oldSwapchain.reset();
    // 5. Rebuild in dependancy order ( same expressions as the constructor )

    syncObjects =
        std::make_unique<SyncObjects>(device.handle(), swapchain->imageCountHandle());
    commandBuffers = std::make_unique<CommandBuffers>(device.handle(),
                                                      swapchain->imageViewsHandle(),
                                                      swapchain->imagesHandle(),
                                                      pipeline->pipelineHandle(),
                                                      swapchain->extentHandle(),
                                                      indices,
                                                      vertexBuffer->handle());
    imagesInFlight.assign(swapchain->imageCountHandle(), VK_NULL_HANDLE);
}
