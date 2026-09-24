# Vulkan Call Dictionary

A personal reference of every Vulkan (and Vulkan/GLFW bridge) call actually
used in this codebase so far, in alphabetical order. Pure GLFW window/input
calls (`glfwCreateWindow`, `glfwPollEvents`, etc.) are intentionally left out
— this is a Vulkan-specific reference, not a GLFW one.

This is a living document. New entries get appended (alphabetically) as new
Vulkan calls are introduced in later steps/milestones.

**Legacy entries:** `vkCreateRenderPass`, `vkDestroyRenderPass`,
`vkCreateFramebuffer`, `vkDestroyFramebuffer`, `vkCmdBeginRenderPass` and
`vkCmdEndRenderPass` describe the classic render pass model, which the
codebase no longer uses after the dynamic rendering refactor. They are kept
for reference; see `vkCmdBeginRendering`, `vkCmdEndRendering` and
`vkCmdPipelineBarrier` for what replaced them.

---

## glfwCreateWindowSurface

**Category:** WSI / Surface

**What it does:** GLFW's cross-platform bridge function that creates a
`VkSurfaceKHR` for a given `GLFWwindow`. Internally it calls the correct
platform-specific Vulkan extension function for you (e.g. a Wayland or X11
surface-creation call on Linux), so the app never has to branch on platform.

**Why it matters here:** This is the literal handshake between GLFW and
Vulkan. The `VkSurfaceKHR` it returns is the object every later WSI call
(surface support checks, capabilities, swapchain creation) operates on.

---

## vkAcquireNextImageKHR

**Category:** Render Loop / Swapchain

**What it does:** Asks the swapchain which presentable image index is
available to render into next. The image isn't necessarily usable the
instant this call returns — it signals a given semaphore (and/or fence)
once the image is truly ready, since the presentation engine may still be
finishing with it.

**Why it matters here:** The first real Vulkan call in `App::drawFrame()`,
passing `syncObjects.getImageAvailableSemaphore()`. The returned
`imageIndex` drives the rest of the frame — it selects the correct
pre-recorded `VkCommandBuffer` and the correct per-image render-complete
semaphore out of `SyncObjects`.

---

## vkAllocateCommandBuffers

**Category:** Command Buffers

**What it does:** Allocates one or more `VkCommandBuffer`s from an existing
`VkCommandPool`, described by a `VkCommandBufferAllocateInfo` (pool, level,
count).

**Why it matters here:** `CommandBuffers`'s constructor sizes
`commandBuffers` to match the framebuffer count and allocates all of them
in one batched call — one command buffer per swapchain image, since each
one records a draw targeting that image's own framebuffer.

---

## vkBeginCommandBuffer

**Category:** Command Buffers

**What it does:** Puts a `VkCommandBuffer` into the recording state,
described by a `VkCommandBufferBeginInfo` (usage flags, inheritance info
for secondary buffers).

**Why it matters here:** `CommandBuffers`'s constructor calls this once per
command buffer, in the per-index loop, before recording the fixed draw
sequence into it. Since this triangle is static, that recording happens
once at construction — not every frame.

---

## vkCmdBeginRendering

**Category:** Command Buffer Recording

**What it does:** Records the start of a dynamic rendering instance (core
in Vulkan 1.3), via `VkRenderingInfo` — the render area, layer count, and an
array of `VkRenderingAttachmentInfo` structs (pointer plus count) naming the
image view, layout, `loadOp`/`storeOp` and clear value for each color
attachment. No `VkRenderPass` or `VkFramebuffer` object is involved.

**Why it matters here:** Replaced `vkCmdBeginRenderPass` in
`CommandBuffers`'s recording loop. Each buffer builds its own
`VkRenderingAttachmentInfo` pointing at that frame's swapchain image view
(`pImageViews[i]`), with `CLEAR`/`STORE` and the opaque-black clear color —
the same choices the old `RenderPass` attachment description held, now set
at record time. It does NOT transition image layouts; that is what the two
`vkCmdPipelineBarrier` calls around it are for.

---

## vkCmdBeginRenderPass

**Category:** Command Buffer Recording

**What it does:** Records the start of a render pass instance into a
command buffer, via `VkRenderPassBeginInfo` — which render pass, which
framebuffer, the render area, and the clear values for any
`LOAD_OP_CLEAR` attachments.

