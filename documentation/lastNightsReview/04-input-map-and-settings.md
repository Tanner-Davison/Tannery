# 4. `InputMap` (action mapping) and `Settings` (persistence)

Files: `core/include/InputMap.hpp`, `core/src/InputMap.cpp`,
`core/include/Settings.hpp`, `core/src/Settings.cpp`

These two are worth **understanding**, not rewriting: they are general C++ rather than Vulkan.

---

# Part A: `InputMap`

## Big picture

Game code should ask "is the player moving forward?", not "is W down?". An **action** is what the
player means; a **key** is one way to say it. `InputMap` is the only place that knows which keys
mean which actions.

```
CameraController:  "is Action::MoveForward down?"
                            |
InputMap:          Action::MoveForward -> {W}   (also e.g. MoveUp -> {E, Space})
                            |
GLFW:              glfwGetKey(window, GLFW_KEY_W)
```

Why it matters: rebinding, gamepads, accessibility and per-platform input all plug in at one
place, and `CameraController` never has to change.

## Steps

### Step 1: the actions

```cpp
enum class Action {
    MoveForward, MoveBackward, MoveLeft, MoveRight, MoveUp, MoveDown,
    SensitivityDown, SensitivityUp,
    Quit,
    Count // not an action: the number of actions (sizes the table)
};
```

`enum class` keeps the names scoped (`Action::Quit`). The final `Count` entry is a standard trick:
its integer value equals the number of real actions, so it can size a table.

### Step 2: the table

```cpp
class InputMap {
  public:
    InputMap();
    void bind(Action pAction, int pKey);   // add a key to the action
    void rebind(Action pAction, int pKey); // replace all of the action's keys with this one
    bool isDown(GLFWwindow* pWindow, Action pAction) const;

  private:
    static constexpr std::size_t ACTION_COUNT = static_cast<std::size_t>(Action::Count);
    static std::size_t           index(Action pAction) { return static_cast<std::size_t>(pAction); }

    std::array<std::vector<int>, ACTION_COUNT> keys;
};
```

An array indexed by action, where each slot is a **list** of keys (an action can have several).

**The compile error that was fixed here:** an earlier version declared `index()` as a
`static constexpr` member function and used it for the array size. Inside a class body, member
function **bodies** are only compiled after the whole class is seen, so the function can't be
called in a constant expression in the class's own declarations. The fix was a plain constant
(`ACTION_COUNT`) for the size and an ordinary static function for runtime indexing.

### Step 3: defaults, bind, rebind, query

```cpp
InputMap::InputMap() {
    bind(Action::MoveForward, GLFW_KEY_W);
    bind(Action::MoveBackward, GLFW_KEY_S);
    bind(Action::MoveLeft, GLFW_KEY_A);
    bind(Action::MoveRight, GLFW_KEY_D);
    bind(Action::MoveUp, GLFW_KEY_E);
    bind(Action::MoveUp, GLFW_KEY_SPACE);      // two keys, one action
    bind(Action::MoveDown, GLFW_KEY_Q);
    bind(Action::MoveDown, GLFW_KEY_LEFT_SHIFT);
    bind(Action::SensitivityDown, GLFW_KEY_LEFT_BRACKET);
    bind(Action::SensitivityUp, GLFW_KEY_RIGHT_BRACKET);
    bind(Action::Quit, GLFW_KEY_ESCAPE);
}

void InputMap::bind(Action pAction, int pKey)   { keys[index(pAction)].push_back(pKey); }
void InputMap::rebind(Action pAction, int pKey) { keys[index(pAction)].assign(1, pKey); }

bool InputMap::isDown(GLFWwindow* pWindow, Action pAction) const {
    for (const int key : keys[index(pAction)]) {
        if (glfwGetKey(pWindow, key) == GLFW_PRESS) {
            return true;      // any bound key counts
        }
    }
    return false;
}
```

`glfwGetKey` is **polling**: it asks "is it down right now?". A key tapped and released between
two frames is never seen, which is invisible for movement at 60+ fps.

## Gotchas (A)

- `rebind` exists in code but nothing calls it yet, and bindings are not saved in `Settings`.
- `Quit` is checked in `App::run`, not in the controller, because quitting is an application
  concern and not a camera one.

## Check yourself (A)

1. How does holding E and Space together behave, and why?
2. Why is `Action::Count` the last enum entry?
3. What is the difference between `bind` and `rebind`?

<details>
<summary>Answer key (A)</summary>

1. Both map to `MoveUp`, so `isDown(MoveUp)` is true; the controller adds `(0,1,0)` **once**
   (it asks per action, not per key), so there is no double speed.
2. Its integer value equals the number of real actions, so it sizes the table.
3. `bind` adds another key to the action's list; `rebind` throws away the old keys and leaves
   exactly one.

</details>

---

# Part B: `Settings`

## Big picture

Speed and sensitivity used to reset to defaults on every launch. `Settings` saves them to a small
text file on exit and reads them on startup.

