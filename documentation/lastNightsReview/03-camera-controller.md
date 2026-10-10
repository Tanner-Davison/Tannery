# 3. `CameraController`: input, mouse look, cursor capture

Files: `core/include/CameraController.hpp`, `core/src/CameraController.cpp`,
callbacks in the `App` constructor (`core/src/App.cpp`)
Longer reference: `documentation/camera.md`

## Big picture

`Camera` has no idea where input comes from. `CameraController` turns **player actions and the
mouse** into `Camera::move` / `Camera::rotate` calls, and owns the tunables and the cursor
state. It never mentions a physical key (that is `InputMap`, file 4).

```
 InputMap (actions)  --+
                       +--> CameraController::update(window, camera, input, dt) --> Camera
 GLFW mouse position --+          owns: moveSpeed, sensitivity, captured, framesToSkip
 GLFW events (focus, click, scroll) -> App callbacks -> onFocusChanged / onMouseButton / onScroll
```

All GLFW callbacks run inside `glfwPollEvents()` on the same thread, between frames, so `update()`
never runs at the same time as a callback: no locking is needed.

## Steps

### Step 1: movement actions into one direction

```cpp
// Which camera-space direction (x right, y up, z forward) each movement action asks for
struct MoveAction {
    Action    action;
    glm::vec3 localDir;
};
const std::array<MoveAction, 6> MOVE_ACTIONS = {{
    {Action::MoveForward,  {0.0f, 0.0f,  1.0f}},
    {Action::MoveBackward, {0.0f, 0.0f, -1.0f}},
    {Action::MoveRight,    {1.0f, 0.0f,  0.0f}},
    {Action::MoveLeft,     {-1.0f, 0.0f, 0.0f}},
    {Action::MoveUp,       {0.0f, 1.0f,  0.0f}},
    {Action::MoveDown,     {0.0f, -1.0f, 0.0f}},
}};

// in update():
glm::vec3 dir(0.0f);
for (const MoveAction& move : MOVE_ACTIONS) {
    if (pInput.isDown(pWindow, move.action)) {
        dir += move.localDir;
    }
}
```

Holding W and D gives `dir = (1, 0, 1)`. Opposite actions cancel because `dir` is a **sum**.

### Step 2: normalize, then move

```cpp
// Normalize so holding two keys (W + D) isn't faster than one. The length check also
// avoids normalizing a zero vector, which would produce NaN.
if (glm::length(dir) > 0.0f) {
    pCamera.move(glm::normalize(dir), moveSpeed * pDt);
}
```

- `(1, 0, 1)` has length sqrt(2), about 1.41, so diagonal movement would be 41% faster.
- `normalize((0,0,0))` is NaN. With no keys pressed, `dir` is zero, hence the guard.
- **`* pDt`** makes movement a speed (units per second): at 60 fps and at 240 fps the camera
  covers the same distance per second.

### Step 3: sensitivity tuning

```cpp
// Sensitivity actions tune it while held (exponential, so it feels even at any value)
if (pInput.isDown(pWindow, Action::SensitivityDown)) {
    sensitivity *= std::exp(-SENSITIVITY_RATE * pDt);
}
if (pInput.isDown(pWindow, Action::SensitivityUp)) {
    sensitivity *= std::exp(SENSITIVITY_RATE * pDt);
}
sensitivity = std::clamp(sensitivity, MIN_SENSITIVITY, MAX_SENSITIVITY);
```

Multiplying makes each step a **percentage** change, so it feels the same at 0.001 as at 0.01.
(An additive step would be huge at low values and invisible at high ones.) The scroll wheel does
the same for speed: `moveSpeed *= pow(1.15, notches)`.

### Step 4: mouse delta

```cpp
double mouseX = 0.0;
double mouseY = 0.0;
glfwGetCursorPos(pWindow, &mouseX, &mouseY);

const float dx = static_cast<float>(mouseX - lastMouseX);
const float dy = static_cast<float>(lastMouseY - mouseY); // screen Y grows downward; flip
// Always refresh the baseline, even on frames we ignore, so a stale position never
// turns into one big delta later
lastMouseX = mouseX;
lastMouseY = mouseY;
```

- While the cursor is captured, `glfwGetCursorPos` is a **virtual, unbounded** position, so we
  only ever use the difference between frames.
- `dy` is subtracted in the opposite order from `dx` because screen Y grows **down**: moving the
  mouse up makes `mouseY` smaller, but we want pitch to increase.
- The baseline is refreshed **every** frame, including ignored ones. That detail is the whole fix
  for the alt-tab jump (step 7).

### Step 5: apply the rotation (no `dt`)

```cpp
// No dt here: a mouse count is a displacement, not a speed
pCamera.rotate(dx * sensitivity, dy * sensitivity);
```

Keys are a **speed** (so they scale by `dt`); a mouse count is a **displacement** (100 counts turn
the camera by the same angle at any frame rate). 100 counts * 0.0018 rad = 0.18 rad, about 10.3
degrees.

### Step 6: raw motion and why DPI suddenly mattered

```cpp
void CameraController::capture(GLFWwindow* pWindow) {
    glfwSetInputMode(pWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    if (glfwRawMouseMotionSupported()) {
        glfwSetInputMode(pWindow, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    }
    captured     = true;
    framesToSkip = CAPTURE_SKIP_FRAMES;
}
```