**Why it matters here:** Binds that frame's target `VkFramebuffer`
(`frameBuffers[i]`) to `RenderPass`'s single color attachment slot, and
supplies the opaque-black `clearColor` that fills the screen before the
triangle is drawn.

---

## vkCmdBindPipeline

**Category:** Command Buffer Recording

**What it does:** Records a command binding a `VkPipeline` to a bind point
(`VK_PIPELINE_BIND_POINT_GRAPHICS` here) for subsequent draw commands in
the same command buffer.

**Why it matters here:** Binds the one `Pipeline` built in `Pipeline.cpp`
(shader stages plus every fixed-function state struct) so the
`vkCmdDraw` right after it knows which pipeline to execute.

---

## vkCmdDraw

**Category:** Command Buffer Recording

**What it does:** Records a non-indexed draw call — vertex count, instance
count, first vertex, first instance.

**Why it matters here:** `CommandBuffers`'s constructor records
`vkCmdDraw(commandBuffers[i], 3, 1, 0, 0)` — three hardcoded vertices, one
instance. There's no vertex buffer at all; `triangle.vert` generates its
own positions from `gl_VertexIndex`, matching the empty
`VkPipelineVertexInputStateCreateInfo` in `Pipeline`.

---

## vkCmdEndRendering

**Category:** Command Buffer Recording

**What it does:** Records the end of the current dynamic rendering
instance, resolving the attachments' store operations. Takes only the
command buffer — there is no render pass object to name.

**Why it matters here:** Closes the scope opened by `vkCmdBeginRendering`
in `CommandBuffers`. Unlike `vkCmdEndRenderPass`, it does not move the
image to a final layout — the second `vkCmdPipelineBarrier` right after it
does that (`COLOR_ATTACHMENT_OPTIMAL` to `PRESENT_SRC_KHR`).

---

## vkCmdEndRenderPass

**Category:** Command Buffer Recording

**What it does:** Records the end of the current render pass instance,
resolving any attachment store operations (`VK_ATTACHMENT_STORE_OP_STORE`
here) and transitioning the attachment to its `finalLayout`.

**Why it matters here:** Closes out the render pass opened by
`vkCmdBeginRenderPass`, triggering the color attachment's transition to
`VK_IMAGE_LAYOUT_PRESENT_SRC_KHR` as configured in `RenderPass` — the
layout the swapchain needs for presentation.

---

## vkCmdPipelineBarrier

**Category:** Command Buffer Recording / Synchronization

**What it does:** Records an execution and memory dependency into a command
buffer: work in the source pipeline stage(s) must finish, and its memory
access be made visible, before work in the destination stage(s) begins. Takes
arrays of global, buffer and image barriers; an image barrier
(`VkImageMemoryBarrier`) can also transition the image's layout
(`oldLayout` to `newLayout`) as part of the same dependency.

**Why it matters here:** Dynamic rendering does not transition image layouts
for you, so `CommandBuffers` records two barriers per swapchain image.
Before rendering: `UNDEFINED` to `COLOR_ATTACHMENT_OPTIMAL`, with source and
destination stage `COLOR_ATTACHMENT_OUTPUT` (the stage `drawFrame`'s
`vkQueueSubmit` waits on the image-available semaphore at) and destination
access `COLOR_ATTACHMENT_WRITE`. After rendering: `COLOR_ATTACHMENT_OPTIMAL`
to `PRESENT_SRC_KHR`, from `COLOR_ATTACHMENT_OUTPUT` to `BOTTOM_OF_PIPE`.
These replace the `initialLayout`/`finalLayout` and `VkSubpassDependency`
that the old `RenderPass` handled implicitly.

---

## vkCreateCommandPool

**Category:** Command Buffers

**What it does:** Creates a `VkCommandPool` — the allocator that
`VkCommandBuffer`s are drawn from — tied to one queue family, via
`VkCommandPoolCreateInfo` (queue family index, flags).

