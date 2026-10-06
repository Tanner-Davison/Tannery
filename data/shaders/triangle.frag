#version 450

// set 1 = per-material (MaterialDescriptors), binding 0 = the texture + its sampler
layout(set = 1, binding = 0) uniform sampler2D texSampler;

layout(location = 0) in vec3 fragColor; // still passed through; unused now that we sample
layout(location = 1) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

void main() {
  outColor = texture(texSampler, fragUV);
}
