#version 450

layout(binding = 0) uniform CameraUBO {
  mat4 model;
  mat4 view;
  mat4 proj;
} ubo;
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inUV;
// out
layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragUV;

void main() {
  // right to left model= local to world, view= world to camera, proj= camera to clip
  gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 0.5);
  fragColor = inColor;
  fragUV = inUV;
}
