#include "App.hpp"
#include <filesystem>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>

namespace {
// First indexed mesh: a quad. First triangle (0 -> 1 -> 2), second triangle (2 -> 3 -> 0)
const float               ZINDEX        = 0.0f;
const std::vector<Vertex> QUAD_VERTICES = {
    // {pos}, {color}, {uv}
    // World +Y is up on screen (proj flips Y), so the image's top row (v = 0) goes on y = +0.5
    {{-0.5f, -0.5f, ZINDEX}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}}, // 0 bottom-left
    {{0.5f, -0.5f, ZINDEX}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},  // 1 bottom-right
    {{0.5f, 0.5f, ZINDEX}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},   // 2 top-right
    {{-0.5f, 0.5f, ZINDEX}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},  // 3 top-left
    {{0, -0.5, -0.5}, {1, 1, 0}, {0, 1}},
    {{0, -0.5, 0.5}, {0, 1, 1}, {1, 1}},
    {{0, 0.5, 0.5}, {1, 0, 1}, {1, 0}},
    {{0, 0.5, -0.5}, {1, 0.5, 0}, {0, 0}},

};
const std::vector<uint16_t> QUAD_INDICES = {0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4};
} // namespace

App::App(int width, int height, const char* title)
    : window(width, height, title)
    , context(window, title)
    , mesh(context, QUAD_VERTICES, QUAD_INDICES)
    , texture(context, std::filesystem::path(TEXTURE_DIR) / "uv_checker.png")
    , sampler(context)
    , material(context, texture, sampler)
    , renderer(context, window, mesh, material) {
    glfwSetWindowUserPointer(window.handle(), this);
    // Frame resize callback
    glfwSetFramebufferSizeCallback(window.handle(), [](GLFWwindow* w, int, int) {
        auto* app = static_cast<App*>(glfwGetWindowUserPointer(w));
        app->renderer.onFramebufferResized();
    });
}

void App::run() {
    while (!glfwWindowShouldClose(window.handle())) {
        glfwPollEvents();
        renderer.drawFrame(makeCamera(static_cast<float>(glfwGetTime())));
    }
    // GPU must finish before any destructor frees what it is

    context.waitIdle();
}

CameraUBO App::makeCamera(float timeSeconds) const {
    CameraUBO ubo{};
    ubo.model = glm::rotate(glm::mat4(1.0f),
                            timeSeconds * glm::radians(90.0f),
                            glm::vec3(0.0f, 1.0f, 0.0f));
    ubo.view =
        glm::lookAt(glm::vec3(0.0f, 0.0f, 2.0f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    ubo.proj = glm::perspective(glm::radians(45.0f), renderer.aspectRatio(), 0.1f, 10.0f);
    ubo.proj[1][1] *= -1.0f; // GLM assumes OpenGl (Y up in clip space); Vulkans Y points down
    return ubo;
}
