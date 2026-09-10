# Tannery — Project Notes & Lessons Learned

A running reference for this project, updated as the Vulkan renderer grows.
Unlike the per-lesson files (see `lesson-one/`), this doc isn't tied to one
milestone — it's the place to look up "wait, why did we decide that?" or
"what's the pattern we've been using for X?" regardless of which lesson it
came from.

- **Project name:** Tannery (renamed from an earlier working name,
  `forge3d`; same git history, same all-rights-reserved license)
- **Stack:** raw Vulkan C API, GLFW3 (windowing), C++23, CMake
- **Environment:** native Ubuntu, GNOME/Wayland, NVIDIA RTX 3090
  (proprietary driver, Vulkan 1.4.x), GCC 15.2

## Current architecture

```
core/
  include/   *.hpp — RAII wrapper class declarations, plus a couple of
             free-function/header-only helper modules that don't own a
             long-lived handle (physicalDevice, queueFamilies,
             swapchainSupport, debugCallbackVulkan.hpp)
  src/       *.cpp — one .cpp per class/concern
```

**Restructured (this session) from the original flat-function-per-file +
manual `cleanup()` design into RAII wrapper classes** —
`Window`, `VulkanInstance`, `Surface`, `LogicalDevice`, `Swapchain`, each
owning exactly one (or a small related set of) Vulkan handle(s). Every
wrapper follows the same shape: the constructor creates the handle or
`throw`s `std::runtime_error` on failure; the destructor destroys it if
it's not `VK_NULL_HANDLE`; copy and move construction/assignment are all
explicitly `= delete`d, since these types own a unique GPU resource that
must never be duplicated or left dangling. `physicalDevice` and
`queueFamilies` stayed as plain free functions — a `VkPhysicalDevice` isn't
created/destroyed by the app (it's enumerated hardware), and
`QueueFamilyIndices` is just a query result struct, so neither needs
ownership semantics.

A new `App` class (`App.hpp`/`App.cpp`) owns one instance of each wrapper
as a member field, in dependency order:
`window → instance → surface → physicalDevice → indices → device → support → swapchain`.
This ordering is load-bearing, not cosmetic — C++ constructs members in
declaration order and destroys them in the *reverse* of that order,
regardless of what order they're listed in the constructor's initializer
list. Because the fields are declared in the same order the objects
actually depend on each other, `App`'s constructor builds everything
correctly and its (implicit, `= default`) destructor tears everything down
in the exact reverse-of-creation order the project already required by
convention — automatically, with no function to remember to call or update.

`main.cpp` is now just:
```cpp
try {
    App app(WINDOW_WIDTH, WINDOW_HEIGHT, "Tannery");
    app.run();
} catch (const std::exception& e) {
    std::println(stderr, "Fatal error: {}", e.what());
    return 1;
}
```
The old pattern — "create X, check X == VK_NULL_HANDLE, print, call
`cleanup(...)` with the right handles, `return N`" repeated at every
stage — is gone. Failure now means a constructor throws, and stack
unwinding runs every already-constructed member's destructor automatically
on the way out, so the object never exists half-torn-down. `cleanup.hpp`
itself was deleted. See "Concepts covered" item 14 below for the bugs this
refactor surfaced and how they were fixed.

## Concepts covered so far

1. **`VkInstance`** — the Vulkan library's entry point for this process.
   Built from `VkApplicationInfo` + `VkInstanceCreateInfo`; needs to know
   which extensions to enable (GLFW tells us via
   `glfwGetRequiredInstanceExtensions`, since core Vulkan knows nothing
   about windowing).
2. **Validation layers** (`VK_LAYER_KHRONOS_validation`) — Vulkan drivers do
   almost no error checking by default (a deliberate performance trade-off).
   The validation layer is an opt-in interceptor that checks every call for
   correct usage. Enabled via `enabledLayerCount`/`ppEnabledLayerNames` on
   `VkInstanceCreateInfo`, same count-then-array shape as extensions.
3. **Debug messenger (`VK_EXT_debug_utils`)** — structured delivery of
   validation messages to our own callback (severity + type + message text),
   instead of relying on the layer's default stdout fallback. Because
   `vkCreateDebugUtilsMessengerEXT`/`vkDestroyDebugUtilsMessengerEXT` are
   _extension_ functions, they aren't linked directly — they're loaded at
   runtime via `vkGetInstanceProcAddr` (see `debugCallbackVulkan.hpp`'s
   proxy functions). Chaining `VkDebugUtilsMessengerCreateInfoEXT` into
   `VkInstanceCreateInfo.pNext` also catches issues _during_
   `vkCreateInstance`/`vkDestroyInstance` themselves, before the persistent
   runtime messenger even exists.
4. **`VkPhysicalDevice`** — not created, _enumerated_ (`vkEnumeratePhysicalDevices`,
   count-then-array). Represents real hardware already present on the
   system; a machine can expose several. `VkPhysicalDeviceProperties2` (the
   `pNext`-extensible version) gets device name/type/API version/vendor+
   device IDs.
5. **Queue families** — a GPU's queues are grouped into families sharing
   the same capabilities (`VkQueueFamilyProperties.queueFlags`:
   graphics/compute/transfer/sparse-binding bits). Found via
   `vkGetPhysicalDeviceQueueFamilyProperties` (count-then-array again — the
   fourth time this exact pattern has shown up). This GPU (RTX 3090)
   reports 6 queue families; family index `0` supports graphics.
6. **`VkDevice` (logical device) + `VkQueue`** — the live, software instance
   of a `VkPhysicalDevice` that the app actually talks to; everything
   downstream (memory, pipelines, command submission) hangs off this handle
   rather than the physical device. Built from one or more
   `VkDeviceQueueCreateInfo` (family index + queue count + a `pQueuePriorities`
   array that must stay alive through the `vkCreateDevice` call itself, since
   Vulkan doesn't copy it) plus a `VkPhysicalDeviceFeatures` (all-zero for the
   triangle milestone — no optional GPU features needed yet) folded into
   `VkDeviceCreateInfo`. The queue handle itself isn't returned by
   `vkCreateDevice` — it's fetched afterward with a separate
   `vkGetDeviceQueue(device, familyIndex, queueIndexWithinFamily, &queue)` call.
7. **`VkSurfaceKHR`** — core Vulkan has no concept of a window; presenting to
   one is entirely handled by optional WSI extensions (`VK_KHR_surface` +
   a platform-specific one, `VK_KHR_wayland_surface` here), both already
   pulled in by `glfwGetRequiredInstanceExtensions`. `glfwCreateWindowSurface`
   wraps the platform-specific creation call so the app never has to branch
   on OS itself. A queue family supporting graphics is _not_ guaranteed to
   support presenting to a given surface — that's a separate, per-surface
   query: `vkGetPhysicalDeviceSurfaceSupportKHR(device, familyIndex, surface,
&supported)`. `QueueFamilyIndices` grew a second field,
   `presentFamilyIndex`, plus an `isComplete()` helper. On the RTX 3090 both
   indices land on family `0`, but the code can't assume that — Vulkan
   forbids duplicate `queueFamilyIndex` entries in
   `VkDeviceCreateInfo::pQueueCreateInfos`, so `createLogicalDevice`
   deduplicates via `std::set<uint32_t>` before building one
   `VkDeviceQueueCreateInfo` per _unique_ family, and fetches both
   `graphicsQueue` and `presentQueue` afterward (which may be the same
   handle, or may not, depending on hardware).

8. **Swapchain support querying (`SwapchainSupportDetails`)** — before a
   `VkSwapchainKHR` can be created, three properties of the
   `(VkPhysicalDevice, VkSurfaceKHR)` pair must be queried:
   `vkGetPhysicalDeviceSurfaceCapabilitiesKHR` (a single struct fill, _not_
   count-then-array — there's exactly one capabilities struct, never an
   array), `vkGetPhysicalDeviceSurfaceFormatsKHR`, and
   `vkGetPhysicalDeviceSurfacePresentModesKHR` (both back to the familiar
   count-then-array shape). Mirrors `QueueFamilyIndices`'s design: a struct
   (`SwapchainSupportDetails`) holding all three results plus an
   `isComplete()` helper — though unlike queue families, `isComplete()`
   here only checks `formats`/`presentModes` non-empty, since `capabilities`
   is a single struct with no natural "empty" state. An early draft
   mistakenly gated completeness on `capabilities.maxImageCount > 0`, but
   `maxImageCount == 0` is a _valid_ spec value meaning "no upper limit,"
   not a failure signal — caught and removed before it could misfire on a
   real driver reporting that value.

9. **Device extensions (`VK_KHR_swapchain`)** — unlike instance extensions
   (`VK_EXT_debug_utils`), swapchain support is a _device_ extension: it
   must be requested per-logical-device via
   `VkDeviceCreateInfo::ppEnabledExtensionNames`, since not every
   Vulkan-capable device (e.g. a headless compute GPU) needs it. Verified
   as actually supported first via `vkEnumerateDeviceExtensionProperties`
   (same count-then-array shape as `vkEnumeratePhysicalDevices`), matching
   each requested name with `strcmp` against every reported
   `VkExtensionProperties::extensionName`, before adding it to the enabled
   list — requesting an unsupported extension makes `vkCreateDevice` fail
   with `VK_ERROR_EXTENSION_NOT_PRESENT`.

10. **Swapchain settings selection** (`chooseSwapSurfaceFormat`,
    `chooseSwapPresentMode`, `chooseSwapExtent` in `swapchainSupport.cpp`) —
    three independent design decisions made from the data
    `getSwapchainSupportDetails` already queried, each with a standard
    preferred choice plus a spec-guaranteed fallback: surface format prefers
    `VK_FORMAT_B8G8R8A8_SRGB` + `VK_COLOR_SPACE_SRGB_NONLINEAR_KHR`, falling
    back to whatever's first in the list if that combo isn't offered;
    present mode prefers `VK_PRESENT_MODE_MAILBOX_KHR` (low-latency triple
    buffering), falling back to `VK_PRESENT_MODE_FIFO_KHR` (the only mode
    the spec guarantees exists everywhere); extent either takes
    `capabilities.currentExtent` directly, or — when the surface reports
    `currentExtent.width == UINT32_MAX` (a sentinel meaning "you choose") —
    queries `glfwGetFramebufferSize` and clamps it into
    `[minImageExtent, maxImageExtent]` with `std::clamp`.
11. **`VkSwapchainKHR` creation** (`createSwapchain` in
    `swapchainSupport.cpp`) — folds the three chosen settings plus the
    queried capabilities into one `VkSwapchainCreateInfoKHR` and calls
    `vkCreateSwapchainKHR`. Two details worth remembering: image count is
    `capabilities.minImageCount + 1` (one extra so the driver isn't ever
    forced to stall waiting on the app), clamped back down to
    `maxImageCount` only if that's nonzero (zero means "no upper limit," the
    same gotcha noted in item 8 below); and `imageSharingMode` branches on
    whether the graphics and present queue families actually differ —
    `VK_SHARING_MODE_CONCURRENT` (no ownership transfers needed, simpler but
    slower) when they're different families, `VK_SHARING_MODE_EXCLUSIVE`
    (best performance, requires explicit ownership transfers to use from
    another family) when they're the same, which is the common case on this
    RTX 3090 where both indices land on family `0`.
