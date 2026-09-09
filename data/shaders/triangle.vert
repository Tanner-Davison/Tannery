#version 450
/*
My first shader ever
The Z-Axis Depth
--------------------
    The Z-axis represents depth (how far or close an object is relative to the screen).
  In 2D graphics or a flat screen overlay, you don't need depth, so you set it to 0.0.In

  3D graphics, Z determines what objects are in front of or behind other objects.

The W Component: Homogeneous Coordinates
-------------------------------------------
  W = 1.0 (A Position): When you set W to 1.0, you are telling the GPU that this vector is a specific point in space.
  It can be moved around, rotated, and scaled.

  W = 0.0 (A Direction): If you set W to 0.0, you are telling the GPU this is a direction vector (like a light ray or a surface normal).
  It can be rotated, but it cannot be moved (translated), because directions have no origin.
  */

void main() {
  vec2 positions[3] = vec2[](
      vec2(-0.8, -0.2),
      vec2(-0.5, -0.5),
      vec2(-0.2, -0.2)
    );
  gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0); // z= 0.0 w=1.0
}
