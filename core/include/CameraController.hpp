#pragma once
#include "Camera.hpp"
#include "InputMap.hpp"
#include "Settings.hpp"

#include <GLFW/glfw3.h>

// Turns player actions (from InputMap) and the mouse into Camera::move / Camera::rotate calls.
// Owns the tunable speed/sensitivity and whether the cursor is captured; it never mentions a
// physical key (that's InputMap's job) and Camera itself stays input-agnostic.
class CameraController {
  public:
    // Hide + lock the cursor for mouse look (raw motion if the system supports it)
    void capture(GLFWwindow* pWindow);
    // Give the cursor back (visible, free to leave the window)
    void release(GLFWwindow* pWindow);

    // Call once per frame. Does nothing while the cursor is released.
    // pDt = seconds since the last frame.
    void update(GLFWwindow* pWindow, Camera& pCamera, const InputMap& pInput, float pDt);

    // Persisted tunables. apply clamps to the allowed range, so a bad file can't break the camera.
    void     applySettings(const Settings& pSettings);
    Settings currentSettings() const;

    // GLFW event hooks (App forwards its callbacks here)
    void onFocusChanged(GLFWwindow* pWindow, bool pFocused);
    void onMouseButton(GLFWwindow* pWindow, int pButton, int pAction);
    void onScroll(double pYOffset); // scroll wheel scales move speed

  private:
    // Tunables, adjustable at runtime
    float moveSpeed   = 3.0f;    // world units per second (scroll wheel)
    float sensitivity = 0.0018f; // radians per raw mouse count; scales with mouse DPI

    // Mouse state. The cursor is captured, so its position is virtual and unbounded; we only
    // ever use the difference between frames.
    double lastMouseX = 0.0;
    double lastMouseY = 0.0;
    bool   captured   = false;

    // When the compositor (re-)locks the pointer, the reported position can shift for a frame
    // or two (seen on Wayland). Ignore mouse deltas for this many frames after capturing.
    static constexpr int CAPTURE_SKIP_FRAMES = 3;
    int                  framesToSkip        = 0;
};