**Why it matters here:** `CommandBuffers`'s constructor creates one pool
tied to the graphics queue family, with
`VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT` set (allows individual
buffers to be reset/re-recorded later, even though this milestone's static
triangle doesn't currently re-record any).

---

## vkCreateDebugUtilsMessengerEXT

**Category:** Validation / Debugging (extension function, loaded dynamically)

**What it does:** Creates a `VkDebugUtilsMessengerEXT` object that routes
validation layer output (errors, warnings, info messages) into a callback
function you provide, instead of leaving it to print wherever the driver
defaults to.

**Why it matters here:** This is the real mechanism behind every
"Validation layer: ..." message printed to `stderr` from `debugCallback`.
It's an `EXT` function, so it isn't statically linked — it has to be looked
up at runtime via `vkGetInstanceProcAddr` (see the proxy pattern below).
Called from `VulkanInstance`'s constructor.

---

## vkCreateDevice

**Category:** Logical Device

**What it does:** Creates a `VkDevice` — the actual working handle to the
GPU — built from a chosen `VkPhysicalDevice`, a list of queues to create
(`VkDeviceQueueCreateInfo`), and the features/extensions you want enabled.

**Why it matters here:** The physical device is just a read-only hardware
description. Almost every future Vulkan call in this project (buffers,
pipelines, command submission) goes through the logical device, not the
physical one — this is the call that produces it.

---

## vkCreateFence

**Category:** Synchronization

**What it does:** Creates a `VkFence`, a CPU-observable synchronization
primitive signaled by the GPU when submitted work completes — the only
sync primitive `vkWaitForFences` can wait on from host code.

**Why it matters here:** `SyncObjects`'s constructor creates one fence with
`VK_FENCE_CREATE_SIGNALED_BIT` set, so the very first `vkWaitForFences`
call in `App::drawFrame()` doesn't block forever waiting for a "previous
frame" that never happened.

---

## vkCreateFramebuffer

**Category:** Framebuffers

**What it does:** Creates a `VkFramebuffer` binding a specific set of
`VkImageView`s to a `VkRenderPass`'s attachment slots, along with
width/height/layer count, via `VkFramebufferCreateInfo`.

**Why it matters here:** `FrameBuffers`'s constructor creates one
framebuffer per swapchain image view, each bound to the single
`RenderPass` built earlier — this is what makes the abstract render pass
concrete for a specific swapchain image.

---

## vkCreateGraphicsPipelines

**Category:** Graphics Pipeline

**What it does:** Creates one or more `VkPipeline` objects in a single
batched call, from an array of `VkGraphicsPipelineCreateInfo` structs
describing every fixed-function stage plus the programmable shader stages,
pipeline layout, and render pass/subpass.

**Why it matters here:** `Pipeline`'s constructor creates exactly one
pipeline (the second argument, `VK_NULL_HANDLE`, opts out of pipeline
caching), assembling the vertex/fragment shader stages plus every
fixed-function state struct (vertex input, input assembly, viewport,
rasterizer, multisampling, color blending) built earlier in the
constructor.

---

## vkCreateImageView

**Category:** Swapchain / Image Views

**What it does:** Creates a `VkImageView`, a typed "lens" onto a
`VkImage`'s memory describing how to interpret it (view type, format,
component swizzling, and the subresource range of mip levels/array layers
it exposes).

**Why it matters here:** `Swapchain`'s constructor creates one image view
per swapchain image, since `VkImage` objects can't be bound directly as
render targets or read by shaders — a view is always required. Folded
directly into `Swapchain` rather than a separate wrapper class, since these
views share the swapchain's exact lifetime.

---

## vkCreateInstance

**Category:** Instance & Setup

**What it does:** Creates the `VkInstance` — the connection between the
application and the Vulkan loader/library. Every other Vulkan object is
either created through this instance or is a child of it.

**Why it matters here:** Literally the first Vulkan object created in
`main.cpp`. It's also where instance-level extensions (debug utils, the
GLFW-required WSI extensions) and validation layers get enabled up front.

---

## vkCreatePipelineLayout

**Category:** Graphics Pipeline

**What it does:** Creates a `VkPipelineLayout` describing the descriptor
set layouts and push constant ranges a pipeline can access — the interface
between shaders and any external resources.

**Why it matters here:** `Pipeline`'s constructor creates a layout with
zero descriptor sets (`setLayoutCount = 0`), since the hardcoded triangle
shader reads no external resources at all — everything it needs comes from
`gl_VertexIndex`.

---

## vkCreateRenderPass

**Category:** Render Pass

