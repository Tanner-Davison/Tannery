#pragma once
#include "GraphicsContext.hpp"
#include "Mesh.hpp"
#include "Renderer.hpp"
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
    CameraUBO makeCamera(float timeSeconds) const;
    // Destroyed in reverse: renderer, mesh, context, window
    Window          window;
    GraphicsContext context;
    Mesh            mesh;
    Renderer        renderer;
};
