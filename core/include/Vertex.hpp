#pragma once

#include <array>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

/*
 * Vertex.hpp describes how the pipeline reads vertices
 *
 * */
struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 uv; // texture coordinate: (0,0) top-left of the image, (1,1) bottom-right

    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription binding{};
        binding.binding   = 0;
        binding.stride    = sizeof(Vertex);
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return binding;
    }

    // Input attribute description is {location, binding, format,  offset}
    // R32G32_SFLOAT means 'two 32-bit floats'
    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescription() {
        std::array<VkVertexInputAttributeDescription, 3> attrs{};
        attrs[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos)};
        attrs[1] = {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color)};
        attrs[2] = {2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv)};
        return attrs;
    }
};
