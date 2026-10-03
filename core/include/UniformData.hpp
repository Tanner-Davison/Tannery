#pragma once
#include <glm/glm.hpp>

// Must match the 'CameraUBO' block in triangle.vert (std140: three mat4s, 16-byte aligned)
struct CameraUBO {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

// 192 bytes (glm::mat4 = 16 floats and 1 float is 4 bytes 16 x 4 = 64; 64x3 = 192)