12. **`cleanup.hpp` grew a `VkSwapchainKHR` parameter** — the swapchain is a
    child of the logical device, so `vkDestroySwapchainKHR` has to run
    _before_ `vkDestroyDevice`. Destruction order is now: messenger →
    swapchain → device → surface → instance → window, exactly reversing
    creation order as the convention below requires.
13. **Cross-platform portability extensions** — `VK_KHR_portability_enumeration`
    (instance-level, `vulkanInstance.cpp`) plus the
    `VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR` flag, and
    `VK_KHR_portability_subset` (device-level, `logicalDevice.cpp`), both
    gated behind `#ifdef __APPLE__` since they only matter for MoltenVK
    (Vulkan-over-Metal) — a real conformant driver like NVIDIA's on Linux
    never advertises `portability_subset`, so requiring it unconditionally
    would make this codebase's Linux path either fail or silently disable
    optimizations meant only for Mac. `VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME`
    had to be manually `#define`d in `logicalDevice.cpp`, since the official
    Vulkan headers only expose it when `VK_ENABLE_BETA_EXTENSIONS` is
    defined at compile time — this project isn't using that flag, so the
    name string is declared by hand instead of pulled from `vulkan_core.h`.

14. **RAII restructure** — every Vulkan-owning concern became a class
    (`Window`, `VulkanInstance`, `Surface`, `LogicalDevice`, `Swapchain`)
    with a throwing constructor and a destructor that null-checks before
    destroying, orchestrated by a new `App` class whose member declaration
    order encodes the dependency graph (see "Current architecture" above).
    Three real bugs came out of doing this conversion, all caught in
    review before ever building:
    - `swapchainSupport.cpp`'s old free-function `chooseSwapSurfaceFormat`
      / `chooseSwapPresentMode` / `chooseSwapExtent` / `createSwapchain`
      were left in place after `Swapchain`'s constructor was written to do
      the exact same work as private methods — ~95 lines of dead code
      nothing called anymore, since it happened to compile fine (different
      scope: free functions vs. `Swapchain::` members). Deleted once
      spotted; only `getSwapchainSupportDetails` still belongs in that
      file.
    - `Surface`'s move-assignment operator was declared
      (`Surface& operator=(Surface&&);`) but never `= delete`d or defined
      — inconsistent with its other three special members, and a latent
      link error (not even a clear compile error) waiting for the day
      something actually tried to move-assign a `Surface`.
    - Converting `LogicalDevice`'s extension-check loop accidentally
      **added** a new hard requirement: the loop already tracked whether
      `VK_KHR_driver_properties` was supported, but a stray `throw` got
      added for it not being present — even though nothing in the
      codebase reads driver-properties data, so failing startup over a
      driver not reporting an unused extension wasn't intentional.
      Reverted; that extension is opportunistic again (enabled if present,
      simply skipped if not).

