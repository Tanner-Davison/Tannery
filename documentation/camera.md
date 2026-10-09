# Camera and Input

A free-fly first-person camera. Vulkan has no camera object: a "camera" is just the **view
matrix** we upload every frame (`CameraUBO::view`), produced from three numbers (position, yaw,
pitch) that input edits.

## Controls

| Input | Action |
|---|---|
| `W` `A` `S` `D` | Move forward / left / backward / right (camera-relative) |
| `E` or `Space` | Move up (world up) |
| `Q` or `Left Shift` | Move down |
| Mouse | Look (yaw and pitch) |
| Scroll wheel | Scale move speed (x1.15 per notch, 0.1 to 100) |
| `[` / `]` (hold) | Lower / raise mouse sensitivity |
| Left click | Capture the cursor (after alt-tab) |
| Alt-tab / focus lost | Releases the cursor |
| `Esc` | Quit |

Speed and sensitivity are saved on exit to `~/.config/forge3d/settings.cfg`
(`$XDG_CONFIG_HOME/forge3d/` if set) and restored on the next launch. The file is plain
`key=value` text and safe to hand-edit; bad lines are ignored.

## Architecture

```
App (App.cpp)                    wiring: owns everything, runs the loop, forwards GLFW events
 ├─ Camera            Camera.cpp           state + math. Knows nothing about GLFW or Vulkan.
 ├─ CameraController  CameraController.cpp policy: actions + mouse -> Camera calls,
 │                                         speed, sensitivity, cursor capture state
 ├─ InputMap          InputMap.cpp         action -> physical keys
 ├─ FixedTimestep     FixedTimestep.hpp    fixed-rate simulation clock (header only)
 └─ Settings          Settings.cpp         persisted speed/sensitivity
```

| File | Responsibility |
|---|---|
| `core/include/Camera.hpp`, `core/src/Camera.cpp` | `position`, `yaw`, `pitch`; `viewMatrix()`, `move()`, `rotate()`, `forward()` |
| `core/include/CameraController.hpp`, `core/src/CameraController.cpp` | Sums movement actions into one direction, applies mouse deltas, owns tunables and capture state |
| `core/include/InputMap.hpp`, `core/src/InputMap.cpp` | `enum class Action` and its key bindings; `isDown(window, action)`, `bind`, `rebind` |
| `core/include/Settings.hpp`, `core/src/Settings.cpp` | `parse` / `serialize` / `load` / `save` |
| `core/include/FixedTimestep.hpp` | `advance(frameDt)` returns how many fixed ticks to run |
| `core/src/App.cpp` | Frame loop, GLFW callbacks, `makeCamera()` builds the UBO |

