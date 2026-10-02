#pragma once
#include "Buffer.hpp"
#include "GraphicsContext.hpp"
#include "Vertex.hpp"
#include <cstdint>
#include <memory>
#include <vector>

class Mesh {
  public:
    Mesh(const GraphicsContext&       context,
         const std::vector<Vertex>&   vertices,
         const std::vector<uint16_t>& indices);

    Mesh(const Mesh&)            = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&&)                 = delete;
    Mesh& operator=(Mesh&&)      = delete;

    VkBuffer vertexBufferHandle() const;
    VkBuffer indexBufferHandle() const;
    uint32_t indexCount() const;

  private:
    std::unique_ptr<Buffer> vertexBuffer;
    std::unique_ptr<Buffer> indexBuffer;
    uint32_t                numIndices;
};
