#include "App.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <filesystem>
#include <vector>

namespace {
// First indexed mesh: a quad. First triangle (0 -> 1 -> 2), second triangle (2 -> 3 -> 0)
const float ZINDEX                      = 0.0f;
const std::vector<Vertex> QUAD_VERTICES = {
    // {pos}, {color}, {uv}
    // World +Y is up on screen (proj flips Y), so the image's top row (v = 0) goes on y = +0.5
    {{-0.5f, -0.5f, ZINDEX}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}}, // 0 bottom-left
    {{0.5f, -0.5f, ZINDEX},  {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}}, // 1 bottom-right
    {{0.5f, 0.5f, ZINDEX},   {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}}, // 2 top-right
    {{-0.5f, 0.5f, ZINDEX},  {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}}, // 3 top-left
    {{0, -0.5, -0.5},        {1, 1, 0},          {0, 1}      },
    {{0, -0.5, 0.5},         {0, 1, 1},          {1, 1}      },
    {{0, 0.5, 0.5},          {1, 0, 1},          {1, 0}      },
    {{0, 0.5, -0.5},         {1, 0.5, 0},        {0, 0}      },
};
const std::vector<uint16_t> QUAD_INDICES = {0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4};
}  // namespace

App::App(int width, int height, const char* title)
    : window(width, height, title)
    , context(window, title)
    , mesh(context, QUAD_VERTICES, QUAD_INDICES)
    , texture(context, std::filesystem::path(TEXTURE_DIR) / "uv_checker.png")
    , sampler(context)
    , material(context, texture, sampler)
    , renderer(context, window, mesh, material) {
    // Hide and lock the cursor for mouse look (the controller owns capture/release)
    cameraController.capture(window.handle());
    glfwSetWindowUserPointer(window.handle(), this);
    // Frame resize callback
    glfwSetFramebufferSizeCallback(window.handle(), [](GLFWwindow* w, int, int) {
        auto* app = static_cast<App*>(glfwGetWindowUserPointer(w));
        app->renderer.onFramebufferResized();
    });
    // Input events go to the controller (the user pointer set above is how we find `this`)
    glfwSetWindowFocusCallback(window.handle(), [](GLFWwindow* w, int focused) {
        auto* app = static_cast<App*>(glfwGetWindowUserPointer(w));
        app->cameraController.onFocusChanged(w, focused == GLFW_TRUE);
    });
    glfwSetMouseButtonCallback(window.handle(), [](GLFWwindow* w, int button, int action, int) {
        auto* app = static_cast<App*>(glfwGetWindowUserPointer(w));
        app->cameraController.onMouseButton(w, button, action);
    });
    glfwSetScrollCallback(window.handle(), [](GLFWwindow* w, double, double yOffset) {
        auto* app = static_cast<App*>(glfwGetWindowUserPointer(w));
        app->cameraController.onScroll(yOffset);
    });
}

void App::run() {
    lastFrameTime = glfwGetTime(); // so the first dt isn't the whole startup time
    while (!glfwWindowShouldClose(window.handle())) {
        glfwPollEvents();

        const double now = glfwGetTime();
        // Clamp so one long hitch (window drag, debugger pause) can't fling the camera
        const float dt = std::min(static_cast<float>(now - lastFrameTime), MAX_DT);
        lastFrameTime  = now;

        if (glfwGetKey(window.handle(), GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window.handle(), GLFW_TRUE);
        }
        cameraController.update(window.handle(), camera, dt);
        renderer.drawFrame(makeCamera());
    }
    // GPU must finish before any destructor frees what it is

    context.waitIdle();
}

CameraUBO App::makeCamera() const {
    CameraUBO ubo{};
    ubo.model = glm::mat4(1.0f); // quads stay where they are; the camera does the moving
    ubo.view  = camera.viewMatrix();
    ubo.proj  = glm::perspective(glm::radians(45.0f), renderer.aspectRatio(), 0.1f, 100.0f);
    ubo.proj[1][1] *= -1.0f;  // GLM assumes OpenGl (Y up in clip space); Vulkans Y points down
    return ubo;
}
