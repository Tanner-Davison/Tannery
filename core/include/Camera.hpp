#pragma once
#include <glm/glm.hpp>

// First-person fly camera. Pure state + math: knows nothing about GLFW or Vulkan.
// Angles are in radians. yaw = 0 looks along +X, so -pi/2 looks down -Z.
class Camera {
public:
    Camera(glm::vec3 pPosition, float pYaw, float pPitch);

    glm::mat4 viewMatrix() const;
    // pLocalDir is in camera space: x = right, y = up, z = forward. Moves pDistance along it.
    void move(glm::vec3 pLocalDir, float pDistance);
    // Adds to the angles (radians). Pitch is clamped so the camera can't flip over.
    void rotate(float pDeltaYaw, float pDeltaPitch);

private:
    glm::vec3 position = glm::vec3(0.0f);  // world space
    float yaw          = 0.0f;             // radians, left/right
    float pitch        = 0.0f;             // radians, up/down

    // Unit vector the camera looks along, from yaw and pitch
    glm::vec3 forward() const;
};