Layering is the point: `Camera` is testable with plain numbers; `CameraController` never names a
physical key (that is `InputMap`'s job); neither touches Vulkan.

## One frame (`App::run`)

```
glfwPollEvents()                     callbacks fire here (focus, click, scroll), between frames
dt = min(now - last, MAX_DT)         MAX_DT = 0.1 s: a long hitch can't fling the camera
Quit action? close window
fixedClock.advance(dt)  -> N x fixedUpdate(1/60)     empty hook; physics will go here
cameraController.update(window, camera, input, dt)   once per rendered frame, not per fixed tick
renderer.drawFrame(makeCamera())     camera.viewMatrix() -> CameraUBO -> GPU
```

All callbacks run inside `glfwPollEvents()` on the same thread, so `update()` never runs
concurrently with one: no locking needed.

The camera is deliberately **not** in the fixed step. It is sampled once per rendered frame so it
stays smooth at any refresh rate; fixed ticks are for physics and gameplay.

## The math

**`forward()`** (spherical coordinates, always unit length because `cos^2 + sin^2 = 1`):

```
(cos(yaw) * cos(pitch),  sin(pitch),  sin(yaw) * cos(pitch))
```

`yaw = 0` looks along +X, so the start yaw of -90 degrees (`App.hpp`) looks down -Z.

**`move(localDir, distance)`** converts a camera-space direction (x right, y up, z forward) to
world space: `right * x + WORLD_UP * y + forward * z`, with `right = cross(forward, WORLD_UP)`.
"Up" is world up, so `E` always goes straight up even when looking down.

**Pitch clamp**: +/-89 degrees. At exactly 90 `forward` is parallel to `WORLD_UP`, the cross
product is zero, and `normalize` returns NaN.

**`viewMatrix()`**: `glm::lookAt(position, position + forward(), WORLD_UP)` shifts the world by
`-eye` and rotates it so the camera's axes become X/Y/Z. In view space the camera looks down -Z.

**Projection** (`App::makeCamera`): `perspective(45 deg, aspect, 0.1, 100)` with
`proj[1][1] *= -1` because Vulkan's clip-space Y points down (GLM assumes OpenGL).
`GLM_FORCE_DEPTH_ZERO_TO_ONE` (CMake) gives Vulkan's 0..1 depth. `model` is identity: the camera
does all the moving.

## Input details worth remembering

- **Normalized direction.** Summed keys are normalized (`W` + `D` would otherwise be 41% faster).
  The `length > 0` check avoids `normalize(0)`, which is NaN.
- **dt scales movement but not the mouse.** Movement is a speed (units per second); a mouse
  count is a displacement (radians per count), so it must not depend on frame rate.
- **Raw mouse motion.** `GLFW_RAW_MOUSE_MOTION` bypasses OS pointer acceleration, so sensitivity
  scales directly with mouse DPI. A full turn takes `2*pi / sensitivity` counts. If it feels
  fast, lower sensitivity (`[`) instead of lowering DPI.
- **`dy` is flipped** (`lastY - y`): screen Y grows downward, but mouse-up should look up.
- **Exponential tuning.** Scroll and `[` `]` multiply by a factor, so each step is a percentage
  change and feels even at any value.

## Cursor capture state machine (Wayland lesson)

```
              left click                    focus lost
 RELEASED ───────────────► CAPTURED ───────────────► RELEASED
 (cursor free,   capture()  (hidden, locked,  release()
  update() no-op)            raw deltas)
```

On Wayland (GNOME) the compositor decides when the pointer lock takes effect, and it only
re-locks on a **click** after alt-tab. The reported cursor position can shift for a frame or two
around that moment, which showed up as a camera jump. Fixes, in the order they were learned:

1. Resetting a "first mouse" flag on focus regain was too early (the lock had not happened yet).
2. The skip window must start at **our own `capture()` call**, not at an event we only observe.
   `capture()` sets `framesToSkip = 3`.
3. While skipping, `update()` still refreshes `lastMouseX/Y` every frame, so the first accepted
   delta is relative to a fresh baseline instead of a stale position.
4. Alt-tab releases the cursor and a click recaptures it, like most games.

General lesson: when an external system acts on its own schedule, hang your guard on **your**
call to it, not on a nearby event.

## Tests

`tests/engine_tests.cpp` covers the code with no GLFW/Vulkan dependency: `Camera` (forward
vectors, movement, "up is world up", pitch clamp with no NaN, view matrix maps the eye to the
origin and ahead to -Z), `FixedTimestep` (tick counts, the step cap, split-frame equivalence) and
`Settings` (round trip, junk/NaN/inf rejection, whitespace, file save and load).

```bash
cmake --build build --target engine_tests && ./build/engine_tests    # "N checks, 0 failed"
```

No framework on purpose (a `CHECK` macro and a failure counter keeps the dependency list
apt-only). Exit code 0 means all passed, so `ctest -R engine_tests` also works.

## Known rough edges

| Issue | Where | Fix when it matters |
|---|---|---|
| Loop keeps rendering at full speed while unfocused | `App::run` | `glfwWaitEvents()` when unfocused |
| No on-screen readout of speed/sensitivity | `CameraController` | Needs a text/UI renderer |
| Yaw grows without bound | `Camera::rotate` | `fmod(yaw, 2*pi)` after very long spins |
| Keys polled, not event-driven | `InputMap::isDown` | A tap shorter than one frame is missed (invisible for movement) |
| Euler angles, not quaternions | `Camera` | Needed for camera blending / cutscenes |
| Key rebinding exists in code (`rebind`) but has no UI or config | `InputMap` | Persist bindings in `Settings` |

## References

- LearnOpenGL, [Camera](https://learnopengl.com/Getting-started/Camera): same yaw/pitch derivation
- GLFW, [Input guide](https://www.glfw.org/docs/latest/input_guide.html): keyboard, cursor
  modes, raw mouse motion
- Glenn Fiedler, [Fix Your Timestep](https://gafferongames.com/post/fix_your_timestep/)
