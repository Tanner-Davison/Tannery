#pragma once

#include <array>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

/*
 * Vertex.hpp describes how the pipeline reads vertices
 *
 * */
struct Vertex {
    glm::vec2 pos;
    glm::vec3 color;

    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription binding{};
        binding.binding   = 0;
        binding.stride    = sizeof(Vertex);
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return binding;
    }

    // Input attribute description is {location, binding, format,  offset}
    // R32G32_SFLOAT means 'two 32-bit floats'
    static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescription() {
        std::array<VkVertexInputAttributeDescription, 2> attrs{};
        attrs[0] = {0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, pos)};
        attrs[1] = {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color)};
        return attrs;
    }
};
