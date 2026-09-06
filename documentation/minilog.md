# What Is "minilog" in Your Vulkan Book, and How to Wire It Into CMake

## TL;DR

"minilog" is a small MIT-licensed C++17 logging library written by Sergey Kosarevsky (GitHub handle `corporateshark`; the header reads "minilog v1.2.0 / MIT License / Copyright (c) 2021-2026 Sergey Kosarevsky"), and it is almost certainly the exact library your Vulkan book uses — because your file layout (`VulkanInstance.cpp`, `PhysicalDevice.cpp`, `QueueFamilies.cpp`, `Swapchain.cpp`, etc.) plus `source_group(TREE ...)` matches the style of Kosarevsky's material, and his *Vulkan 3D Graphics Rendering Cookbook* (2nd ed., Packt, Feb 14 2025; ISBN 9781803248110; by Sergey Kosarevsky, Alexey Medvedev, and Viktor Latypov) begins its Vulkan setup recipe by initializing Minilog.

- **What it does:** minilog gives you leveled, thread-safe, colored console + file (and optional HTML) logging via a tiny C-style API — `minilog::initialize()`, `minilog::log(level, fmt, ...)`, `minilog::deinitialize()` — plus convenience macros `LLOGP/LLOGD/LLOGL/LLOGW/LLOGE` and the book's `MINILOG_LOG_PROC` wrapper. It replaces scattered `printf`/`std::cout` calls with a single, uniform logging facility.
- **How to add it:** minilog is two source files (`minilog.h` + `minilog.cpp`) that build into a tiny library target. Drop it under `deps/`/`third-party/` and either `add_subdirectory(deps/minilog)` + `target_link_libraries(YourApp PRIVATE minilog)`, or use `FetchContent` to clone `https://github.com/corporateshark/minilog` at configure time. Then `#include "minilog/minilog.h"`.

## Key Findings

### 1. Which "minilog" is relevant