```
startup:  Settings::load(defaultPath) -> cameraController.applySettings(...)
exit:     cameraController.currentSettings().save(defaultPath)
```

The file: `~/.config/forge3d/settings.cfg` (or `$XDG_CONFIG_HOME/forge3d/settings.cfg`):

```
# Forge3D settings
moveSpeed=3
mouseSensitivity=0.0018
```

Design rule: **a bad file must never break startup.** Anything missing, unknown or garbage just
keeps its default.

## Steps

### Step 1: the data

```cpp
struct Settings {
    float moveSpeed        = 3.0f;    // world units per second
    float mouseSensitivity = 0.0018f; // radians per raw mouse count
    ...
};
```

Defaults live in the struct; `parse` starts from a default `Settings` and only overwrites what it
can read.

### Step 2: where the file goes

```cpp
std::filesystem::path Settings::defaultPath() {
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME"); xdg != nullptr && *xdg != '\0') {
        return std::filesystem::path(xdg) / "forge3d" / "settings.cfg";
    }
    if (const char* home = std::getenv("HOME"); home != nullptr && *home != '\0') {
        return std::filesystem::path(home) / ".config" / "forge3d" / "settings.cfg";
    }
    return "settings.cfg"; // no home directory known: fall back to the working directory
}
```

The XDG convention: per-user config lives under `$XDG_CONFIG_HOME`, defaulting to `~/.config`.

### Step 3: parsing one number safely

```cpp
bool parseFloat(const std::string& pText, float& pOut) {
    if (pText.empty()) return false;
    char*       end   = nullptr;
    const float value = std::strtof(pText.c_str(), &end);
    if (end != pText.c_str() + pText.size() || !std::isfinite(value)) {
        return false;
    }
    pOut = value;
    return true;
}
```

- `end` points where parsing stopped; it must be at the **end** of the string, or there was junk
  (`"1.5x"` is rejected).
- `isfinite` rejects `nan` and `inf`, which `strtof` happily accepts.
- `pOut` is only written on success.

### Step 4: parsing the whole text

```cpp
while (std::getline(stream, line)) {
    line = trim(line);
    if (line.empty() || line.front() == '#') continue;      // blank or comment
    const auto equals = line.find('=');
    if (equals == std::string::npos) continue;               // no '=': not a setting
    const std::string key   = trim(line.substr(0, equals));
    const std::string value = trim(line.substr(equals + 1));

    if (key == "moveSpeed")              parseFloat(value, result.moveSpeed);
    else if (key == "mouseSensitivity")  parseFloat(value, result.mouseSensitivity);
    // unknown keys are ignored on purpose (forward/backward compatibility)
}
```

`trim` removes spaces, tabs and `\r` (so Windows line endings are fine). Ignoring unknown keys
means a newer file still loads in an older build.

### Step 5: writing it back

```cpp
std::string Settings::serialize() const {
    // {} on a float prints the shortest text that round-trips to the same value
    return std::format("# Forge3D settings\nmoveSpeed={}\nmouseSensitivity={}\n", moveSpeed, mouseSensitivity);
}

bool Settings::save(const std::filesystem::path& pPath) const {
    std::error_code ec;
    if (pPath.has_parent_path()) {
        std::filesystem::create_directories(pPath.parent_path(), ec);   // make the folder
        if (ec) return false;
    }
    std::ofstream file(pPath, std::ios::trunc);
    if (!file) return false;
    file << serialize();
    return static_cast<bool>(file);
}
```

`save` returns `false` instead of throwing: failing to save a preference must not crash an
otherwise-working engine. `load` mirrors it: a missing file returns defaults.

### Step 6: where it hooks into the app

```cpp
// App constructor
cameraController.applySettings(Settings::load(Settings::defaultPath()));
// end of App::run
cameraController.currentSettings().save(Settings::defaultPath());
```

`applySettings` clamps the values (file 3), so even a valid-but-absurd number is made safe.

## Gotchas (B)

- Settings are saved only on a clean exit (after the loop ends), not on a crash.
- Values are validated twice: `parseFloat` rejects non-numbers; `applySettings` clamps numbers.

## Check yourself (B)

1. The file contains `moveSpeed=abc` and `mouseSensitivity=0.005`. What do you get?
2. Why does `save` return `bool` instead of throwing?
3. Why does `parse` start from default values and not from empty ones?
4. What does `isfinite` protect against that `strtof` alone does not?

<details>
<summary>Answer key (B)</summary>

1. `moveSpeed` stays at its default (3), because `abc` fails to parse; `mouseSensitivity` is
   0.005. One bad line never invalidates the others.
2. Failing to save a preference should not crash the engine. The caller can decide to ignore it.
3. So anything missing or unparsable keeps a sensible default instead of becoming zero.
4. `strtof` accepts `nan` and `inf` as valid inputs; `isfinite` rejects them, so they never reach
   the camera.

</details>