**What it does:** Creates a `VkRenderPass` from a `VkRenderPassCreateInfo`
describing its attachments (`VkAttachmentDescription`), subpasses
(`VkSubpassDescription`), and the dependencies between them
(`VkSubpassDependency`).

**Why it matters here:** `RenderPass`'s constructor builds the one color
attachment (load/store ops, initial/final layout), the one subpass
referencing it, and a `VkSubpassDependency` synchronizing against
`VK_SUBPASS_EXTERNAL` at the color-attachment-output stage — the render
pass every `FrameBuffers` and `CommandBuffers` recording depends on.

---

## vkCreateSemaphore

**Category:** Synchronization

**What it does:** Creates a `VkSemaphore`, a GPU-side synchronization
primitive used to order work between queue operations (one signals it,
another waits on it) without any CPU involvement.

**Why it matters here:** `SyncObjects`'s constructor creates the single
`imageAvailableSemaphore` plus one `renderCompleteSemaphore` per swapchain
image — the per-image design specifically fixes a real validation error
hit this session (`VUID-vkQueueSubmit-pSignalSemaphores-00067`) caused by
reusing one shared render-complete semaphore across different acquired
images.

---

## vkCreateShaderModule

**Category:** Shaders

**What it does:** Creates a `VkShaderModule` wrapping raw SPIR-V bytecode,
described by a `VkShaderModuleCreateInfo` (code size and a pointer to the
bytecode).

**Why it matters here:** `createShaderModule()` in `shaderModule.cpp` is
called once for the vertex shader and once for the fragment shader in
`Pipeline`'s constructor, immediately after `readFile()` loads each
compiled `.spv` file from disk.

---

## vkCreateSwapchainKHR

**Category:** Swapchain / WSI

**What it does:** Creates a `VkSwapchainKHR` — the ring of presentable
`VkImage`s the GPU renders into and hands off to the window system — from a
`VkSwapchainCreateInfoKHR` describing image count, format, color space,
extent, usage, sharing mode, transform, and present mode.

**Why it matters here:** This is where the three chosen settings
(`chooseSwapSurfaceFormat`, `chooseSwapPresentMode`, `chooseSwapExtent`) and
the queried `SwapchainSupport` actually become a real GPU object.
`createSwapchain()` in `swapchainSupport.cpp` also branches
`VK_SHARING_MODE_CONCURRENT` vs. `VK_SHARING_MODE_EXCLUSIVE` here, depending
on whether the graphics and present queue families differ.

---

## vkDestroyCommandPool

**Category:** Command Buffers

**What it does:** Destroys a `VkCommandPool` and implicitly frees every
`VkCommandBuffer` allocated from it — individual command buffers don't need
a separate destroy call.

**Why it matters here:** `CommandBuffers::~CommandBuffers()` only needs
this one call. It's also the exact call a shutdown-ordering bug this
session was flagged against — the validation layer refuses to destroy a
pool whose buffers are still "in use" by a queue with unfinished work.

---

## vkDestroyDebugUtilsMessengerEXT

**Category:** Validation / Debugging (extension function, loaded dynamically)

**What it does:** Destroys the debug messenger created above.

**Why it matters here:** Must be destroyed before `vkDestroyInstance`,
since it's a child of the instance. `VulkanInstance::~VulkanInstance()`
destroys it first, then the instance itself, in that order within the same
destructor. In the overall app-teardown sequence this no longer runs
first, though — `App`'s member declaration order means `Swapchain`,
`LogicalDevice`, and `Surface` are all torn down before `VulkanInstance`
is, since they're declared after it and C++ destroys members in reverse
declaration order.

---

## vkDestroyDevice

**Category:** Logical Device

**What it does:** Destroys the `VkDevice` and implicitly any queues
obtained from it via `vkGetDeviceQueue`.

**Why it matters here:** Must run before `vkDestroyInstance` (the device is
a child of the instance), and before `vkDestroySurfaceKHR` if a swapchain
built from that surface is still referencing it.

---

## vkDestroyFence

**Category:** Synchronization

**What it does:** Destroys a `VkFence`.

**Why it matters here:** `SyncObjects::~SyncObjects()` destroys the single
fence. Must not run while the fence is still associated with an unfinished
`vkQueueSubmit` — the reason `App::run()` added a `vkDeviceWaitIdle` call
before any of `App`'s members (including `syncObjects`) tear down.

---

## vkDestroyFramebuffer

