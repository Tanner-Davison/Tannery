#include "CameraController.hpp"

#include <algorithm>
#include <cmath>

namespace {
constexpr float MIN_MOVE_SPEED   = 0.1f;
constexpr float MAX_MOVE_SPEED   = 100.0f;
constexpr float MIN_SENSITIVITY  = 0.0001f;
constexpr float MAX_SENSITIVITY  = 0.02f;
constexpr float SCROLL_STEP      = 1.15f; // each wheel notch multiplies speed by this
constexpr float SENSITIVITY_RATE = 1.0f;  // [ and ] scale sensitivity by e^(rate * dt) per second
} // namespace

CameraController::CameraController()
    : bindings{
          {GLFW_KEY_W, {0.0f, 0.0f, 1.0f}},
          {GLFW_KEY_S, {0.0f, 0.0f, -1.0f}},
          {GLFW_KEY_D, {1.0f, 0.0f, 0.0f}},
          {GLFW_KEY_A, {-1.0f, 0.0f, 0.0f}},
          {GLFW_KEY_E, {0.0f, 1.0f, 0.0f}},
          {GLFW_KEY_SPACE, {0.0f, 1.0f, 0.0f}},
          {GLFW_KEY_Q, {0.0f, -1.0f, 0.0f}},
          {GLFW_KEY_LEFT_SHIFT, {0.0f, -1.0f, 0.0f}},
      } {}

void CameraController::capture(GLFWwindow* pWindow) {
    glfwSetInputMode(pWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    // Raw device deltas: skips OS pointer acceleration, so the same hand motion always turns
    // the camera by the same angle. Only works while the cursor is disabled, and not on every
    // system, so ask first.
    if (glfwRawMouseMotionSupported()) {
        glfwSetInputMode(pWindow, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    }
    captured     = true;
    framesToSkip = CAPTURE_SKIP_FRAMES;
}

void CameraController::release(GLFWwindow* pWindow) {
    glfwSetInputMode(pWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    captured = false;
}

void CameraController::update(GLFWwindow* pWindow, Camera& pCamera, float pDt) {
    if (!captured) {
        return; // cursor is free (e.g. after alt-tab): the window is not driving the camera
    }

    // Keys -> one summed camera-space direction
    glm::vec3 dir(0.0f);
    for (const KeyBinding& binding : bindings) {
        if (glfwGetKey(pWindow, binding.key) == GLFW_PRESS) {
            dir += binding.localDir;
        }
    }
    // Normalize so holding two keys (W + D) isn't faster than one. The length check also
    // avoids normalizing a zero vector, which would produce NaN.
    if (glm::length(dir) > 0.0f) {
        pCamera.move(glm::normalize(dir), moveSpeed * pDt);
    }

    // [ and ] tune mouse sensitivity while held (exponential, so it feels even at any value)
    if (glfwGetKey(pWindow, GLFW_KEY_LEFT_BRACKET) == GLFW_PRESS) {
        sensitivity *= std::exp(-SENSITIVITY_RATE * pDt);
    }
    if (glfwGetKey(pWindow, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS) {
        sensitivity *= std::exp(SENSITIVITY_RATE * pDt);
    }
    sensitivity = std::clamp(sensitivity, MIN_SENSITIVITY, MAX_SENSITIVITY);

    // Mouse: how far the cursor moved since last frame
    double mouseX = 0.0;
    double mouseY = 0.0;
    glfwGetCursorPos(pWindow, &mouseX, &mouseY);

    const float dx = static_cast<float>(mouseX - lastMouseX);
    const float dy = static_cast<float>(lastMouseY - mouseY); // screen Y grows downward; flip
    // Always refresh the baseline, even on frames we ignore, so a stale position never
    // turns into one big delta later
    lastMouseX = mouseX;
    lastMouseY = mouseY;

    if (framesToSkip > 0) {
        --framesToSkip;
        return; // no rotation this frame
    }

    // No dt here: a mouse count is a displacement, not a speed
    pCamera.rotate(dx * sensitivity, dy * sensitivity);
}

void CameraController::onFocusChanged(GLFWwindow* pWindow, bool pFocused) {
    if (!pFocused) {
        release(pWindow); // free the cursor so alt-tab behaves like any other window
    }
    // On regaining focus we stay released: the user clicks back in to recapture
}

void CameraController::onMouseButton(GLFWwindow* pWindow, int pButton, int pAction) {
    if (!captured && pButton == GLFW_MOUSE_BUTTON_LEFT && pAction == GLFW_PRESS) {
        capture(pWindow);
    }
}

void CameraController::onScroll(double pYOffset) {
    moveSpeed *= std::pow(SCROLL_STEP, static_cast<float>(pYOffset));
    moveSpeed = std::clamp(moveSpeed, MIN_MOVE_SPEED, MAX_MOVE_SPEED);
}
