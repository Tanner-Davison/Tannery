#include "App.hpp"

#include <glm/gtc/matrix_transform.hpp>

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
    // Hide and capture the cursor so mouse movement is unlimited (mouse look)
    glfwSetInputMode(window.handle(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    // Raw device deltas: skips OS pointer acceleration, so the same hand motion always turns
    // the camera by the same angle. Only works while the cursor is disabled, and not on every
    // system, so ask first.
    if (glfwRawMouseMotionSupported()) {
        glfwSetInputMode(window.handle(), GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    }
    glfwSetWindowUserPointer(window.handle(), this);
    // Frame resize callback
    glfwSetFramebufferSizeCallback(window.handle(), [](GLFWwindow* w, int, int) {
        auto* app = static_cast<App*>(glfwGetWindowUserPointer(w));
        app->renderer.onFramebufferResized();
    });
}

void App::run() {
    lastFrameTime = glfwGetTime(); // so the first dt isn't the whole startup time
    while (!glfwWindowShouldClose(window.handle())) {
        glfwPollEvents();

        const double now = glfwGetTime();
        const float  dt  = static_cast<float>(now - lastFrameTime);
        lastFrameTime    = now;

        updateCamera(dt);
        renderer.drawFrame(makeCamera());
    }
    // GPU must finish before any destructor frees what it is

    context.waitIdle();
}

void App::updateCamera(float dt) {
    GLFWwindow* w = window.handle();

    if (glfwGetKey(w, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(w, GLFW_TRUE);
    }

    // Keys -> a camera-space direction (x right, y up, z forward)
    glm::vec3 dir(0.0f);
    if (glfwGetKey(w, GLFW_KEY_W) == GLFW_PRESS) dir.z += 1.0f;
    if (glfwGetKey(w, GLFW_KEY_S) == GLFW_PRESS) dir.z -= 1.0f;
    if (glfwGetKey(w, GLFW_KEY_D) == GLFW_PRESS) dir.x += 1.0f;
    if (glfwGetKey(w, GLFW_KEY_A) == GLFW_PRESS) dir.x -= 1.0f;
    if (glfwGetKey(w, GLFW_KEY_E) == GLFW_PRESS) dir.y += 1.0f;
    if (glfwGetKey(w, GLFW_KEY_Q) == GLFW_PRESS) dir.y -= 1.0f;
    if (glfwGetKey(w, GLFW_KEY_SPACE) == GLFW_PRESS) dir.y += 1.0f;
    if (glfwGetKey(w, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) dir.y -= 1.0f;

    // Normalize so holding two keys (W + D) isn't faster than one
    if (glm::length(dir) > 0.0f) {
        camera.move(glm::normalize(dir), MOVE_SPEED * dt);
    }

    // Mouse: how far the cursor moved since last frame
    double mouseX = 0.0;
    double mouseY = 0.0;
    glfwGetCursorPos(w, &mouseX, &mouseY);
    if (firstMouse) {
        lastMouseX = mouseX;
        lastMouseY = mouseY;
        firstMouse = false;
    }
    const float dx = static_cast<float>(mouseX - lastMouseX);
    const float dy = static_cast<float>(lastMouseY - mouseY); // screen Y grows downward; flip
    lastMouseX     = mouseX;
    lastMouseY     = mouseY;

    camera.rotate(dx * MOUSE_SENSITIVITY, dy * MOUSE_SENSITIVITY);
}

CameraUBO App::makeCamera() const {
    CameraUBO ubo{};
    ubo.model = glm::mat4(1.0f); // quads stay where they are; the camera does the moving
    ubo.view  = camera.viewMatrix();
    ubo.proj  = glm::perspective(glm::radians(45.0f), renderer.aspectRatio(), 0.1f, 100.0f);
    ubo.proj[1][1] *= -1.0f;  // GLM assumes OpenGl (Y up in clip space); Vulkans Y points down
    return ubo;
}
