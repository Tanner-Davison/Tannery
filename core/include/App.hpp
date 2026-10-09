#pragma once
#include "Camera.hpp"
#include "CameraController.hpp"
#include "FixedTimestep.hpp"
#include "GraphicsContext.hpp"
#include "InputMap.hpp"
#include "MaterialDescriptors.hpp"
#include "Mesh.hpp"
#include "Renderer.hpp"
#include "Sampler.hpp"
#include "Settings.hpp"
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
    // Runs at a fixed rate (FIXED_STEP) regardless of frame rate. Physics and gameplay go here;
    // empty until the engine has some. The camera is deliberately NOT here: it is sampled once
    // per rendered frame so it feels responsive at any refresh rate.
    void fixedUpdate(float step);
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

    InputMap         input;            // action -> key bindings
    CameraController cameraController; // actions + mouse -> camera (speed, sensitivity)

    static constexpr float MAX_DT     = 0.1f;        // seconds; longest frame we will react to
    static constexpr float FIXED_STEP = 1.0f / 60.0f; // seconds per fixed simulation tick

    FixedTimestep fixedClock{FIXED_STEP};
    double        lastFrameTime = 0.0;
};
