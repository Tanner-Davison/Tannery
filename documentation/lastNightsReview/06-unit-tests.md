# 6. Unit tests

Files: `tests/engine_tests.cpp`, the test block at the end of `CMakeLists.txt`

## Big picture

A unit test runs a small piece of code with known inputs and checks the answer. These tests
cover only code with **no GLFW and no Vulkan**: `Camera`, `FixedTimestep`, `Settings`. They need
no window and no GPU, so they run anywhere in a fraction of a second.

```
tests/engine_tests.cpp  --links-->  Camera.cpp, Settings.cpp   (+ header-only FixedTimestep)
        |
        +--> prints "N checks, M failed", exit code 0 if all passed
```

This was possible because those classes were designed to **take their inputs as arguments** (frame
time, text, numbers) and never read a clock, a file or a window by themselves. (`Settings::load`
does read a file, which is why its file test uses a temp folder.)

## Steps

### Step 1: the harness (no framework on purpose)

```cpp
namespace {
int checksRun = 0;
int failures  = 0;

#define CHECK(cond)                                                              \
    do {                                                                         \
        ++checksRun;                                                             \
        if (!(cond)) {                                                           \
            ++failures;                                                          \
            std::fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
        }                                                                        \
    } while (0)
```

- `CHECK` counts every check and, on failure, prints the **file, line and the expression text**
  (`#cond` turns the argument into a string).
- `do { ... } while (0)` makes the macro behave like one statement (safe inside `if` without
  braces).
- A test failing does **not** stop the run: all failures are reported.
- No test framework keeps the dependency list apt-only. Catch2 or doctest could replace this
  later; the test bodies would barely change.

### Step 2: comparing floats

```cpp
bool nearly(float a, float b, float eps = 1e-4f) {
    return std::fabs(a - b) < eps;
}
bool nearly(const glm::vec3& a, const glm::vec3& b, float eps = 1e-4f) {
    return nearly(a.x, b.x, eps) && nearly(a.y, b.y, eps) && nearly(a.z, b.z, eps);
}
```

Never compare floats with `==` after arithmetic (`cos(pi/2)` is not exactly 0). Compare within a
small tolerance instead. (The one place `==` is used: `Settings` round trip, where the text is
written and read back to the *same* shortest representation.)

### Step 3: a test in full (`testCameraMove`)

```cpp
void testCameraMove() {
    Camera cam(glm::vec3(0.0f, 0.0f, 2.0f), NEG_HALF_PI, 0.0f);   // looking down -Z

    cam.move(glm::vec3(0.0f, 0.0f, 1.0f), 2.0f);                    // forward 2
    CHECK(nearly(cam.getPosition(), glm::vec3(0.0f, 0.0f, 0.0f)));

    cam.move(glm::vec3(1.0f, 0.0f, 0.0f), 3.0f);                    // strafe right 3 => +X
    CHECK(nearly(cam.getPosition(), glm::vec3(3.0f, 0.0f, 0.0f)));

    cam.move(glm::vec3(-1.0f, 0.0f, 0.0f), 3.0f);                   // and back
    CHECK(nearly(cam.getPosition(), glm::vec3(0.0f, 0.0f, 0.0f)));

    cam.move(glm::vec3(0.0f, 0.0f, 0.0f), 5.0f);                    // zero direction => no movement
    CHECK(nearly(cam.getPosition(), glm::vec3(0.0f)));
}
```

The pattern is **arrange** (set up a camera), **act** (call `move`), **assert** (`CHECK`). The
numbers come from working the math out by hand first (file 2's worked example).

### Step 4: what is covered

| Test | What it proves |
|---|---|
| `testCameraForward` | yaw -90 looks down -Z; yaw 0 looks along +X; pitching up raises `y`; `forward()` is unit length at many angles |
| `testCameraMove` | forward / strafe / back / zero direction |
| `testCameraUpIsWorldUp` | "up" stays vertical even when looking steeply down |
| `testCameraPitchClamp` | pitch never passes +/-89 deg; no NaN after moving at the clamp |
| `testCameraRotateAccumulates` | two rotations add |
| `testCameraViewMatrix` | the eye maps to the origin; a point straight ahead maps to -Z |
| `testFixedTimestep` | tick counts, remainder carry, the cap, split-frame equivalence |
| `testSettingsRoundTrip` | serialize then parse gives the same values |
| `testSettingsRobustParsing` | junk, comments, unknown keys, `nan`, `inf`, `1.5x`, whitespace, `\r` |
| `testSettingsFile` | missing file gives defaults; save creates folders; load reads it back |

### Step 5: the main function

```cpp
int main() {
    testCameraForward();
    ... (one call per test) ...
    std::printf("%d checks, %d failed\n", checksRun, failures);
    return failures == 0 ? 0 : 1;
}
```

The **exit code** is what CTest and CI use: `0` passes, anything else fails.

### Step 6: building and running

```cmake
# CMakeLists.txt
option(FORGE3D_BUILD_TESTS "Build the engine unit tests" ON)
if(FORGE3D_BUILD_TESTS)
    enable_testing()
    add_executable(engine_tests EXCLUDE_FROM_ALL
        tests/engine_tests.cpp core/src/Camera.cpp core/src/Settings.cpp)
    target_include_directories(engine_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/core/include)
    add_test(NAME engine_tests COMMAND engine_tests)
endif()
```

```bash
cmake --build build --target engine_tests && ./build/engine_tests
# prints "N checks, 0 failed"  (N is whatever the suite currently has)
ctest --test-dir build -R engine_tests     # same thing through CTest
```

- `EXCLUDE_FROM_ALL` means a normal build doesn't compile the tests; you ask for them by target.
- The tests compile `Camera.cpp` and `Settings.cpp` directly, **not** the whole engine, which
  is why they don't need GLFW or Vulkan.
- Use `-R engine_tests`: your project also registers Taskflow's own tests with CTest.

## How to add a test

1. Write `void testSomething() { ... CHECK(...); }` in the right section.
2. Add a call in `main()`.
3. Rebuild the `engine_tests` target and run it.
4. To see a test fail on purpose, change an expected value and read the `FAIL file:line` line.

## Gotchas

- A test is only as good as its expected values: work them out independently of the code.
- `testSettingsFile` writes to a temp directory (`forge3d_test`) and cleans up.
- These tests cannot catch bugs in anything that touches GLFW or Vulkan (the capture state
  machine, the barriers). Those are checked by running the app and by validation layers.

## Check yourself

1. Why does `CHECK` print `#cond`?
2. Why can the tests run without a window or a GPU?
3. Why is `nearly(...)` used instead of `==` for floats?
4. What exit code does a passing run return, and why does it matter?
5. Which kinds of bugs from last night could these tests **not** have caught?

<details>
<summary>Answer key</summary>

1. `#cond` stringizes the expression, so a failure shows the exact check that failed along with
   its file and line.
2. They only compile code with no GLFW/Vulkan dependency (`Camera`, `FixedTimestep`, `Settings`),
   and those classes take their inputs as arguments.
3. Float arithmetic is rarely exact (`cos(pi/2)` is not 0), so results are compared within a
   tolerance.
4. `0`. CTest and CI treat a non-zero exit as failure, so the exit code makes the tests automatable.
5. The Wayland cursor-capture jump, barrier ordering or layout mistakes in the mip loop, and any
   Vulkan validation error: all depend on GLFW, the compositor or the GPU.

</details>

## Exercise

Add a test `testCameraRotateYawWraps` that rotates yaw by `2 * pi` and checks that `forward()` is
(nearly) the same as before. Run it, then change the expected value to something wrong and read
the failure line.
