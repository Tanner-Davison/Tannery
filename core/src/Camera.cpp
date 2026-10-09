#include "Camera.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace {
const glm::vec3 WORLD_UP(0.0f, 1.0f, 0.0f);
// Stop just short of straight up/down, where forward and WORLD_UP would line up and the
// "right" vector (their cross product) would collapse to zero
const float MAX_PITCH = glm::radians(89.0f);
} // namespace

Camera::Camera(glm::vec3 pPosition, float pYaw, float pPitch)
    : position(pPosition)
    , yaw(pYaw)
    , pitch(pPitch) {}

glm::mat4 Camera::viewMatrix() const {
    return glm::lookAt(position, position + forward(), WORLD_UP);
}

void Camera::move(glm::vec3 pLocalDir, float pDistance) {
    const glm::vec3 fwd   = forward();
    const glm::vec3 right = glm::normalize(glm::cross(fwd, WORLD_UP));
    // Convert camera-space direction (x right, y up, z forward) to world space
    position += (right * pLocalDir.x + WORLD_UP * pLocalDir.y + fwd * pLocalDir.z) * pDistance;
}

void Camera::rotate(float pDeltaYaw, float pDeltaPitch) {
    yaw += pDeltaYaw;
    pitch = std::clamp(pitch + pDeltaPitch, -MAX_PITCH, MAX_PITCH);
}

glm::vec3 Camera::forward() const {
    return glm::vec3(std::cos(yaw) * std::cos(pitch),
                     std::sin(pitch),
                     std::sin(yaw) * std::cos(pitch));
}
