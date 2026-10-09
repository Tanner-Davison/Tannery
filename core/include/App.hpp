#pragma once
#include "Camera.hpp"
#include "GraphicsContext.hpp"
#include "MaterialDescriptors.hpp"
#include "Mesh.hpp"
#include "Renderer.hpp"
#include "Sampler.hpp"
#include "Texture.hpp"
#include "UniformData.hpp"
#include "Window.hpp"

class App {
public:
    App(int width, int height, const char* title);
    ~App() = default;

    void run();

    App(const App&)            = delete;
    App& operator=(const App&) = delete;
    App(App&&)                 = delete;
    App& operator=(App&&)      = delete;

private:
    CameraUBO makeCamera() const;
    // Reads keyboard/mouse (polled, once per frame) and moves the camera. dt = seconds.
    void updateCamera(float dt);
    // Destroyed in reverse: renderer, material, sampler, texture, mesh, context, window
    // (the material's descriptor set points at the texture + sampler, so it must die first)
    Window window;
    GraphicsContext context;
    Mesh mesh;
    Texture texture;
    Sampler sampler;
    MaterialDescriptors material;
    Renderer renderer;

    // yaw -90 deg looks down -Z, toward the origin
    Camera camera{glm::vec3(0.0f, 0.0f, 2.0f), glm::radians(-90.0f), 0.0f};

    static constexpr float MOVE_SPEED = 3.0f;  // world units per second
    // Radians per mouse count. With raw motion this scales with your mouse DPI:
    // a full turn takes 2*pi / sensitivity counts. Lower = slower, higher = faster.
    static constexpr float MOUSE_SENSITIVITY = 0.0018f;

    double lastFrameTime = 0.0;
    double lastMouseX    = 0.0;
    double lastMouseY    = 0.0;
    bool firstMouse      = true;  // skip the first delta, or the view jumps on startup
};
