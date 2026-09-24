# Tannery — Architecture Brain Map

A visual snapshot of everything built so far, current as of the
triangle-render milestone: dynamic rendering (Vulkan 1.3, no
`VkRenderPass`/`VkFramebuffer`), sync objects wired into the render loop, and
construction/teardown driven by RAII class lifetimes rather than manual step
codes. Two views: a **structural diagram** (module relationships, in the
spirit of a classic GoF UML class diagram) and a **build-order flow diagram**
(the actual sequence `App`'s constructor and `run()` execute).

This file is a living snapshot, not auto-generated — re-sync it by hand whenever
a new module/file pair is added.

---

## 1. Structural diagram (module relationships)

Each Vulkan "concern" is modeled as a class: its exposed free function(s) as
methods, and any struct it owns as attributes. `..>` is a dependency ("uses to
produce"), `-->` is an association ("needs a handle from"). This project is raw
procedural C-API code, not real OOP — treat the boxes as **file-pair modules**,
not literal C++ classes.

```mermaid
classDiagram
    class main_cpp {
        <<orchestrator>>
        +main() int
    }

    class WindowHandling {
        <<module>>
        +createWindow(w, h, title) GLFWwindow*
    }

    class VulkanInstance {
        <<module>>
        +createInstance(appName) VkInstance
    }

    class DebugCallbackVulkan {
        <<module, header-only>>
        +populateDebugMessengerCreateInfo(info)
        +CreateDebugUtilsMessengerEXT(...) VkResult
        +DestroyDebugUtilsMessengerEXT(...)
        -debugCallback(...) VkBool32
    }

    class PhysicalDevice {
        <<module>>
        +getPhysicalDevice(instance) VkPhysicalDevice
        +printPhysicalDevices(instance)
    }

    class Surface {
        <<module>>
        +getWindowSurface(instance, window) VkSurfaceKHR
    }

    class QueueFamilyIndices {
        <<struct>>
        +optional~uint32_t~ graphicsFamilyIndex
        +optional~uint32_t~ presentFamilyIndex
        +isComplete() bool
    }

    class QueueFamilies {
        <<module>>
        +findQueueFamilies(physicalDevice, surface) QueueFamilyIndices
    }

    class LogicalDeviceInfo {
        <<struct>>
        +VkDevice logicalDevice
        +VkQueue graphicsQueue
        +VkQueue presentQueue
    }

    class LogicalDevice {
        <<module>>
        +createLogicalDevice(physicalDevice, familyIndices) LogicalDeviceInfo
    }

    class SwapchainSupportDetails {
        <<struct, in progress>>
        +VkSurfaceCapabilitiesKHR capabilities
        +vector~VkSurfaceFormatKHR~ formats
        +vector~VkPresentModeKHR~ presentModes
        +isComplete() bool
    }

    class SwapchainSupport {
        <<module, in progress>>
        +getSwapchainSupportDetails(physicalDevice, surface) SwapchainSupportDetails
    }

    class Cleanup {
        <<module>>
        +cleanup(window, instance, messenger, device, surface)
    }

    main_cpp ..> WindowHandling : creates window
    main_cpp ..> VulkanInstance : creates instance
    main_cpp ..> DebugCallbackVulkan : creates messenger
    main_cpp ..> PhysicalDevice : selects GPU
    main_cpp ..> Surface : creates surface
    main_cpp ..> QueueFamilies : queries indices
    main_cpp ..> LogicalDevice : creates device
    main_cpp ..> SwapchainSupport : queries support
    main_cpp ..> Cleanup : tears down

    VulkanInstance ..> DebugCallbackVulkan : chains messenger info via pNext
    PhysicalDevice --> VulkanInstance : requires VkInstance
    Surface --> VulkanInstance : requires VkInstance
    Surface --> WindowHandling : requires GLFWwindow*
    QueueFamilies --> PhysicalDevice : requires VkPhysicalDevice
    QueueFamilies --> Surface : requires VkSurfaceKHR
    QueueFamilies ..> QueueFamilyIndices : produces
    LogicalDevice --> PhysicalDevice : requires VkPhysicalDevice
    LogicalDevice --> QueueFamilyIndices : requires
    LogicalDevice ..> LogicalDeviceInfo : produces
    SwapchainSupport --> PhysicalDevice : requires VkPhysicalDevice
    SwapchainSupport --> Surface : requires VkSurfaceKHR
    SwapchainSupport ..> SwapchainSupportDetails : produces
```

**Reading it:** everything ultimately traces back to `VulkanInstance` — no other
module can exist without a `VkInstance` first. `PhysicalDevice` and `Surface`
are the two "hubs" everything else after them depends on: both `QueueFamilies`
and `SwapchainSupport` need *both* a `VkPhysicalDevice` and a `VkSurfaceKHR`
together, since queue/swapchain support is a property of that specific
GPU-plus-window-system pairing, not either one alone.

---

## 2. Build-order flow (what `App` actually runs)

The structural diagram shows *relationships*; this shows *execution order* —
the literal top-to-bottom sequence `App`'s member-initializer list runs
through in `App::App` (`core/src/App.cpp`), followed by the `run()` loop.
`main.cpp` itself is now just `App app(...); app.run();` inside a
`try`/`catch`.

There are no more numbered `return N` failure codes or a manual `cleanup()`
call — every member is an RAII class (or a `static` helper that throws), so a
failure anywhere unwinds the stack, destructs whatever was already built (in
reverse order, for free, via normal C++ semantics), and lands in the single
`catch` block in `main.cpp`.

```mermaid
flowchart TD
    A["Window\n(createWindow)"] --> B["VulkanInstance\n(createInstance + debug messenger via pNext)"]
    B --> C["Surface\n(getWindowSurface)"]
    C --> D["pickPhysicalDevice\n(getPhysicalDevice)"]
    D --> E["pickQueueFamilies\n(findQueueFamilies)"]
    E --> F["LogicalDevice\n(createLogicalDevice)"]
    F --> G["pickSwapchainSupport\n(getSwapchainSupportDetails)"]
    G --> H["Swapchain\n(create swapchain + image views)"]
    H --> I["SyncObjects\n(image-available/render-complete semaphores + fence)"]
    I --> J["Pipeline\n(load .spv shaders, dynamic-rendering pipeline)"]
    J --> K["CommandBuffers\n(allocate + record: barrier -> vkCmdBeginRendering -> draw -> vkCmdEndRendering -> barrier)"]

    K --> L["App::run() render loop\n(glfwPollEvents)"]
    L --> M["drawFrame()\nwait fence -> acquire image -> submit -> present"]
    M -->|window open| L
    M -->|window closed| N[vkDeviceWaitIdle]
    N --> O["~App: RAII teardown\n(destructors fire in reverse declaration order)"]

    A -.->|throws| X["std::runtime_error\ncaught once in main()"]
    B -.->|throws| X
    C -.->|throws| X
    D -.->|throws| X
    E -.->|throws| X
    F -.->|throws| X
    G -.->|throws| X
    H -.->|throws| X
    I -.->|throws| X
    J -.->|throws| X
    K -.->|throws| X
    X --> P["stack unwinds: already-built members\ndestruct in reverse order"]
    P --> Q["print error, return 1"]

    style X fill:#5a1f1f,color:#fff
    style P fill:#5a1f1f,color:#fff
    style Q fill:#5a1f1f,color:#fff
```

**Reading it:** every step is still a strict prerequisite for the next — this
is a linear pipeline, not a tree — but the failure handling inverted: instead
of each step explicitly calling `cleanup()` with `VK_NULL_HANDLE` placeholders
for anything not yet created, each module's constructor is responsible only
for its own resource, and the compiler-generated stack unwinding guarantees
reverse-order teardown of everything that *did* get constructed. `App`'s
member declaration order in `App.hpp` (window, instance, surface,
physicalDevice, indices, device, support, swapchain, syncObjects, pipeline,
commandBuffers) *is* the build order, and `~App`'s implicit destructor runs
it in reverse — the same guarantee `cleanup.hpp` used to provide by hand.

---

## Legend

| Symbol | Meaning |
|---|---|
| `..>` | dependency — "uses this to produce something" |
| `-->` | association — "needs a handle owned by this" |
| `<<module>>` | a `.hpp`/`.cpp` file pair, one Vulkan concern each |
| `<<struct>>` | a plain data-holding type, no owned Vulkan lifetime logic |
| red flowchart node | the shared exception path — any construction step can `throw std::runtime_error`, caught once in `main()`, unwinding already-built RAII members in reverse |
| dashed arrow (`-.->`) | "may throw into" — every build step can fail into the single exception path |