**Category:** Framebuffers

**What it does:** Destroys a `VkFramebuffer`.

**Why it matters here:** `FrameBuffers::~FrameBuffers()` loops over every
framebuffer it created (one per swapchain image) and destroys each
individually, since `vkCreateFramebuffer` has no batched destroy
equivalent.

---

## vkDestroyImageView

**Category:** Swapchain / Image Views

**What it does:** Destroys a `VkImageView`.

**Why it matters here:** `Swapchain::~Swapchain()` destroys every image
view it created before destroying the swapchain itself — image views
aren't implicitly freed when their parent `VkImage` (owned by the
swapchain) is destroyed.

---

## vkDestroyInstance

**Category:** Instance & Setup

**What it does:** Destroys the `VkInstance` and frees Vulkan's internal
state for it.

**Why it matters here:** Must be the LAST real Vulkan destroy call in
cleanup — every other handle (device, surface, messenger) is a child of
the instance, and destroying a parent before its children is undefined
behavior that validation layers will flag.

---

## vkDestroyPipeline

**Category:** Graphics Pipeline

**What it does:** Destroys a `VkPipeline`.

**Why it matters here:** `Pipeline::~Pipeline()` destroys the pipeline
before the pipeline layout, mirroring their creation order in reverse.

---

## vkDestroyPipelineLayout

**Category:** Graphics Pipeline

**What it does:** Destroys a `VkPipelineLayout`.

**Why it matters here:** Called second in `Pipeline::~Pipeline()`, after
`vkDestroyPipeline` — the layout is the kind of object other pipelines
could in principle still reference, so it's freed only after the one
pipeline using it here is already gone.

---

## vkDestroyRenderPass

**Category:** Render Pass

**What it does:** Destroys a `VkRenderPass`.

**Why it matters here:** `RenderPass::~RenderPass()` destroys it — must
happen after `FrameBuffers` and `CommandBuffers` (both reference it) are
torn down, which `App`'s member declaration order (`renderPass` declared
before `frameBuffers` and `commandBuffers`) guarantees automatically via
reverse-order destruction.

---

## vkDestroySemaphore

**Category:** Synchronization

**What it does:** Destroys a `VkSemaphore`. The spec requires that every
submitted batch referencing it has completed execution first.

**Why it matters here:** `SyncObjects::~SyncObjects()` destroys
`imageAvailableSemaphore` and every entry in `renderCompleteSemaphores`.
Also used inside the local `SemaphoreModuleGuard` in `SyncObjects.cpp` — a
constructor-scoped RAII guard that cleans up any semaphores already
created if a later step (like fence creation) throws, mirroring the
`ShaderModuleGuard` pattern from `Pipeline`.

---

## vkDestroyShaderModule

**Category:** Shaders

**What it does:** Destroys a `VkShaderModule`.

**Why it matters here:** Shader modules are only needed at
pipeline-creation time — once `vkCreateGraphicsPipelines` has consumed
them, they can be freed immediately rather than kept alive for the
pipeline's lifetime. `ShaderModuleGuard` (a local RAII struct in
`Pipeline.cpp`) calls this in its destructor for both the vertex and
fragment modules, right when each one goes out of scope at the end of
`Pipeline`'s constructor.

---

## vkDestroySurfaceKHR

**Category:** WSI / Surface

**What it does:** Destroys a `VkSurfaceKHR` previously created via
`glfwCreateWindowSurface`.

**Why it matters here:** The surface is a child of the instance, not the
device, so it must be destroyed before `vkDestroyInstance` — and after
anything (like a future swapchain) that was built from it.

---

## vkDestroySwapchainKHR

**Category:** Swapchain / WSI

**What it does:** Destroys a `VkSwapchainKHR` and the `VkImage`s it owns —
but *not* any `VkImageView`s created from those images, which have to be
destroyed separately.

**Why it matters here:** The swapchain is a child of the logical device, so
it must be destroyed before `vkDestroyDevice`. `Swapchain::~Swapchain()`
handles this, and it doesn't need to know anything about that ordering
requirement explicitly — `App` declares `swapchain` as its last member, so
C++ destroys it first, before `device` (a `LogicalDevice`), automatically.

---

## vkDeviceWaitIdle

**Category:** Synchronization

**What it does:** Blocks the calling thread until every queue on the given
`VkDevice` has finished all submitted work.

