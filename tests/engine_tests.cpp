// Engine tests: only code with no GLFW/Vulkan dependency (Camera, FixedTimestep, Settings), so they build and run anywhere without a window or GPU.
//
// No test framework on purpose: a CHECK macro plus a failure counter is enough, and it keeps
// the dependency list (apt-only) unchanged. Exit code 0 = all passed, so CTest can run it.

#include "Camera.hpp"
#include "FixedTimestep.hpp"
#include "Settings.hpp"

#include <glm/glm.hpp>

#include <cmath>
#include <cstdio>
#include <string>

namespace {
int checksRun = 0;
int failures  = 0;

#define CHECK(cond)                                                                       \
    do {                                                                                  \
        ++checksRun;                                                                      \
        if (!(cond)) {                                                                    \
            ++failures;                                                                   \
            std::fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);          \
        }                                                                                 \
    } while (0)

bool nearly(float a, float b, float eps = 1e-4f) {
    return std::fabs(a - b) < eps;
}
bool nearly(const glm::vec3& a, const glm::vec3& b, float eps = 1e-4f) {
    return nearly(a.x, b.x, eps) && nearly(a.y, b.y, eps) && nearly(a.z, b.z, eps);
}

const float NEG_HALF_PI = glm::radians(-90.0f); // the app's start yaw: looks down -Z

// ---------------------------------------------------------------- Camera
void testCameraForward() {
    Camera cam(glm::vec3(0.0f), NEG_HALF_PI, 0.0f);
    CHECK(nearly(cam.forward(), glm::vec3(0.0f, 0.0f, -1.0f)));

    Camera yaw0(glm::vec3(0.0f), 0.0f, 0.0f); // yaw 0 looks along +X
    CHECK(nearly(yaw0.forward(), glm::vec3(1.0f, 0.0f, 0.0f)));

    Camera up(glm::vec3(0.0f), 0.0f, glm::radians(45.0f));
    CHECK(up.forward().y > 0.7f); // pitching up raises the look direction

    // forward() must be unit length at any angle (the spherical-coordinates identity)
    for (float yaw = -6.0f; yaw < 6.0f; yaw += 0.7f) {
        for (float pitch = -1.5f; pitch < 1.5f; pitch += 0.5f) {
            Camera c(glm::vec3(0.0f), yaw, pitch);
            CHECK(nearly(glm::length(c.forward()), 1.0f));
        }
    }
}

void testCameraMove() {
    Camera cam(glm::vec3(0.0f, 0.0f, 2.0f), NEG_HALF_PI, 0.0f);

    cam.move(glm::vec3(0.0f, 0.0f, 1.0f), 2.0f); // forward 2, looking down -Z
    CHECK(nearly(cam.getPosition(), glm::vec3(0.0f, 0.0f, 0.0f)));

    cam.move(glm::vec3(1.0f, 0.0f, 0.0f), 3.0f); // strafe right 3 => +X
    CHECK(nearly(cam.getPosition(), glm::vec3(3.0f, 0.0f, 0.0f)));

    cam.move(glm::vec3(-1.0f, 0.0f, 0.0f), 3.0f); // and back
    CHECK(nearly(cam.getPosition(), glm::vec3(0.0f, 0.0f, 0.0f)));

    cam.move(glm::vec3(0.0f, 0.0f, 0.0f), 5.0f); // zero direction => no movement
    CHECK(nearly(cam.getPosition(), glm::vec3(0.0f)));
}

void testCameraUpIsWorldUp() {
    // "Up" must stay straight up even when looking steeply down
    Camera cam(glm::vec3(0.0f), NEG_HALF_PI, glm::radians(-60.0f));
    cam.move(glm::vec3(0.0f, 1.0f, 0.0f), 2.0f);
    CHECK(nearly(cam.getPosition(), glm::vec3(0.0f, 2.0f, 0.0f)));
}

void testCameraPitchClamp() {
    Camera cam(glm::vec3(0.0f), NEG_HALF_PI, 0.0f);
    cam.rotate(0.0f, 10.0f); // way past straight up
    CHECK(cam.getPitch() <= glm::radians(89.0f) + 1e-5f);
    CHECK(cam.getPitch() > glm::radians(88.0f));
    cam.rotate(0.0f, -50.0f); // way past straight down
    CHECK(cam.getPitch() >= glm::radians(-89.0f) - 1e-5f);

    // At the clamp, moving must still produce finite numbers (no NaN from a zero cross product)
    cam.rotate(0.0f, 100.0f);
    cam.move(glm::vec3(1.0f, 0.0f, 1.0f), 1.0f);
    const glm::vec3 p = cam.getPosition();
    CHECK(std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z));
}

void testCameraRotateAccumulates() {
    Camera cam(glm::vec3(0.0f), 0.0f, 0.0f);
    cam.rotate(0.25f, 0.0f);
    cam.rotate(0.25f, 0.0f);
    CHECK(nearly(cam.getYaw(), 0.5f));
}

