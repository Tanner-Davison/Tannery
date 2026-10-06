#pragma once
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
    CameraUBO makeCamera(float timeSeconds) const;
    // Destroyed in reverse: renderer, material, sampler, texture, mesh, context, window
    // (the material's descriptor set points at the texture + sampler, so it must die first)
    Window              window;
    GraphicsContext     context;
    Mesh                mesh;
    Texture             texture;
    Sampler             sampler;
    MaterialDescriptors material;
    Renderer            renderer;
};
