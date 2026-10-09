#pragma once
#include "Camera.hpp"
#include "CameraController.hpp"
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

    CameraController cameraController; // keys + mouse -> camera (bindings, speed, sensitivity)

    static constexpr float MAX_DT = 0.1f; // seconds; longest frame the camera will react to

    double lastFrameTime = 0.0;
};
