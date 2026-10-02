#include "Mesh.hpp"
#include <stdexcept>

namespace {
void requireNonEmpty(bool nonEmpty) {
    if (!nonEmpty) {
        throw std::runtime_error("Mesh needs at least one vertex and one index");
    }
}
} // namespace

Mesh::Mesh(const GraphicsContext&       context,
           const std::vector<Vertex>&   vertices,
           const std::vector<uint16_t>& indices)
    : vertexBuffer(nullptr)
    , indexBuffer(nullptr)
    , numIndices(static_cast<uint32_t>(indices.size())) {
    requireNonEmpty(!vertices.empty() && !indices.empty());

    vertexBuffer = context.createDeviceLocalBuffer(vertices.data(),
                                                   sizeof(Vertex) * vertices.size(),
                                                   VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    indexBuffer  = context.createDeviceLocalBuffer(indices.data(),
                                                  sizeof(uint16_t) * indices.size(),
                                                  VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
}

VkBuffer Mesh::vertexBufferHandle() const {
    return vertexBuffer->handle();
}

VkBuffer Mesh::indexBufferHandle() const {
    return indexBuffer->handle();
}

uint32_t Mesh::indexCount() const {
    return numIndices;
}
