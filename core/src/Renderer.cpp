#include "Renderer.hpp"
#include "vk_enum_string_helper.h"
#include <filesystem>
#include <format>
#include <stdexcept>

Renderer::Renderer(const GraphicsContext& ctx, const Window& win, const Mesh& m)
    : context(ctx)
    , window(win.handle())
    , mesh(m)
    , support(pickSwapchainSupport(ctx.physicalDeviceHandle(), ctx.surfaceHandle()))
    , swapchain(std::make_unique<Swapchain>(ctx.deviceHandle(),
                                            ctx.surfaceHandle(),
                                            support,
                                            window,
                                            ctx.queueFamilies()))
    , syncObjects(
          std::make_unique<SyncObjects>(ctx.deviceHandle(), swapchain->imageCountHandle()))
    , descriptors(ctx, SyncObjects::MAX_FRAMES_IN_FLIGHT)
    , pipeline(ctx.deviceHandle(),
               swapchain->extentHandle(),
               std::filesystem::path(SHADER_DIR) / "triangle.vert.spv",
               std::filesystem::path(SHADER_DIR) / "triangle.frag.spv",
               swapchain->formatHandle().format,
               descriptors.layoutHandle())
    , commandBuffers(ctx.deviceHandle(),
                     ctx.queueFamilies(),
                     SyncObjects::MAX_FRAMES_IN_FLIGHT) {}

SwapchainSupport Renderer::pickSwapchainSupport(VkPhysicalDevice physicalDevice,
                                                VkSurfaceKHR     surface) {
    SwapchainSupport support = getSwapchainSupportDetails(physicalDevice, surface);
    if (!support.isComplete()) {
        throw std::runtime_error("Swapchain Support failed to setup");
    }
    return support;
}

void Renderer::onFramebufferResized() {
    framebufferResized = true;
}

float Renderer::aspectRatio() const {
    const VkExtent2D e = swapchain->extentHandle();
    return static_cast<float>(e.width) / static_cast<float>(e.height);
}

void Renderer::drawFrame(const CameraUBO& camera) {
    if (framebufferResized) {
        framebufferResized = false;
        recreateSwapchain();
    }
    const VkDevice device = context.deviceHandle();
    VkFence        fence  = syncObjects->getFence(currentFrame);

    // Once this returns, the GPU is done with this slot's previous frame: its command
    // buffer and its uniform buffer are safe to overwrite.
    VkResult res = vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
    if (res != VK_SUCCESS) {
        throw std::runtime_error(
            std::format("vkWaitForFences failed. VkError: {}", string_VkResult(res)));
    }

    uint32_t imageIndex;
    res = vkAcquireNextImageKHR(device,
                                swapchain->handle(),
                                UINT64_MAX,
                                syncObjects->getImageAvailableSemaphore(currentFrame),
                                VK_NULL_HANDLE,
                                &imageIndex);
    if (res == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain();
        return;
    }
    if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error(std::format("Failed to acquire swapchain image. VkError: {}",
                                             string_VkResult(res)));
    }
    vkResetFences(device, 1, &fence); // only now that a submit is certain

    descriptors.update(currentFrame, camera);
    VkCommandBuffer commandBuffer =
        commandBuffers.record(currentFrame,
                              swapchain->imagesHandle()[imageIndex],
                              swapchain->imageViewsHandle()[imageIndex],
                              swapchain->extentHandle(),
                              pipeline.pipelineHandle(),
                              pipeline.pipelineLayoutHandle(),
                              descriptors.setHandle(currentFrame),
                              mesh);

    VkSemaphore          waitSemaphore = syncObjects->getImageAvailableSemaphore(currentFrame);
    VkSemaphore          signalSemaphore = syncObjects->getRenderCompleteSemaphore(imageIndex);
    VkPipelineStageFlags waitStage       = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submitInfo{};
    submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount   = 1;
    submitInfo.pWaitSemaphores      = &waitSemaphore;
    submitInfo.pWaitDstStageMask    = &waitStage;
    submitInfo.commandBufferCount   = 1;
    submitInfo.pCommandBuffers      = &commandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores    = &signalSemaphore;

    res = vkQueueSubmit(context.graphicsQueue(), 1, &submitInfo, fence);
    if (res != VK_SUCCESS) {
        throw std::runtime_error(
            std::format("vkQueueSubmit failed. VkError: {}", string_VkResult(res)));
    }

    VkSwapchainKHR   swapchains[] = {swapchain->handle()};
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = &signalSemaphore;
    presentInfo.swapchainCount     = 1;
    presentInfo.pSwapchains        = swapchains;
    presentInfo.pImageIndices      = &imageIndex;

    res = vkQueuePresentKHR(context.presentQueue(), &presentInfo);
    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
        recreateSwapchain();
    } else if (res != VK_SUCCESS) {
        throw std::runtime_error(
            std::format("vkQueuePresentKHR failed. VkError: {}", string_VkResult(res)));
    }

    currentFrame = (currentFrame + 1) % SyncObjects::MAX_FRAMES_IN_FLIGHT;
}

void Renderer::recreateSwapchain() {
    // Wait out a minimized window (0x0 framebuffer)
    int width = 0, height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window, &width, &height);
        glfwWaitEvents();
    }

    context.waitIdle();
    support = pickSwapchainSupport(context.physicalDeviceHandle(), context.surfaceHandle());

    // Only the swapchain and the per-image semaphores depend on the window size now
    syncObjects.reset();
    auto oldSwapchain = std::move(swapchain);
    swapchain         = std::make_unique<Swapchain>(context.deviceHandle(),
                                            context.surfaceHandle(),
                                            support,
                                            window,
                                            context.queueFamilies(),
                                            oldSwapchain->handle());
    oldSwapchain.reset();
    syncObjects =
        std::make_unique<SyncObjects>(context.deviceHandle(), swapchain->imageCountHandle());
}
