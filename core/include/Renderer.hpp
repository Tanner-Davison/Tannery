#pragma once
#include "CommandBuffers.hpp"
#include "FrameDescriptors.hpp"
#include "GraphicsContext.hpp"
#include "Mesh.hpp"
#include "Pipeline.hpp"
#include "Swapchain.hpp"
#include "SyncObjects.hpp"
#include "UniformData.hpp"
#include "Window.hpp"
#include "swapchainSupport.hpp"
#include <memory>

class Renderer {
  public:
    Renderer(const GraphicsContext& context, const Window& window, const Mesh& mesh);
    ~Renderer() = default;

    Renderer(const Renderer&)            = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&)                 = delete;
    Renderer& operator=(Renderer&&)      = delete;

    void  drawFrame(const CameraUBO& camera);
    void  onFramebufferResized();
    float aspectRatio() const;

  private:
    static SwapchainSupport pickSwapchainSupport(VkPhysicalDevice pPhysicalDevice,
                                                 VkSurfaceKHR     pSurface);
    void                    recreateSwapchain();

    // Borrowed, ownded by App (declared before the Renderer there, so they outlive it)
    const GraphicsContext& context;
    GLFWwindow*            window;
    const Mesh&            mesh;

    bool     framebufferResized = false;
    uint32_t currentFrame       = 0;

    // Rebuilt on resize
    SwapchainSupport           support;
    std::unique_ptr<Swapchain> swapchain;
    std::unique_ptr<SyncObjects>
        syncObjects; // render-complete semaphores follow the image count

    // Live for the whole program ( Nothing here depends on the window size )
    FrameDescriptors descriptors;
    Pipeline         pipeline;
    CommandBuffers   commandBuffers;
};