void testCameraViewMatrix() {
    Camera cam(glm::vec3(1.0f, 2.0f, 3.0f), NEG_HALF_PI, 0.0f);
    const glm::mat4 view = cam.viewMatrix();

    // The camera's own position becomes the origin of view space...
    CHECK(nearly(glm::vec3(view * glm::vec4(cam.getPosition(), 1.0f)), glm::vec3(0.0f)));
    // ...and a point straight ahead lands on -Z (view space looks down -Z)
    const glm::vec3 ahead = cam.getPosition() + cam.forward();
    CHECK(nearly(glm::vec3(view * glm::vec4(ahead, 1.0f)), glm::vec3(0.0f, 0.0f, -1.0f)));
}

// ---------------------------------------------------------------- FixedTimestep
void testFixedTimestep() {
    FixedTimestep clock(0.01f);

    CHECK(clock.advance(0.004f) == 0); // not enough for a tick yet
    CHECK(clock.advance(0.0071f) == 1); // 0.0111 total: one tick, remainder carried
    CHECK(clock.advance(0.0301f) == 3); // carried remainder + this frame
    CHECK(clock.alpha() >= 0.0f && clock.alpha() < 1.0f);

    // A huge frame runs at most maxSteps ticks and drops the backlog (no spiral of death)
    FixedTimestep capped(0.01f, 5);
    CHECK(capped.advance(10.0f) == 5);
    CHECK(capped.advance(0.0f) == 0); // nothing left over to chase
    CHECK(capped.alpha() < 1.0f);

    // Same total time, split differently, gives the same number of ticks
    FixedTimestep a(0.01f);
    FixedTimestep b(0.01f);
    int           ticksA = a.advance(0.0505f);
    int           ticksB = 0;
    for (int i = 0; i < 101; ++i) {
        ticksB += b.advance(0.0005f);
    }
    CHECK(ticksA == 5);
    CHECK(ticksB == 5);
}

// ---------------------------------------------------------------- Settings
void testSettingsRoundTrip() {
    Settings original;
    original.moveSpeed        = 7.5f;
    original.mouseSensitivity = 0.0031f;

    const Settings loaded = Settings::parse(original.serialize());
    CHECK(loaded.moveSpeed == original.moveSpeed);
    CHECK(loaded.mouseSensitivity == original.mouseSensitivity);
}

void testSettingsRobustParsing() {
    const Settings defaults;

    // Empty text, junk, comments and unknown keys all leave the defaults alone
    CHECK(Settings::parse("").moveSpeed == defaults.moveSpeed);
    CHECK(Settings::parse("garbage without equals").moveSpeed == defaults.moveSpeed);
    CHECK(Settings::parse("# moveSpeed=99\n").moveSpeed == defaults.moveSpeed);
    CHECK(Settings::parse("unknownKey=5\n").moveSpeed == defaults.moveSpeed);

    // Bad values are rejected; the other key still applies
    const Settings mixed = Settings::parse("moveSpeed=abc\nmouseSensitivity=0.005\n");
    CHECK(mixed.moveSpeed == defaults.moveSpeed);
    CHECK(nearly(mixed.mouseSensitivity, 0.005f, 1e-7f));

    CHECK(Settings::parse("moveSpeed=nan\n").moveSpeed == defaults.moveSpeed);
    CHECK(Settings::parse("moveSpeed=inf\n").moveSpeed == defaults.moveSpeed);
    CHECK(Settings::parse("moveSpeed=1.5x\n").moveSpeed == defaults.moveSpeed);

    // Whitespace and Windows line endings are tolerated
    const Settings spaced = Settings::parse("  moveSpeed = 4.25 \r\n");
    CHECK(nearly(spaced.moveSpeed, 4.25f));
}

void testSettingsFile() {
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "forge3d_test";
    const std::filesystem::path file = dir / "nested" / "settings.cfg";
    std::filesystem::remove_all(dir);

    CHECK(Settings::load(file).moveSpeed == Settings{}.moveSpeed); // missing file => defaults

    Settings s;
    s.moveSpeed = 12.0f;
    CHECK(s.save(file)); // creates the nested folders
    CHECK(Settings::load(file).moveSpeed == 12.0f);

    std::filesystem::remove_all(dir);
}
} // namespace

int main() {
    testCameraForward();
    testCameraMove();
    testCameraUpIsWorldUp();
    testCameraPitchClamp();
    testCameraRotateAccumulates();
    testCameraViewMatrix();
    testFixedTimestep();
    testSettingsRoundTrip();
    testSettingsRobustParsing();
    testSettingsFile();

    std::printf("%d checks, %d failed\n", checksRun, failures);
    return failures == 0 ? 0 : 1;
}