15. **`VkRenderPass`** (new `RenderPass` class, `RenderPass.hpp`/`.cpp`) —
    describes the shape of a frame's rendering *before* any actual image
    views are bound to it (that binding happens later, per-frame, via
    `VkFramebuffer`). Built from four pieces: a `VkAttachmentDescription`
    for the single color attachment (format matches the swapchain's chosen
    `surfaceFormat`, `loadOp = CLEAR`/`storeOp = STORE` since the triangle
    needs to be visible after the pass, `initialLayout = UNDEFINED` since
    clearing makes prior contents irrelevant, `finalLayout =
    PRESENT_SRC_KHR` since the image goes straight to `vkQueuePresentKHR`
    next); a `VkAttachmentReference` (`attachment = 0`, an *index* into the
    attachments array, not a handle); a `VkSubpassDescription` referencing
    it; and a `VkSubpassDependency` from `VK_SUBPASS_EXTERNAL` to subpass
    `0`, both stage masks set to `VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT`,
    forcing the subpass to wait until that stage rather than starting to
    write color output before the swapchain image is actually available.
    Two bugs caught in review before ever building: an accidental
    `dependency.dstStageMask = 0;` typo (meant to be `srcAccessMask`)
    silently reset the just-configured `dstStageMask` back to `0`,
    defeating the entire synchronization point without any compiler error;
    and `RenderPass renderPass;` was initially declared in `App.hpp`
    *before* `device`/`swapchain`, so despite the `App.cpp` initializer
    list writing `renderPass(device.handle(), swapchain.formatHandle())`
    last (which reads correctly), C++ still constructs members in
    declaration order regardless of initializer-list order — moved to be
    the last-declared member, after `swapchain`, to fix it. Same pattern as
    concept 14's bug list: this class matters because nothing about it is
    checked by the type system, only by paying attention to declaration
    order.