**Why it matters here:** Called once, in `App::run()`, right after the main
loop exits and before the function returns — not per-frame. Fixes a
shutdown-ordering validation error where `App`'s member destructors
(`SyncObjects`, `CommandBuffers`, etc.) started destroying GPU-owned
objects the last submitted frame was still using. Deliberately not used
inside `drawFrame()` itself, where it would serialize CPU and GPU work
every frame and defeat the purpose of the per-frame `VkFence`.

---

## vkEndCommandBuffer

**Category:** Command Buffers

**What it does:** Ends the recording of a `VkCommandBuffer`, finalizing it
for submission.

**Why it matters here:** Closes out each command buffer's one-time
recording in `CommandBuffers`'s constructor, right after
`vkCmdEndRenderPass` — after this call the buffer is ready to be submitted
via `vkQueueSubmit` any number of times without needing to re-record,
which is exactly how `App::drawFrame()` uses it every frame.

---

## vkEnumerateDeviceExtensionProperties

**Category:** Logical Device / Extensions

**What it does:** Two-call pattern function listing the extensions a given
`VkPhysicalDevice` actually supports, as an array of `VkExtensionProperties`
(each holding an `extensionName` and `specVersion`).

**Why it matters here:** Device extensions like `VK_KHR_swapchain` must be
verified as supported *before* being requested in `VkDeviceCreateInfo` —
requesting one that isn't reported here makes `vkCreateDevice` fail with
`VK_ERROR_EXTENSION_NOT_PRESENT`. Matched against each desired extension
name via `strcmp`.

---

## vkEnumeratePhysicalDevices

**Category:** Physical Device

**What it does:** Two-call pattern function (query count, then fill array)
that lists the GPUs Vulkan can see on the system as `VkPhysicalDevice`
handles.

**Why it matters here:** This is how the RTX 3090 gets discovered as a
usable device — you never hardcode a device, you always ask the driver
what's available and select from that list.

---

## vkGetDeviceQueue

**Category:** Logical Device / Queues

**What it does:** Retrieves a `VkQueue` handle for a given queue family
index + queue index from an already-created `VkDevice`. Doesn't create
anything new — the queue already exists as a side effect of
`vkCreateDevice`; this just hands you a handle to it.

**Why it matters here:** This is how `graphicsQueue` and `presentQueue` get
populated in `LogicalDeviceInfo` — actual `VkQueue` handles are needed
later to submit command buffers and to present images.

---

## vkGetInstanceProcAddr

**Category:** Extension Loading

**What it does:** Looks up the function pointer for a Vulkan command by
name, scoped to a specific `VkInstance`. Required for any extension
function (anything ending in `EXT`, or a vendor suffix) because those
aren't statically linked — the loader has no compile-time knowledge of them.

**Why it matters here:** This is exactly how `CreateDebugUtilsMessengerEXT`
and `DestroyDebugUtilsMessengerEXT` in `debugCallbackVulkan.hpp` fetch the
real `vkCreateDebugUtilsMessengerEXT` / `vkDestroyDebugUtilsMessengerEXT`
functions at runtime — a pattern required for every `VK_EXT_*` call.

---

## vkGetPhysicalDeviceProperties2

**Category:** Physical Device

**What it does:** Fills a `VkPhysicalDeviceProperties2` struct describing
the queried device — name, vendor/device ID, API version, driver version,
device type, and limits.

**Why it matters here:** Used to print diagnostic info confirming which GPU
got selected and that it's genuinely the 3090 talking real Vulkan (not a
translation layer like Mesa Dozen).

---

## vkGetPhysicalDeviceQueueFamilyProperties

**Category:** Queue Families

**What it does:** Two-call pattern function returning an array of
`VkQueueFamilyProperties`, one per queue family the device exposes,
describing what operations (graphics, compute, transfer, sparse binding)
each family supports and how many queues it has.

**Why it matters here:** Vulkan has no single "the GPU queue" — hardware
groups queues into families with different capabilities. This is the call
that finds which family index supports `VK_QUEUE_GRAPHICS_BIT`.

---

## vkGetPhysicalDeviceSurfaceCapabilitiesKHR

**Category:** Swapchain / WSI