There are several unrelated projects called "minilog"/"minilogger" on GitHub (e.g., `archibate/minilog` for C++20 teaching, `ysbbswork/Minilogger`, `dominikschnitzer/minilog`, `RDCH106/miniLogger`, and Esoteric Software's Java `minlog`). **The one used by Vulkan/graphics-programming books is `corporateshark/minilog`** — Sergey Kosarevsky's "Minimalistic logging library with threads and manual callstacks." Kosarevsky is the lead author of Packt's graphics cookbooks, so his own logging library is what those books use.

### 2. It matches your book

Your project structure — a `main.cpp`/`App.cpp` driver, one translation unit per Vulkan concept (`VulkanInstance`, `Window`, `PhysicalDevice`, `QueueFamilies`, `LogicalDevice`, `Surface`, `SwapchainSupport`, `Swapchain`) organized into IDE folders with `source_group(TREE ...)` — is the classic "build a renderer incrementally, one class per object" structure used by Kosarevsky's material.

In the *Vulkan 3D Graphics Rendering Cookbook* (2nd ed.), the "Initializing Vulkan instance and graphical device" recipe opens `main()` with the verbatim line:

```cpp
minilog::initialize(nullptr, { .threadNames = false });
```

followed by GLFW window creation via `lvk::initWindow(...)` and `lvk::createVulkanContextWithSwapchain(...)`. A later error path uses:

```cpp
MINILOG_LOG_PROC(minilog::FatalError, "Missing Vulkan device extensions: %s\n", ...);
```

### 3. What minilog provides

Per its README, the feature list is (verbatim): "Easy to use (2 files, C-style interface); Multiple outputs (a text log file, an HTML log file, Android logcat, system console with colors); Thread-safe (write to log from multiple threads, set thread names); Cross-platform (Windows, Linux, OS X, Android); Callbacks (if you want to intercept formatted log messages); Custom time stamps; C++17." It also supports manual per-thread callstacks.

### 4. The API

From `minilog.h` (v1.2.0):

- **Log levels** (verbatim):
  ```cpp
  enum eLogLevel { Paranoid = 0, Debug = 1, Log = 2, Warning = 3, FatalError = 4 };
  ```
- **Core functions:**
  ```cpp
  bool minilog::initialize(const char* fileName, const LogConfig& cfg);
  void minilog::deinitialize();
  void minilog::log(eLogLevel level, const char* format, ...); // printf-style, thread-safe
  void minilog::logRaw(...);
  ```
- **Helper macros:** `LLOGP` (Paranoid), `LLOGD` (Debug), `LLOGL` (Log), `LLOGW` (Warning), `LLOGE` (FatalError) — each forwarding to `MINILOG_LOG_PROC`, which is `minilog::log` normally or `minilog::logRaw` if `MINILOG_RAW_OUTPUT` is defined.
- **Extras:** `threadNameSet/Get`, `callstackPushProc/PopProc`, RAII `CallstackScope`, and `callbackAdd/Remove` for routing messages into (e.g.) an in-game console.

### 5. How it's integrated

minilog compiles from `minilog.cpp` + `minilog.h` and ships its own `CMakeLists.txt`. In Kosarevsky's own LightweightVK (`corporateshark/lightweightvk`), it's gated behind the CMake option `LVK_WITH_MINILOG` (confirmed verbatim in the repo's docs: "LVK_WITH_MINILOG: Enable Minilog (default: ON)"), and dependencies are downloaded into `third-party/deps/` by a Python bootstrap script "Driven by third-party/bootstrap-deps.json."

The cookbook uses the same "Bootstrap" tool: a `deploy_deps.py`/`bootstrap.json` mechanism clones dependencies into `deps/src/...`, after which the root `CMakeLists.txt` pulls them in. In the cookbook, **minilog typically comes in transitively via LightweightVK** rather than as a separate top-level dependency.

---

## Details

### Why a book uses a logging library at all

A Vulkan renderer produces a lot of diagnostic output: which physical device was picked, which queue families were found, whether required device extensions are present, swapchain format/present-mode selection, validation-layer messages, and fatal initialization errors. Rather than sprinkle `printf`/`std::cout` everywhere (which is untidy, not thread-safe, and hard to redirect), the book routes everything through minilog so that:

- (a) every message has a severity level and can be filtered;
- (b) output goes to both a colored console and a persistent log file simultaneously;
- (c) messages from worker threads (Vulkan apps are multithreaded) don't interleave into garbage; and
- (d) a single call can switch verbosity.

The `MINILOG_LOG_PROC(minilog::FatalError, "Missing Vulkan device extensions: %s\n", ...)` line in the cookbook's device-creation recipe is a concrete example: when a required extension is absent, it logs a fatal error before aborting.

### Distinguishing the correct project

When you search GitHub for "minilog" you will hit multiple libraries. To be unambiguous:

- **Correct for your book:** `corporateshark/minilog` — namespace `minilog`, `enum eLogLevel { Paranoid, Debug, Log, Warning, FatalError }`, macros `LLOGL/LLOGW/LLOGE`, include path `#include "minilog/minilog.h"`. Author Sergey Kosarevsky is the cookbook author.
- **Not your book:** `archibate/minilog` (C++20 single-header teaching project), `dominikschnitzer/minilog` (uses `MINILOG(logINFO) << ...` stream syntax and a `MiniLog::current_level()` API), `ysbbswork/Minilogger` (a `Logger` class), `RDCH106/miniLogger` (LGPL), and `EsotericSoftware/minlog` (Java).

If your code calls `minilog::log(...)` or uses `LLOG*`/`MINILOG_LOG_*` macros, you have the corporateshark one.

### Basic usage example (corporateshark/minilog)

```cpp
#include "minilog/minilog.h"

int main() {
    // 1) Initialize once at startup. Second arg is a LogConfig (brace-init).
    minilog::initialize("app.log", {});           // plain text log file + console
    // The cookbook uses:  minilog::initialize(nullptr, { .threadNames = false });
    // (nullptr = no log file, console only; disable thread-name prefixes)
    // For an HTML log:  minilog::initialize("app.html", { .htmlLog = true });

    // 2) Log with explicit levels (printf-style, thread-safe):
    minilog::log(minilog::Log,     "Vulkan instance created");
    minilog::log(minilog::Log,     "Picked GPU: %s", deviceName);
    minilog::log(minilog::Warning, "Falling back to FIFO present mode");
    minilog::log(minilog::FatalError, "vkCreateDevice failed: %d", (int)result);

    // 3) Or use the convenience macros (same levels):
    LLOGL("Swapchain: %u images, %ux%u", imageCount, w, h);   // Log
    LLOGW("Validation layer requested but not available");     // Warning
    LLOGE("No suitable queue family found");                   // FatalError

    // 4) Shut down at exit to flush and write the log "outro":
    minilog::deinitialize();
    return 0;
}
```

The `LogConfig` struct lets you tune behavior. Confirmed defaults from `minilog.h`:

| Field | Default | Meaning |
|---|---|---|
| `logLevel` | `minilog::Debug` | Threshold written to the file |
| `logLevelPrintToConsole` | `minilog::Log` | Threshold printed to the console |
| `forceFlush` | `true` | Calls `fflush()` after every `log()` |
| `coloredConsole` | `true` | Colorize console output |
| `htmlLog` | `false` | Also write an HTML log |

Plus `threadNames`, `mainThreadName`, and a custom `writeTimeStamp` function pointer. Levels below the configured threshold are cheaply discarded.

> **Note:** Some codebases (including the cookbook's helper headers) wrap these in project-specific macros such as `MINILOG_LOG_PROC`, or `LOGI`/`LOGE`-style aliases. **`LOGI`/`LOGE` are not minilog's native macro names** — minilog's own macros are `LLOGP/LLOGD/LLOGL/LLOGW/LLOGE` — so if you see `LOGI`/`LOGE` in your project they're either project-defined aliases or come from another logging shim (Android's `<android/log.h>` also defines `LOGI`/`LOGE`). Check the book's shared/utility header to see which macro maps to which minilog level.

### The GitHub repository

- **Repository:** https://github.com/corporateshark/minilog
- **Layout:** `minilog.h`, `minilog.cpp`, `example.cpp`, `CMakeLists.txt`, `LICENSE` (MIT), `README.md`.
- It is a **compiled 2-file library** (not strictly header-only): you build `minilog.cpp` and include `minilog.h`.

### Integrating into your existing CMakeLists.txt

You already have an executable target (say `VulkanRenderer`) built from your `main.cpp`/`App.cpp`/etc. You have three good options.

#### Option A — Vendored subdirectory (simplest, mirrors the book)

Put minilog under `deps/` (or `third-party/`) as a git submodule or copied source, then:

```cmake
# Bring in minilog's own CMakeLists.txt, which defines the library target:
add_subdirectory(deps/minilog)

# Link it to your renderer and make its headers visible:
target_link_libraries(VulkanRenderer PRIVATE minilog)
```

Because minilog ships its own `CMakeLists.txt`, `add_subdirectory` gives you a ready-made library target you can link against. If the target's headers aren't automatically on your include path, add:

```cmake
target_include_directories(VulkanRenderer PRIVATE deps)
```

so that `#include "minilog/minilog.h"` resolves.

As a git submodule:

```bash
git submodule add https://github.com/corporateshark/minilog deps/minilog
git submodule update --init --recursive
```

#### Option B — FetchContent (no vendored copy; CMake ≥ 3.14)

Let CMake clone it at configure time:

```cmake
include(FetchContent)
FetchContent_Declare(
  minilog
  GIT_REPOSITORY https://github.com/corporateshark/minilog.git
  GIT_TAG        master        # or pin to a specific commit for reproducibility
)
FetchContent_MakeAvailable(minilog)

target_link_libraries(VulkanRenderer PRIVATE minilog)
```

#### Option C — Just compile the two files yourself (most minimal)

If you don't want minilog's CMakeLists at all, add its sources directly to your target:

```cmake
target_sources(VulkanRenderer PRIVATE deps/minilog/minilog.cpp)
target_include_directories(VulkanRenderer PRIVATE deps)   # so "minilog/minilog.h" is found
```

### Keeping it consistent with your `source_group(TREE ...)`

Your book uses `source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ...)` purely to mirror the on-disk folder structure into Visual Studio/Xcode "filters" so `VulkanInstance.cpp`, `Swapchain.cpp`, etc. appear in tidy IDE folders.

minilog is orthogonal to that: if you use `add_subdirectory`/`FetchContent`, minilog's files live under their own target and won't clutter your `source_group` tree. Only if you inline minilog's `.cpp` into your own target (Option C) should you decide whether to list it in your `source_group(TREE ...)` FILES set.

> **Note:** `source_group(TREE ...)` requires that every listed file actually lives under the given root directory, or CMake errors out — so keep third-party sources out of your app's TREE grouping.

---

## Recommendations

1. **Confirm which minilog you have (2 minutes).** Grep your source for `minilog::`, `LLOG`, `MINILOG_LOG`, or `#include "minilog`. If you see the `minilog::` namespace or `LLOG*` macros, it is `corporateshark/minilog` — proceed below. If you instead see `MINILOG(logINFO) <<` stream syntax or a `Logger` class, you have a different library and should match that repo instead.

2. **Prefer the integration style your book already uses.** If your book's top-level `CMakeLists.txt` calls a Python bootstrap/`deploy_deps.py` step or has a `deps/`/`third-party/` folder, run that first — minilog will be fetched for you (often transitively through LightweightVK, gated by `LVK_WITH_MINILOG=ON`). Don't hand-add it in parallel, or you'll get duplicate targets.

3. **If you're building your own standalone renderer, use Option A (`add_subdirectory`) or Option B (`FetchContent`).** Option A is closest to the book and works offline once vendored; Option B is cleanest if you don't want to commit third-party code. In both cases finish with `target_link_libraries(YourTarget PRIVATE minilog)` and `#include "minilog/minilog.h"`.

4. **Initialize early, deinitialize late.** Call `minilog::initialize(...)` at the very top of `main()`/App startup (before creating the Vulkan instance) and `minilog::deinitialize()` at shutdown, so instance/device/swapchain diagnostics are captured and the file is flushed. Pass a filename for a persistent log, or `nullptr` for console-only as the cookbook does. Set `logLevelPrintToConsole` higher than `logLevel` to keep the console quiet while the file keeps full detail.

5. **Pin a version for reproducibility.** For `FetchContent`, replace `GIT_TAG master` with a specific commit hash so your build doesn't silently change when upstream updates.

### Thresholds that change the plan

- If your build already succeeds and minilog symbols link, do nothing further.
- If you get `undefined reference to minilog::log(...)`, you included the header but never compiled `minilog.cpp` — add the library target/source (Options A–C).
- If you get `cannot open source file "minilog/minilog.h"`, your include directory is wrong — add the parent of the `minilog/` folder via `target_include_directories`.
- If CMake errors that `source_group TREE` files must be under the root, remove minilog's files from your app's TREE grouping.

---

## Caveats

- **Exact CMake target name unconfirmed.** I could not fetch the verbatim text of minilog's own `CMakeLists.txt`, so the exact library target name and whether it is declared `STATIC`/`SHARED`/auto is an inference from the repo layout (a compiled `minilog.cpp` + `minilog.h` pair with a shipped `CMakeLists.txt`). The link name is conventionally `minilog`; if `target_link_libraries(... minilog)` fails to resolve, open `deps/minilog/CMakeLists.txt` and use the exact `add_library(...)` target name defined there. Option C (compiling `minilog.cpp` directly) sidesteps this entirely.
- **`LOGI`/`LOGE` are not minilog's native macros.** minilog's own macros are `LLOGP/LLOGD/LLOGL/LLOGW/LLOGE` and `MINILOG_LOG_PROC`. If your book uses `LOGI`/`LOGE`, they are project-defined aliases (check the shared utility header) or come from a different logging path (e.g., Android's NDK log macros). Don't assume they map 1:1 to minilog without checking.
- **Book edition matters.** The 1st-edition "3D Graphics Rendering Cookbook" (2021, OpenGL + Vulkan) and the 2nd-edition "Vulkan 3D Graphics Rendering Cookbook" (2025, Vulkan 1.3 + LightweightVK) differ in structure and dependency tooling. Your exact filenames aren't a verbatim match to either book's published sample repo (which use recipe-numbered folders), so it's possible you're following a different tutorial that adopted Kosarevsky's minilog. The integration guidance above is identical regardless.
- **Small project, low bus factor.** As of this writing the repo is a small personal library (34 GitHub stars, 2 forks). It's stable and MIT-licensed, but it is not a large community project like spdlog; pin a commit and vendor a copy if you need long-term build stability.

---

## Related: LightweightVK (lvk)

**What it is:** LightweightVK is a lean, cross-platform C++ graphics API sitting on top of Vulkan 1.3, created by Sergey Kosarevsky. It's "a deeply refactored bindless-only fork of https://github.com/facebook/igl." Repo: https://github.com/corporateshark/lightweightvk

**Its relationship to the book:** It's the foundation the entire *Vulkan 3D Graphics Rendering Cookbook* (2nd ed.) is built on. "This library implements all the low-level Vulkan wrapper classes, which we will discuss in detail throughout this book." **This means the book expects LightweightVK to *be* your VulkanInstance/PhysicalDevice/LogicalDevice/Surface/Swapchain code** — not something you additionally hand-write from scratch alongside it.

**Minimal usage example from the book:**

```cpp
int main(void) {
  minilog::initialize(nullptr, { .threadNames = false });
  int width = 960, height = 540;
  GLFWwindow* window = lvk::initWindow("Simple example", width, height);
  std::unique_ptr<lvk::vulkan::VulkanContext> ctx =
      lvk::createVulkanContextWithSwapchain(window, width, height, {});
}
```

**CMake integration:** LightweightVK uses its own bootstrap-based dependency system — a `deploy_deps.py` script clones/downloads third-party libraries (Vulkan headers, GLFW, GLM, ImGui, Tracy, Slang, etc.) into `third-party/deps/`, driven by `third-party/bootstrap-deps.json`. Confirmed CMake options:

| Option | Default | Purpose |
|---|---|---|
| `LVK_DEPLOY_DEPS` | ON | Deploy dependencies via CMake |
| `LVK_WITH_GLFW` | ON | Enable GLFW |
| `LVK_WITH_SAMPLES` | ON | Build sample demo apps |
| `LVK_WITH_SAMPLES_ANDROID` | OFF | Generate Android projects for demo apps |
| `LVK_WITH_TRACY` | ON | Enable Tracy profiler |
| `LVK_WITH_MINILOG` | ON | Enable Minilog |

The cookbook itself declares LightweightVK as a git dependency in its own bootstrap config:

```json
{
  "name": "lightweightvk",
  "source": {
    "type": "git",
    "url": "https://github.com/corporateshark/lightweightvk.git",
    "revision": "v1.3"
  }
}
```

**Wiring it into your own CMakeLists.txt:**

```cmake
add_subdirectory(deps/lightweightvk)   # after deploy_deps.py has cloned it
target_link_libraries(VulkanRenderer PRIVATE LVKLibrary)  # confirm exact target name in its CMakeLists.txt
```

Because LightweightVK vendors its own `third-party/deps/`, it typically brings in GLFW, GLM, minilog, ImGui, Tracy, etc. transitively — so you likely don't need to declare minilog separately if you're already pulling in lightweightvk.

Your `source_group(TREE ...)` grouping is unaffected either way, since it only applies to files you explicitly list under your own target — LightweightVK's sources live inside its own subdirectory/target and won't appear in your tree unless you add them yourself.
