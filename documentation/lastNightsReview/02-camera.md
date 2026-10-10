# 2. `Camera`: state and math

Files: `core/include/Camera.hpp`, `core/src/Camera.cpp`, `App::makeCamera` in `core/src/App.cpp`
Longer reference: `documentation/camera.md`

## Big picture

Vulkan has no camera object. A "camera" is the **view matrix** you already upload every frame
(`CameraUBO::view`). `Camera` is a pure function of three numbers:

```
position (vec3), yaw (radians), pitch (radians)  ->  viewMatrix() (mat4)
```

It knows nothing about GLFW or Vulkan. That is why it can be unit-tested with plain numbers.

```
                        Camera                              (App.cpp)
 input ... move()/rotate() -> state -> viewMatrix() -> ubo.view -> UBO -> vertex shader
```

The vertex shader line that uses it (`data/shaders/triangle.vert`):

```glsl
gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
```

## Steps

### Step 1: the class shape

```cpp
class Camera {
public:
    Camera(glm::vec3 pPosition, float pYaw, float pPitch);

    glm::mat4 viewMatrix() const;
    // pLocalDir is in camera space: x = right, y = up, z = forward. Moves pDistance along it.
    void move(glm::vec3 pLocalDir, float pDistance);
    // Adds to the angles (radians). Pitch is clamped so the camera can't flip over.
    void rotate(float pDeltaYaw, float pDeltaPitch);

    glm::vec3 forward() const;       // unit vector the camera looks along
    glm::vec3 getPosition() const;   // (and getYaw/getPitch: used by the tests)

private:
    glm::vec3 position = glm::vec3(0.0f);  // world space
    float yaw          = 0.0f;             // radians, left/right
    float pitch        = 0.0f;             // radians, up/down
};
```

Design decisions worth knowing:

- `move` and `rotate` are separate: `move` shifts `position`; `rotate` changes the angles.
- There is **no roll** (a first-person camera would tilt the horizon).
- `move` takes a **camera-space** direction, so the caller says "forward 1" without knowing which
  way the camera currently faces.
- Angles are in radians (`glm::radians`, `std::sin` and `std::cos` all use radians).

### Step 2: `forward()`, two angles into one direction

```cpp
glm::vec3 Camera::forward() const {
    return glm::vec3(std::cos(yaw) * std::cos(pitch),
                     std::sin(pitch),
                     std::sin(yaw) * std::cos(pitch));
}
```

Build it in two layers:

1. **Yaw alone** is a point on a circle laid flat on the ground (X-Z plane): `(cos(yaw), sin(yaw))`.
2. **Pitch** tips that arrow up or down. Its height is `sin(pitch)`; as it tilts, the flat part
   shrinks by `cos(pitch)`. So the flat part is scaled by `cos(pitch)`.

```
yaw = 0, pitch = 0   ->  (1, 0, 0)   +X
yaw = -90 deg        ->  (0, 0, -1)  -Z   (the app's start yaw: looks toward the origin)
pitch = 90 deg       ->  (0, 1, 0)   straight up
```

It is always **length 1**, because `cos^2 + sin^2 = 1`:

```
x^2 + y^2 + z^2 = cos^2(p) * (cos^2(y) + sin^2(y)) + sin^2(p) = cos^2(p) + sin^2(p) = 1
```

so it never needs `normalize`.

### Step 3: `move()`, camera-space direction to world space

```cpp
namespace {
const glm::vec3 WORLD_UP(0.0f, 1.0f, 0.0f);
}

void Camera::move(glm::vec3 pLocalDir, float pDistance) {
    const glm::vec3 fwd   = forward();
    const glm::vec3 right = glm::normalize(glm::cross(fwd, WORLD_UP));
    // Convert camera-space direction (x right, y up, z forward) to world space
    position += (right * pLocalDir.x + WORLD_UP * pLocalDir.y + fwd * pLocalDir.z) * pDistance;
}
```

The camera's three axes:

