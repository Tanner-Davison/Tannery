#include "App.hpp"
#include <vector>

namespace {
// First indexed mesh: a quad. First triangle (0 -> 1 -> 2), second triangle (2 -> 3 -> 0)
const std::vector<Vertex> QUAD_VERTICES = {
    {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}}, // 0 top-left, red
    {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},  // 1 top-right, green
    {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},   // 2 bottom-right, blue
    {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}},  // 3 bottom-left, white
};
const std::vector<uint16_t> QUAD_INDICES = {0, 1, 2, 2, 3, 0};
} // namespace

App::App(int width, int height, const char* title)
    : window(width, height, title)
    , context(window, title)
    , mesh(context, QUAD_VERTICES, QUAD_INDICES)
    , renderer(context, window, mesh) {
    glfwSetWindowUserPointer(window.handle(), this);
    glfwSetFramebufferSizeCallback(window.handle(), [](GLFWwindow* w, int, int) {
        auto* app = static_cast<App*>(glfwGetWindowUserPointer(w));
        app->renderer.onFramebufferResized();
    });
}

void App::run() {
    while (!glfwWindowShouldClose(window.handle())) {
        glfwPollEvents();
        renderer.drawFrame();
    }
    // GPU must finish before any destructor frees what it is
    // using/home/tanner-davison/Pictures/Screenshots/Screenshot\ From\ 2026-10-01\ 20-55-54.png

    context.waitIdle();
}