**What it does:** Fills a `VkSurfaceCapabilitiesKHR` struct describing what
a given surface supports on a given physical device: min/max image count,
min/max image extent, supported transforms, and more. Not a two-call
pattern — there's always exactly one capabilities struct, no array.

**Why it matters here:** Any swapchain built later must fit inside these
bounds or `vkCreateSwapchainKHR` will fail outright. This is the first of
the three swapchain-support queries.

---

## vkGetPhysicalDeviceSurfaceFormatsKHR

**Category:** Swapchain / WSI

**What it does:** Two-call pattern function (query count, then fill array)
returning the list of `(VkFormat, VkColorSpaceKHR)` pairs a given surface
can actually present on a given physical device.

**Why it matters here:** You don't get to request an arbitrary pixel format
for the swapchain — you pick one from whatever this call reports the
surface actually supports. Populates `SwapchainSupport::formats`.

---

## vkGetPhysicalDeviceSurfacePresentModesKHR

**Category:** Swapchain / WSI

**What it does:** Two-call pattern function returning the list of
`VkPresentModeKHR` values (`FIFO`, `MAILBOX`, `IMMEDIATE`, `FIFO_RELAXED`) a
surface supports for handing finished images to the display.

**Why it matters here:** Present mode is what actually controls
vsync/tearing behavior later. `VK_PRESENT_MODE_FIFO_KHR` is the only mode
the spec *guarantees* exists everywhere — anything else must be confirmed
present in this list before it's requested. Populates
`SwapchainSupport::presentationModes`.

---

## vkGetPhysicalDeviceSurfaceSupportKHR

**Category:** Queue Families / WSI

**What it does:** Given a queue family index and a `VkSurfaceKHR`, reports
whether that family can present images to that specific surface.

**Why it matters here:** Graphics-capable and present-capable are NOT
guaranteed to be the same queue family. This is the call that finds the
present family index, searched independently from the graphics family.

---

## vkGetSwapchainImagesKHR

**Category:** Swapchain / WSI

**What it does:** Two-call pattern function (query count, then fill array)
retrieving the `VkImage`s actually owned by a `VkSwapchainKHR`.

**Why it matters here:** `Swapchain`'s constructor calls this right after
`vkCreateSwapchainKHR` to discover how many images the driver actually
allocated (which can differ from the requested `minImageCount`) and to get
handles to them. This `imageCount` is also what `SyncObjects` uses to size
its per-image render-complete semaphores.

---

## vkQueuePresentKHR

**Category:** Render Loop / Swapchain

**What it does:** Hands a rendered image back to the presentation engine to
display, described by a `VkPresentInfoKHR` (which swapchain(s), which image
index(es), and which semaphore(s) to wait on first).

**Why it matters here:** The last call in `App::drawFrame()`, waiting on
that frame's per-image render-complete semaphore so the presentation
engine doesn't display a partially-rendered image — that semaphore is
signaled once `vkQueueSubmit`'s draw work finishes.

---

## vkQueueSubmit

**Category:** Render Loop / Command Submission

**What it does:** Submits one or more command buffers to a queue for
execution, described by a `VkSubmitInfo` (wait semaphores plus the
pipeline stage(s) to wait at, the command buffers to run, signal
semaphores, and an optional fence).

**Why it matters here:** `App::drawFrame()` submits that frame's
pre-recorded `VkCommandBuffer` (indexed by `imageIndex`) to the graphics
queue, waiting on `imageAvailableSemaphore` at
`VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT`, signaling that image's
render-complete semaphore, and passing the frame's `VkFence` so the CPU can
later confirm this work finished via `vkWaitForFences`.

---

## vkResetFences

**Category:** Synchronization

**What it does:** Resets one or more fences back to the unsignaled state.
Must be called manually — `vkWaitForFences` never does this for you.

**Why it matters here:** `App::drawFrame()` resets the fence immediately
after `vkWaitForFences` confirms the previous frame's work is done, and
before resubmitting — resetting too early (before the wait) risks
deadlocking on a fence that's already been consumed.

---

## vkWaitForFences

**Category:** Synchronization

**What it does:** Blocks the calling thread until one or more fences are
signaled, or a timeout elapses.

**Why it matters here:** The first call in `App::drawFrame()` every frame —
waits on the previous frame's fence before reusing its command buffer and
sync objects, which is what prevents the CPU from racing ahead of the GPU
and overwriting resources still in use.