| Axis | Value | Why |
|---|---|---|
| forward | `forward()` | where we look |
| right | `cross(forward, WORLD_UP)` | the cross product is perpendicular to both inputs; the right-hand rule makes it point right |
| up | `WORLD_UP` (not the camera's true up) | so "up" is always straight up, even when looking down |

**Worked example.** yaw = -90 deg, pitch = 0:

```
forward = (0, 0, -1)
right   = cross((0,0,-1), (0,1,0)) = (1, 0, 0)
press W for one 60 fps frame at speed 3:  localDir = (0,0,1), distance = 3 * 0.0167 = 0.05
position += (0,0,-1) * 0.05      ->  z goes from 2.00 to 1.95
```

### Step 4: `rotate()` and the pitch clamp

```cpp
const float MAX_PITCH = glm::radians(89.0f);

void Camera::rotate(float pDeltaYaw, float pDeltaPitch) {
    yaw += pDeltaYaw;
    pitch = std::clamp(pitch + pDeltaPitch, -MAX_PITCH, MAX_PITCH);
}
```

**Why 89 and not 90:** at exactly +/-90 deg, `forward` is parallel to `WORLD_UP`, so
`cross(forward, WORLD_UP)` is `(0,0,0)` and `normalize` of a zero vector is NaN. That NaN would
then be added into `position` forever. 89 deg leaves a small but non-zero cross product.

Yaw has no clamp: `sin`/`cos` wrap on their own. (Its only weakness is float precision after a
very long spin; `fmod(yaw, 2*pi)` would fix that if it ever matters.)

### Step 5: `viewMatrix()`

```cpp
glm::mat4 Camera::viewMatrix() const {
    return glm::lookAt(position, position + forward(), WORLD_UP);
}
```

`lookAt(eye, center, up)` builds a basis and returns the **inverse** of the camera's transform
directly:

```
f = normalize(center - eye)      // forward
s = normalize(cross(f, up))      // right
u = cross(s, f)                  // true camera up

 [  s.x   s.y   s.z   -dot(s, eye) ]
 [  u.x   u.y   u.z   -dot(u, eye) ]
 [ -f.x  -f.y  -f.z    dot(f, eye) ]
 [   0     0     0         1       ]
```

Read it as two moves: shift the world by `-eye` (camera at the origin), then rotate the world so
the camera's `s`, `u`, `-f` become the X, Y, Z axes. In **view space the camera looks down -Z**,
which is why `f` is negated in the third row.

Moving the camera right by 1 is the same picture as moving the whole world left by 1.

### Step 6: projection and the Y flip (in `App`, not in `Camera`)

```cpp
// App.cpp, makeCamera()
ubo.model = glm::mat4(1.0f); // quads stay where they are; the camera does the moving
ubo.view  = camera.viewMatrix();
ubo.proj  = glm::perspective(glm::radians(45.0f), renderer.aspectRatio(), 0.1f, 100.0f);
ubo.proj[1][1] *= -1.0f;  // GLM assumes OpenGL (Y up in clip space); Vulkan's Y points down
```

- 45 deg is the **vertical** field of view; `aspectRatio` stops the picture stretching.
- `0.1` / `100` are the near and far planes (outside them, geometry is clipped).
- `GLM_FORCE_DEPTH_ZERO_TO_ONE` (set in `CMakeLists.txt`) makes depth 0..1 as Vulkan expects.
- `proj[1][1] *= -1` flips clip-space Y. Flipping an axis also reverses triangle winding; today
  that has no visible effect because `cullMode` is `NONE` (`Pipeline.cpp`), but it will matter
  the day back-face culling is turned on.

### The coordinate-spaces ladder

```
local (mesh) --model--> world --view--> view/camera space --proj--> clip --> screen
```

Every bug in this area so far was a coordinate-system bug (Y down on screen, Y flipped in clip
space, -Z forward in view space). When something looks mirrored or inverted, ask which space the
broken value is in first.

## Gotchas

- Never `normalize` a vector that can be zero (NaN).
- Radians versus degrees: `glm::radians(...)` at the boundary.
- The camera start yaw of `-90 deg` is set in `App.hpp`, not in `Camera`.

## Check yourself

1. With yaw = -90 deg and pitch = 0, what are `forward`, `right`, and where does the camera end
   up after `move((1, 0, 0), 3)`, starting at `(0, 0, 2)`?
2. Why is `forward()` always length 1?
3. Why does `move()` use `WORLD_UP` for the Y component instead of the camera's own up?
4. What breaks if pitch reaches exactly 90 deg, and where in the code does it first show up?
5. Why does `lookAt` negate `f` in its third row?
6. Why does `makeCamera()` multiply `proj[1][1]` by -1?

<details>
<summary>Answer key</summary>

1. `forward = (0, 0, -1)`, `right = (1, 0, 0)`. Strafing right by 3 from `(0, 0, 2)` ends at
   `(3, 0, 2)`.
2. Because `cos^2(y) + sin^2(y) = 1`, so the flat part has length `cos(p)`; then
   `cos^2(p) + sin^2(p) = 1`.
3. So "up" is always straight up in the world: pressing `E` rises vertically even when you look
   down at an angle. The camera's own up would tilt with the pitch.
4. `forward` becomes parallel to `WORLD_UP`, so `cross(forward, WORLD_UP)` is zero and
   `normalize` in `Camera::move` (the `right` line) returns NaN, which then poisons `position`.
   The clamp in `rotate()` prevents it.
5. In view space the camera looks down -Z, so the camera's forward direction is the **-Z** axis.
6. Vulkan's clip-space Y points down, while GLM's `perspective` assumes OpenGL where Y points up.

</details>

## Rewrite it yourself

Delete `Camera::forward()` and `Camera::move()` (keep a copy), then rewrite them from the circle
and axis explanations above. Run `./build/engine_tests`: `testCameraForward`, `testCameraMove`
and `testCameraUpIsWorldUp` check exactly these two functions.