16. **Shader loading (`readFile`/`createShaderModule`, `shaderModule.hpp`/
    `.cpp`) + `Pipeline` class scaffolding (`Pipeline.hpp`/`.cpp`, in
    progress)** — unlike every wrapper class so far, a `VkShaderModule` is
    transient: created, referenced while building the pipeline, and
    destroyed immediately after, so this deliberately is **not** an RAII
    class. `readFile` (`std::ifstream` opened with `ios::binary | ios::ate`
    to read the file size via `tellg()` before seeking back to `0` and
    reading) returns `std::vector<char>`; `createShaderModule` fills
    `VkShaderModuleCreateInfo` (`codeSize` in bytes, `pCode` as
    `reinterpret_cast<const uint32_t*>(code.data())` — legal because
    `std::vector`'s allocator guarantees `max_align_t` alignment, which
    satisfies `uint32_t`'s alignment requirement) and calls
    `vkCreateShaderModule`. Both are free functions living in a
    lowercase-named file (`shaderModule.hpp`, not `ShaderModule.hpp`) per
    the established convention that lowercase-first files are
    free-function/header-only helpers, not RAII wrapper classes. The new
    `Pipeline` class (constructor: `VkDevice`, `VkRenderPass`, `VkExtent2D`,
    vertex/fragment shader paths) calls both to build a
    `VkPipelineShaderStageCreateInfo` per stage (`stage` = which shader
    stage, `module` = the created `VkShaderModule`, `pName` = `"main"`,
    the GLSL entry point name both shaders use). One bug caught in review:
    a copy-pasted `fragStageInfo.module = vertModule;` (should have been
    `fragModule`) — would have made the fragment stage try to execute a
    module compiled with only a `Vertex` SPIR-V execution model, since
    `glslangValidator` bakes the stage's execution model in at compile time
    based on the `.vert`/`.frag` source; caught before `vkCreateGraphicsPipelines`
    was ever reached.

**The "count, then array" convention** has now appeared four times
(`glfwGetRequiredInstanceExtensions`, `vkEnumeratePhysicalDevices`,
`vkGetPhysicalDeviceQueueFamilyProperties`, and implicitly in layer/extension
enumeration) — call once with a null output pointer to get a count, allocate,
call again to fill it. Expect it constantly in the swapchain/image stages
still ahead.

## Conventions established

- **Designated initializers** (`.fieldName = value`) for every Vulkan
  struct, always in declaration order (C++ requirement, unlike C). Adopted
  after an early bug where positional initialization silently put
  `pApplicationName`'s value into `pEngineName`'s slot.
- **Every Vulkan handle is initialized to `VK_NULL_HANDLE` at declaration**,
  never left default-uninitialized — this makes "was this ever actually
  created?" checkable, and makes it safe to pass an unset handle into the
  matching destroy function (Vulkan destroy calls treat `VK_NULL_HANDLE` as
  a documented no-op, same convention as `free(NULL)`).
- **`std::optional<T>` for "not found yet" state**, instead of a sentinel
  value like `-1` — used for `QueueFamilyIndices::graphicsFamilyIndex`, since
  a valid queue family index can legitimately be `0`, which would collide
  with an `-1`/`0`-as-sentinel convention.
- **`std::print`/`std::println` (C++23) over `iostream`/`printf`** for new
  code, but note: `std::print`'s format string uses `{}` placeholders, _not_
  printf's `%d`/`%s` — mixing the two conventions compiles in some cases but
  produces wrong output (the literal `%d`/`%s` characters get printed
  as-is). Double-check this whenever translating old `fprintf` code to
  `std::print`.
- **Reverse-of-creation-order teardown**, always — window → instance →
  messenger → physical-device-derived-stuff created in that order means
  cleanup destroys in the opposite order. This became non-negotiable rather
  than theoretical the moment `VkDevice` entered the picture: a first draft
  of `cleanup.hpp` destroyed the logical device _after_ `vkDestroyInstance`,
  which is a genuine Vulkan Object Lifetime violation (a device is a child
  of the instance/physical-device hierarchy, not just an unrelated handle)
  — caught in review before it was ever run. Correct order going forward:
  messenger → device → instance → window → `glfwTerminate`. Expect this
  chain to keep growing as swapchain/pipeline objects (children of the
  device) join it.
- **Every error branch must actually stop, not just log.** The most
  frequently repeated real bug across early lessons was logging a failure
  message and then letting execution fall through to code that assumed
  success (an uninitialized/garbage handle passed to a destroy call, an
  empty `std::vector` indexed at `[0]`). Standing habit: whenever writing an
  `if (failed) { print(...); }`, explicitly check whether that block ends in
  a `return`/`break` or if control can fall past it. Recurred in the surface
  lesson too — a first draft's `surface == VK_NULL_HANDLE` branch returned
  early but skipped the `cleanup()` call every sibling branch made, leaking
  the window/instance/messenger on that one path. "Stops" means "stops and
  tears down," not just "stops." Recurred a fourth time, in the opposite
  direction, in the swapchain-creation failure branch: a first draft called
  `cleanup()` on `vkCreateSwapchainKHR` failure but had no `return` after
  it, so execution fell through into the render loop using a window
  `cleanup()` had just destroyed (and a GLFW that had just been
  terminated) — caught in review before it was ever run. "Stops and tears
  down" cuts both ways: tearing down without stopping is just as broken as
  stopping without tearing down. **This entire bug class — forgetting to
  update a manual teardown call, forgetting to `return` after one, getting
  the order wrong — is what the RAII restructure (concept 14) eliminates
  structurally.** There's no `cleanup()` function left to forget to call or
  update; a constructor throwing just unwinds the stack and every
  already-built member's destructor runs, in the right order, without any
  code written per-object to make that happen.
- **A loop that searches for multiple independent things needs an
  independent stop condition for each one.** `findQueueFamilies`'s first
  draft checked present-support and graphics-support in the same loop
  iteration but only `break`d on finding the graphics family — so if the
  present-capable family had a higher index than the graphics family, the
  loop would exit before ever reaching it. Invisible on the RTX 3090 (both
  land on family `0`), so it passed a real test run while still being wrong.
  Fixed with a `QueueFamilyIndices::isComplete()` helper and only breaking
  once every field being searched for actually has a value — a reminder that
  "it printed the right answer on my machine" isn't the same as "the logic
  is correct." Recurred a third time in the device-extension support check
  (`logicalDevice.cpp`): a first draft used one shared `else { break; }` for
  two independently-searched extension names, so a single non-matching
  entry at any index aborted the search for _both_ names at once. Fixed the
  same way — an independent `if (!found)` guard per target, with the
  combined early-exit condition re-evaluated inside the loop rather than
  computed once beforehand.
- **A platform-conditional early-exit condition needs to actually be
  conditional, not just wrapped in a preprocessor guard next to an
  unconditional twin.** Adding the `VK_KHR_portability_subset` check to
  `createLogicalDevice`'s extension loop first landed with `canStop`
  requiring `portabilitySubsetSupported` unconditionally — which can never
  become `true` on a real conformant driver (NVIDIA on Linux never reports
  that extension), silently disabling the loop's early-`break` on this
  dev's actual machine. The fix attempt after that made it worse: two
  separate `#ifdef __APPLE__ ... #endif` blocks each declaring `bool
canStop`, which compiles on Linux (the first block is stripped) but is a
  duplicate-declaration error on Mac — exactly the platform the guard was
  supposed to support. The working shape is one unconditional declaration
  covering the platform-independent case, then an `#ifdef __APPLE__`
  block that _reassigns_ (not redeclares) it to fold in the Mac-only
  condition.

## Environment-specific gotchas worth remembering

- **Wayland windows aren't visible until a buffer is actually committed.**
  On X11, `glfwCreateWindow` maps a visible (if blank/undefined) window
  immediately. On this machine's Wayland/GNOME session, nothing shows up —
  not even in Alt+Tab — until the swapchain/present stage actually draws a
  frame. Don't mistake "nothing on screen yet" for a bug during the
  instance/device-setup lessons; it's expected until much later in the
  pipeline. Confirm the process is alive via `pgrep -af <binary>` instead.
- **`VK_LAYER_KHRONOS_validation` is a separate apt package**
  (`vulkan-validationlayers`), not covered by the base toolchain list in
  `CLAUDE.md` (`libvulkan-dev`, `libglfw3-dev`, `libglm-dev`,
  `glslang-tools`, `vulkan-tools`). Confirmed installed as of the
  validation-layers lesson — if this project is ever set up on a fresh
  machine, this package needs installing explicitly or validation-layer
  requests will fail with `VK_ERROR_LAYER_NOT_PRESENT`.
- **CMake keyword arguments (`REQUIRED`, etc.) and package names in
  `find_package()` are case-sensitive on Linux.** `find_package(vulkan
required)` silently fails to work as intended — lowercase `required` isn't
  recognized as the keyword, and `vulkan` (lowercase) won't match the
  bundled `FindVulkan.cmake` module file on a case-sensitive filesystem.
  Needs to be `find_package(Vulkan REQUIRED)`.
- **Imported CMake targets need their full namespaced name.** `Vulkan::Vulkan`,
  not bare `Vulkan` or `vulkan` — the `::` marks it as a real imported
  target (hard configure-time error if misspelled) rather than a raw
  linker flag guess.
- **A stray `#include <vulkan/vulkan_core.h>` (or, once, just
  `"vulkan_core.h"`) kept recurring across multiple new files** —
  `RenderPass.cpp` three separate times, then `Pipeline.hpp` once, with the
  same or similar redundant include each time. `<vulkan/vulkan.h>` (already
  included via each class's header) transitively includes `vulkan_core.h`
  itself — `vulkan.h` is the umbrella header that pulls in `vulkan_core.h`
  (the platform-agnostic core API) plus whatever platform-specific surface
  extensions apply, based on preprocessor defines. Including
  `vulkan_core.h` directly is never necessary anywhere in this codebase and
  was deliberately removed from `Swapchain.cpp` early on for that reason
  (commit `6add02d`). Root cause suspected but not yet confirmed to be an
  editor/clangd auto-import quick-fix triggering on an unresolved `Vk*`
  symbol — worth checking editor settings if it keeps happening.

## Status

Milestone 1 (hardcoded triangle) progress — see `lesson-one/milestone-1-triangle.md`
for the full step list. As of this note: instance, validation layers, physical
device + queue family selection, window surface creation, present-family
detection, logical device + deduplicated graphics/present queue creation, and
the full swapchain stage (support querying, format/present-mode/extent
selection, `VkSwapchainCreateInfoKHR` + `vkCreateSwapchainKHR`) are all done.
Also done, same session: cross-platform portability extension support
(`VK_KHR_portability_enumeration` instance-level, `VK_KHR_portability_subset`
device-level, both `#ifdef __APPLE__`-gated) for eventual MoltenVK
compatibility. The whole codebase was then restructured from flat functions +
manual `cleanup.hpp` into RAII wrapper classes (`Window`, `VulkanInstance`,
`Surface`, `LogicalDevice`, `Swapchain`) owned by a new `App` orchestrator —
see "Current architecture" and concept 14 above; `cleanup.hpp` no longer
exists.

**Update (later session):** image views (`vkCreateImageView`/
`vkDestroyImageView` per swapchain image) are done — folded directly into
`Swapchain` as a `std::vector<VkImageView>` member rather than getting a
separate wrapper class, since they're owned by and share the lifetime of the
swapchain that produced their source images. Also done: the shader-compile
side of Step 9 ahead of schedule — GLSL source (`data/shaders/triangle.vert`/
`.frag`), `glslangValidator` wired into `CMakeLists.txt` via
`find_program`/`add_custom_command`/`add_custom_target` to compile them to
SPIR-V at build time (output to `${CMAKE_BINARY_DIR}/shaders/`, only
recompiling a shader when its source changes).

**Update (later session):** the render pass half of Step 8 is also done —
see concept 15 above. Step 8 is now fully complete.

**Update (later session):** Step 9 (graphics pipeline) is underway — see
concept 16 above. Done so far: `shaderModule.hpp`/`.cpp` (`readFile`,
`createShaderModule`), `Pipeline` class scaffolded, both
`VkPipelineShaderStageCreateInfo` structs built. Next up: combine them into
a `VkPipelineShaderStageCreateInfo shaderStages[2]` array, then the
fixed-function state structs (vertex input, input assembly,
viewport/scissor, rasterizer, multisampling, color blending),
`VkPipelineLayoutCreateInfo` → `vkCreatePipelineLayout`, and finally
`VkGraphicsPipelineCreateInfo` → `vkCreateGraphicsPipelines`. `Pipeline`
still needs to destroy both shader modules after pipeline creation, and
still needs to be wired into `App` as a member. See
`lesson-one/milestone-1-triangle.md` for the detailed sub-checklist.