`GLFW_CURSOR_DISABLED` hides and locks the cursor. `GLFW_RAW_MOUSE_MOTION` (only valid while
disabled) bypasses the OS acceleration curve and reports sensor counts directly. So sensitivity
scales with **mouse DPI**: a full turn takes `2*pi / sensitivity` counts. Too fast? Lower
sensitivity (`[`) instead of lowering the DPI.

### Step 7: the cursor capture state machine (the Wayland lesson)

```
              left click                    focus lost
 RELEASED ----------------> CAPTURED ----------------> RELEASED
 (cursor free,   capture()   (hidden, locked,  release()
  update() no-op)             raw deltas)
                                  ^
                  constructor ----+   App.cpp: cameraController.capture(window.handle())
```

```cpp
void CameraController::update(...) {
    if (!captured) {
        return; // cursor is free (e.g. after alt-tab): the window is not driving the camera
    }
    ...
    if (framesToSkip > 0) {
        --framesToSkip;
        return; // no rotation this frame
    }
    pCamera.rotate(...);
}

void CameraController::onFocusChanged(GLFWwindow* pWindow, bool pFocused) {
    if (!pFocused) {
        release(pWindow);
    }
    // On regaining focus we stay released: the user clicks back in to recapture
}

void CameraController::onMouseButton(GLFWwindow* pWindow, int pButton, int pAction) {
    if (!captured && pButton == GLFW_MOUSE_BUTTON_LEFT && pAction == GLFW_PRESS) {
        capture(pWindow);
    }
}
```

**What went wrong first, in order:**

1. On Wayland (GNOME) the compositor decides when the pointer lock really takes effect, and after
   alt-tab it only re-locks on a **click**. The reported position can shift for a frame or two
   around that moment, which looked like a camera jump.
2. First fix: reset a "first mouse" flag on focus regain. Too early, because the lock had not
   happened yet.
3. Second fix: start a skip window at the focus event. Still wrong, because the real change
   came at the **click**, after the window had already expired.
4. Final fix: make capture an explicit state. `capture()` itself starts the skip window
   (`framesToSkip = 3`), and `update()` keeps refreshing the baseline during it.

**General lesson:** when an external system acts on its own schedule, hang your guard on **your
own call** to it, not on a nearby event you only observe.

### Step 8: persisted tunables and the clamp

```cpp
void CameraController::applySettings(const Settings& pSettings) {
    moveSpeed   = std::clamp(pSettings.moveSpeed, MIN_MOVE_SPEED, MAX_MOVE_SPEED);
    sensitivity = std::clamp(pSettings.mouseSensitivity, MIN_SENSITIVITY, MAX_SENSITIVITY);
}
```

A hand-edited settings file can't make the camera unusable: values are clamped on the way in
(see file 4 for `Settings`).

### Step 9: the glue in `App`

```cpp
// App.cpp, constructor: GLFW callbacks are plain C function pointers, so they can't capture
// `this`. The window's "user pointer" is how a captureless lambda finds the App again.
glfwSetWindowUserPointer(window.handle(), this);
glfwSetWindowFocusCallback(window.handle(), [](GLFWwindow* w, int focused) {
    auto* app = static_cast<App*>(glfwGetWindowUserPointer(w));
    app->cameraController.onFocusChanged(w, focused == GLFW_TRUE);
});
```

(The mouse-button and scroll callbacks follow the same pattern.)

## Gotchas

- `update()` does nothing while released: no key or mouse input while the cursor is free.
- During the 3 skip frames, **keys still move the camera**; only the mouse rotation is skipped.
- The first captured frame has a large virtual-position jump: the baseline refresh plus the skip
  window together absorb it.

## Check yourself

1. Why is `dir` normalized, and what does the `length > 0` check protect against?
2. Why is movement scaled by `dt` but the mouse delta is not?
3. Why is `dy` computed as `lastMouseY - mouseY` and not the other way?
4. What would happen if `lastMouseX/Y` were refreshed only on frames that are *not* skipped?
5. Why does alt-tab release the cursor, and why does it not recapture on regaining focus?
6. Why can a lambda passed to `glfwSetWindowFocusCallback` not capture `this`?

<details>
<summary>Answer key</summary>

1. Without normalization W+D has length sqrt(2) and moves about 41% faster than W alone. The
   `length > 0` check stops `normalize` of the zero vector, which returns NaN and would corrupt
   `position`.
2. Movement is a speed (distance per second), so it must scale with the frame time. A mouse count
   is a displacement: the same hand motion should turn the camera the same angle at any frame rate.
3. Screen Y grows downward, so moving the mouse up makes `mouseY` smaller. We want that to
   increase pitch, hence `last - current`.
4. The baseline would be stale during the skip frames. The first accepted frame would compute its
   delta from a position several frames old, so one large jump would still get through.
5. Releasing frees the cursor like any normal window when you leave. On Wayland the compositor
   only re-locks the pointer on a click, so recapturing on a click (and starting the skip window
   then) avoids the jump.
6. GLFW callbacks are plain C function pointers; a capturing lambda is not convertible to one.
   `glfwSetWindowUserPointer` / `glfwGetWindowUserPointer` is the workaround.

</details>

## Rewrite it yourself

Rewrite `CameraController::update()` (steps 1 to 5) with the file closed. Keep the state members
and the `MOVE_ACTIONS` table. The check is the engine itself: W moves forward, the mouse looks the
right way, and alt-tab then click shows no jump.
