#pragma once
#include "CommandBuffers.hpp"
#include "GraphicsContext.hpp"
#include "Mesh.hpp"
#include "Pipeline.hpp"
#include "Swapchain.hpp"
#include "SyncObjects.hpp"
#include "Window.hpp"
#include "swapchainSupport.hpp"
#include <memory>
#include <vector>

class Renderer {
  public:
    Renderer(const GraphicsContext& context, const Window& window, const Mesh& mesh);
    ~Renderer() = default;

    Renderer(const Renderer&)            = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&)                 = delete;
    Renderer& operator=(Renderer&&)      = delete;

    void drawFrame();
    void onFramebufferResized();

  private:
    static SwapchainSupport         pickSwapchainSupport(VkPhysicalDevice physicalDevice,
                                                         VkSurfaceKHR     surface);
    std::unique_ptr<CommandBuffers> buildCommandBuffers() const;
    void                            recreateSwapchain();

    // Borrowed, owned by App (declared before the Renderer there, so they outlive it)
    const GraphicsContext& context;
    GLFWwindow*            window;
    const Mesh&            mesh;

    bool     framebufferResized = false;
    uint32_t currentFrame       = 0;

    // Owned. Declaration order = dependency order
    SwapchainSupport                support;
    std::unique_ptr<Swapchain>      swapchain;
    std::unique_ptr<SyncObjects>    syncObjects;
    std::unique_ptr<Pipeline>       pipeline;
    std::unique_ptr<CommandBuffers> commandBuffers;
    std::vector<VkFence>            imagesInFlight;
};
