# 10. `Pipeline`, `CommandBuffers` and `SyncObjects`: one frame from three angles

Files: `core/include/Pipeline.hpp`, `core/src/Pipeline.cpp`,
`core/include/CommandBuffers.hpp`, `core/src/CommandBuffers.cpp`,
`core/include/SyncObjects.hpp`, `core/src/SyncObjects.cpp`,
and the frame sequence in `Renderer::drawFrame` (`core/src/Renderer.cpp`)

Older notes on pieces of this: `documentation/frames-in-flight.md`,
`documentation/swapchain-recreation.md`, `documentation/depth-buffer.md`.

---

## Part 1: The big picture

Three objects, three different questions:

| Object | Answers | One-line job |
|---|---|---|
| **`Pipeline`** | **HOW** to draw | A baked, immutable bundle of GPU state: shaders, vertex layout, rasterizer, depth, blending, formats |
| **`CommandBuffers`** | **WHAT** to do | A list of GPU commands, recorded by the CPU now, executed by the GPU later |
| **`SyncObjects`** | **WHEN** | Fences and semaphores that order work between the CPU, the GPU, and the display |

```
        built once (Renderer constructor)                     done every frame
 +----------------------------------------+      +-----------------------------------------------+
 | Pipeline        "how"   (VkPipeline)   |      | wait fence  -> acquire image                  |
 | SyncObjects     "when"  (fences,       |----->| record CommandBuffers (uses Pipeline)         |
 |                          semaphores)    |      | submit (waits/signals SyncObjects) -> present |
 | CommandBuffers  "what"  (pool + N bufs)|      +-----------------------------------------------+
 +----------------------------------------+
```

The key idea behind all three: **Vulkan is asynchronous.** Nothing you call "does" the work right
away. You *describe* work (pipeline, commands), *submit* it, and the GPU runs it later, on its own
schedule. Synchronization is how you stay correct anyway.

All three are created in `Renderer` (`core/src/Renderer.cpp`), in this order, which is also the
dependency order:

```cpp
, syncObjects(std::make_unique<SyncObjects>(ctx.deviceHandle(), swapchain->imageCountHandle()))
...
, pipeline(ctx.deviceHandle(), swapchain->extentHandle(), vertPath, fragPath,
           swapchain->formatHandle().format,
           std::array{descriptors.layoutHandle(), mat.layoutHandle()},   // set 0, set 1
           depthImage->getDepthFormat())
, gpuProfiler(ctx)
, commandBuffers(ctx.deviceHandle(), ctx.queueFamilies(), SyncObjects::MAX_FRAMES_IN_FLIGHT, gpuProfiler.handle())
```

---

## Part 2: `Pipeline`

### What a `VkPipeline` is, and why Vulkan makes you build one

In older APIs (OpenGL) you flip state on and off between draws ("enable depth test", "set blend
mode") and the driver figures out the consequences at draw time, which causes surprise stalls.
Vulkan makes you declare **all** of that state **up front** in one object. The driver compiles it
(together with the shaders) into final GPU code once, at creation. Binding it later is cheap.

The price: **changing any baked state means a different pipeline.** That's why you'll eventually
have many pipelines (one per combination of shaders and states), and why a few things are made
**dynamic** so they *don't* need one.

### The constructor, in the order it builds things

`Pipeline.cpp` fills in about ten small structs and then hands them all to one call. Here they are
in order, with what your current setting is and what you'd change it for.

| # | Struct | Your setting | What it controls / when you'd change it |
|---|---|---|---|
| 1 | `VkPipelineShaderStageCreateInfo` x2 | vertex + fragment, entry `"main"` | Which shader code runs (see file 9). A new algorithm = a new pipeline |
| 2 | `VkPipelineVertexInputStateCreateInfo` | from `Vertex::getBindingDescription/getAttributeDescription` | How vertex bytes map to shader `in` locations. New vertex fields = edit `Vertex.hpp` |
| 3 | `VkPipelineInputAssemblyStateCreateInfo` | `TRIANGLE_LIST` | How vertices form shapes: lines, strips, points |
| 4 | `VkPipelineViewportStateCreateInfo` | 1 viewport, 1 scissor (values overridden, see dynamic state) | The screen rectangle |
| 5 | `VkPipelineRasterizationStateCreateInfo` | `FILL`, `cullMode = NONE`, `frontFace = CLOCKWISE` | Wireframe, back-face culling, which winding is "front" |
| 6 | `VkPipelineMultisampleStateCreateInfo` | 1 sample | MSAA anti-aliasing |
| 7 | `VkPipelineDepthStencilStateCreateInfo` | depth test + write on, compare `LESS` | Occlusion: nearer wins. Transparent objects and skyboxes change this |
| 8 | `VkPipelineColorBlendStateCreateInfo` | blending off, write RGBA | Transparency (alpha blending) |
| 9 | `VkPipelineLayoutCreateInfo` | the two descriptor-set layouts | What resources the shaders can read (see below) |
| 10 | `VkPipelineRenderingCreateInfo` (in `pNext`) | color format = swapchain format, depth format | Which **attachment formats** this pipeline draws into |
| 11 | `VkPipelineDynamicStateCreateInfo` | `VIEWPORT`, `SCISSOR` | What is **not** baked |

then:

```cpp
VkGraphicsPipelineCreateInfo pipelineInfo{};
pipelineInfo.pNext               = &pipelineRenderingInfo;   // dynamic rendering: formats, no render pass
pipelineInfo.stageCount          = 2;
pipelineInfo.pStages             = shaderStages;
pipelineInfo.pVertexInputState   = &vertexInputInfo;
... one pointer per struct above ...
pipelineInfo.layout              = this->pipelineLayout;
pipelineInfo.renderPass          = nullptr;                  // no VkRenderPass: we use dynamic rendering
vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &this->pipeline);
```

### The pipeline layout: the shader's "plug types"

```cpp
VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(pSetLayouts.size());
pipelineLayoutInfo.pSetLayouts    = pSetLayouts.data();
vkCreatePipelineLayout(this->device, &pipelineLayoutInfo, nullptr, &this->pipelineLayout);
```

The layout says "this pipeline's shaders expect descriptor set 0 shaped like *this*, set 1 shaped
like *that*." The **array index is the set number**: `{frameLayout, materialLayout}` makes the
camera set 0 and the texture set 1 (matches `set = 1` in `triangle.frag`). Later it also declares
**push constant ranges**. The layout is separate from the pipeline because the *same layout* is
what you use in `vkCmdBindDescriptorSets` when drawing.

### Dynamic rendering (no render pass)

Older Vulkan baked a `VkRenderPass` into the pipeline. You use **dynamic rendering** instead
(`vkCmdBeginRendering`), so the pipeline only needs the attachment **formats**:

```cpp
pipelineRenderingInfo.colorAttachmentCount    = 1;
pipelineRenderingInfo.pColorAttachmentFormats = &colorFormat;
pipelineRenderingInfo.depthAttachmentFormat   = pDepthFormat;
pipelineRenderingInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;
```

**Contract:** the formats here must equal the formats of the image views passed to
`vkCmdBeginRendering` in `CommandBuffers::record`. A mismatch is a validation error.

### Dynamic state: why resizing the window doesn't rebuild the pipeline

```cpp
VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
```

These two are set **per command buffer** instead (`vkCmdSetViewport` / `vkCmdSetScissor` in
`record()`), so the values given in the constructor's `viewport`/`scissor` structs are ignored.
That is why `Renderer::recreateSwapchain` rebuilds the swapchain, depth image and sync objects but
**not** the pipeline: nothing baked into it depends on the window size.

### What does and does not need a new pipeline

| Change | New pipeline? |
|---|---|
| Different shader, vertex layout, cull mode, blend mode, depth settings, topology, attachment format | **Yes** |
| Window size (viewport, scissor) | No (dynamic) |
| A different texture or uniform contents | No (descriptor sets) |
| A different mesh | No (just bind other buffers) |
| A different transform or color | No (uniform or push constant) |

### Lifetime and cleanup

```cpp
struct ShaderModuleGuard { ... ~ShaderModuleGuard() { vkDestroyShaderModule(device, module, nullptr); } };
```

The `VkShaderModule`s are only needed **while building** the pipeline (the pipeline keeps its own
compiled copy), so the guard destroys them at the end of the constructor, even if it throws
halfway. `~Pipeline` destroys the `VkPipeline` and the `VkPipelineLayout`.

### Gotchas

- Pipeline attachment formats must match what you render into.
- The descriptor-set layout **order** is the set number.
- `cullMode = NONE` hides winding mistakes. When you turn culling on, the Y flip in the projection
  (file 2) matters: it reverses winding, and `frontFace` has to agree.
- Pipelines are expensive to create. Create them at load time, never mid-frame.

---

## Part 3: `CommandBuffers`

### What a command buffer is

The CPU doesn't draw. It **records** commands (`vkCmdBindPipeline`, `vkCmdDrawIndexed`, ...) into a
command buffer. Nothing runs yet. Later, `vkQueueSubmit` hands the finished buffer to the GPU,
which executes it. Recording and executing are two different moments, which is the whole reason
synchronization exists.

```
CPU (record)                             GPU (execute)
 vkBeginCommandBuffer ...                 ...later...
 vkCmdBindPipeline                        runs the recorded list,
 vkCmdDrawIndexed          --submit-->    in order
 vkEndCommandBuffer
```

### The pool, and why there are N buffers

```cpp
commandPoolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
commandPoolInfo.queueFamilyIndex = indices.graphicsFamilyIndex.value();
vkCreateCommandPool(...);
// then
allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
allocInfo.commandBufferCount = frameCount;       // MAX_FRAMES_IN_FLIGHT
vkAllocateCommandBuffers(...);
```

- A **command pool** is the memory manager for command buffers, tied to one **queue family**
  (graphics). Buffers allocated from it can only be submitted to queues of that family.
- `RESET_COMMAND_BUFFER_BIT` lets each buffer be reset individually, which `record()` does every
  frame.
- There is **one buffer per frame slot** (not per swapchain image), because a buffer can't be
  re-recorded while the GPU might still be running it. The slot's fence (Part 4) proves it's done.

### `record()`, step by step

```cpp
VkCommandBuffer cmd = commandBuffers.at(frameIndex);
check(vkResetCommandBuffer(cmd, 0), "vkResetCommandBuffer");        // safe: the fence said the GPU is done

beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;      // re-recorded every frame
check(vkBeginCommandBuffer(cmd, &beginInfo), "vkBeginCommandBuffer");
```

**1. Prepare the swapchain image for drawing** (a barrier):

```cpp
barrierBefore.oldLayout     = VK_IMAGE_LAYOUT_UNDEFINED;
barrierBefore.newLayout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
barrierBefore.srcAccessMask = 0;
barrierBefore.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
vkCmdPipelineBarrier(cmd,
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,    // src
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,    // dst
    ...);
```

`UNDEFINED` is right here because the image is going to be **cleared**, so its old contents don't
matter. **Why the stages are `COLOR_ATTACHMENT_OUTPUT`:** the submit tells the GPU to wait for the
"image available" semaphore at that **same stage** (Part 4). The barrier's source stage matches, so
the layout change happens **after** the semaphore wait. If they didn't line up, the transition
could run while the display still owns the image.

**2. Same for the depth image** (`UNDEFINED -> DEPTH_ATTACHMENT_OPTIMAL`, stages
`EARLY/LATE_FRAGMENT_TESTS`).

**3. Describe the attachments and start rendering:**

```cpp
colorAttachment.loadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR;      // clear to black at the start
colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;     // keep it: it gets presented
depthAttachment.loadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR;      // depth starts at 1.0 (far)
depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // depth isn't needed after the frame
vkCmdBeginRendering(cmd, &renderingInfo);
```

`loadOp`/`storeOp` tell the GPU what to do with attachment memory at the start and end of the
pass; `DONT_CARE` lets it skip work.

**4. Bind everything the draw needs, then draw:**

```cpp
vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);                  // HOW
VkDescriptorSet sets[] = {frameSet, materialSet};                                    // set 0, set 1
vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 2, sets, 0, nullptr);
vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);                           // the data
vkCmdBindIndexBuffer(cmd, mesh.indexBufferHandle(), 0, VK_INDEX_TYPE_UINT16);
vkCmdSetViewport(cmd, 0, 1, &viewport);                                              // the dynamic state
vkCmdSetScissor(cmd, 0, 1, &scissor);
vkCmdDrawIndexed(cmd, mesh.indexCount(), 1, 0, 0, 0);
vkCmdEndRendering(cmd);
```

`vkCmdBindDescriptorSets(..., firstSet = 0, count = 2, sets, ...)`: array index plus `firstSet` is the
set number, matching the pipeline layout's order. `vkCmdDrawIndexed(indexCount, instanceCount = 1,
firstIndex, vertexOffset, firstInstance)`.

**5. Hand the image to the display** (a barrier):

```cpp
barrierAfter.oldLayout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
barrierAfter.newLayout     = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
barrierAfter.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;   // drawing wrote it
barrierAfter.dstAccessMask = 0;                                       // the presenter reads via the semaphore
vkCmdPipelineBarrier(cmd, COLOR_ATTACHMENT_OUTPUT_BIT, BOTTOM_OF_PIPE_BIT, ...);
```

**6. Profiling and end:**

```cpp
PROFILE_GPU_COLLECT(profileCtx, cmd);                      // file 7
check(vkEndCommandBuffer(cmd), "vkEndCommandBuffer");
return cmd;
```

(The GPU zones from file 7 wrap steps 1 to 5 in blocks.)

### The swapchain image's layout over one frame

```
UNDEFINED --barrier 1--> COLOR_ATTACHMENT_OPTIMAL --(draw)--> --barrier 5--> PRESENT_SRC_KHR --> display
```

It's the same idea as the mip levels (file 1): an image moves through layouts, and each transition
is a barrier that says what must finish before what.

### Gotchas

- Everything recorded into the buffer is read when it **executes**, so anything it points at
  (vertex buffers, descriptor sets, the swapchain image) must still be alive then.
- A command buffer must not be reset or re-recorded while the GPU may still be executing it. In
  this engine the slot's fence guarantees that.
- `ONE_TIME_SUBMIT` is a promise: "submitted once between resets".
- `record()` returns the handle, and `drawFrame` passes it to `vkQueueSubmit`.

---

## Part 4: `SyncObjects`

### The problem

The CPU, the GPU and the display engine all run **at the same time**, at different speeds. Vulkan
does not order any of that for you. Three tools exist, each for a different gap:

| Tool | Orders... | Who waits | Used here for |
|---|---|---|---|
| **Fence** | GPU work -> the **CPU** | The CPU (`vkWaitForFences`) | "The GPU finished this frame slot's last frame" |
| **Semaphore** | GPU work -> other GPU work (including the display) | The GPU (in `vkQueueSubmit` / `vkQueuePresentKHR`) | "Image is ready to draw into" and "drawing is done, you can present" |
| **Barrier** | commands **inside** one command buffer | The GPU, stage to stage | Layout transitions (files 1 and Part 3) |

### What `SyncObjects` creates

```cpp
static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;

imagesAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);   // indexed by currentFrame
renderCompleteSemaphores.resize(pImageCount);             // indexed by imageIndex
fences.resize(MAX_FRAMES_IN_FLIGHT);                      // indexed by currentFrame
fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;           // created already signaled
```

| Object | Count | Index | Why that index |
|---|---|---|---|
| fence | `MAX_FRAMES_IN_FLIGHT` | `currentFrame` | one per frame slot: it says "this slot's previous GPU work is done" |
| image-available semaphore | `MAX_FRAMES_IN_FLIGHT` | `currentFrame` | the acquire for this slot signals it, and this slot's fence proves the last use is over |
| render-complete semaphore | one **per swapchain image** | `imageIndex` | the display holds it until that image can be reused; only the image index tells us that |

**The two-index rule** (from `frames-in-flight.md`): `currentFrame` (you choose it) indexes things
whose reuse you can prove with **your own fence**. `imageIndex` (the driver returns it from
`vkAcquireNextImageKHR`) indexes things tied to a **specific swapchain image**. Render-complete
semaphores are per image because the presentation engine, not your fence, decides when a given
image's semaphore is free again.

### Why the fence starts signaled

The first thing each frame does is wait on the slot's fence. If it started unsignaled, the very
first frame would wait forever, because no earlier GPU work exists to signal it.

### Why `MAX_FRAMES_IN_FLIGHT` is 3

It bounds how many frames the CPU may run ahead of the GPU. More slots absorb hiccups and keep the
GPU fed; fewer give lower latency. The same constant sizes three things in the engine:

- the fences and image-available semaphores,
- the per-frame uniform buffers and descriptor sets (`FrameDescriptors`),
- the command buffers (`CommandBuffers`).

(The older `frames-in-flight.md` says the value is 2; the code is now 3.)

### Construction safety (a guard, with one gap)

```cpp
SemaphoreModuleGuard semGuard(this->device);
... create semaphore ... semGuard.push_back(semaphore);     // registered immediately
... create fences ...
semGuard.release();                                          // success: this class owns them now
```

If the constructor **throws**, the destructor of the half-built `SyncObjects` does **not** run, so
without the guard the semaphores created so far would leak. The guard destroys them on the way
out; on success `release()` hands ownership to the class.

**Gap worth knowing:** the guard covers semaphores only. If `vkCreateFence` throws partway through
the fence loop, the fences already created (and nothing else) leak. The same pattern would extend
to fences.

### Rebuilding

`Renderer::recreateSwapchain` resets and rebuilds `SyncObjects`, because the number of
render-complete semaphores follows the swapchain image count, which can change. It first calls
`context.waitIdle()`, so no semaphore is destroyed while the GPU or the display is using it.

---

## Part 5: All three, in one frame

`Renderer::drawFrame` is where they meet. In order, with which object does what:

```cpp
// 1. SyncObjects: has the GPU finished this slot's last frame?
vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);              // fence[currentFrame]

// 2. Ask the swapchain for an image. SyncObjects: the semaphore it will signal
vkAcquireNextImageKHR(device, swapchain, UINT64_MAX,
                      syncObjects->getImageAvailableSemaphore(currentFrame), VK_NULL_HANDLE, &imageIndex);
vkResetFences(device, 1, &fence);                                     // only now that a submit is certain

// 3. CommandBuffers (using the Pipeline): record this frame's work
VkCommandBuffer cmd = commandBuffers.record(currentFrame, image[imageIndex], ..., pipeline.pipelineHandle(), ...);

// 4. Submit, with SyncObjects wired in
submitInfo.pWaitSemaphores   = &imageAvailable;                       // wait: image is ready ...
submitInfo.pWaitDstStageMask = &COLOR_ATTACHMENT_OUTPUT;              // ... but only at the stage that writes it
submitInfo.pSignalSemaphores = &renderComplete[imageIndex];           // signal: drawing is done
vkQueueSubmit(graphicsQueue, 1, &submitInfo, fence);                  // fence[currentFrame] signals when all done

// 5. Present: wait for drawing to finish
presentInfo.pWaitSemaphores = &renderComplete[imageIndex];
vkQueuePresentKHR(presentQueue, &presentInfo);

currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
```

### Who waits for whom

```
CPU:   wait fence[f] -> acquire -> record cmd[f] -> submit -> present -> (next frame) ...
                            |                          |         |
                            | signals                  | waits   | waits
                            v                          v         v
                   imageAvailable[f] ----------> (GPU runs cmd[f]) ----> renderComplete[img] ----> display
                                                       |
                                                       | signals when finished
                                                       v
                                                  fence[f]  --> CPU's next wait on slot f
```

### Three frames in flight

```
slot 0:  [record+submit]========GPU========|                      fence[0] signals ->  reused here
slot 1:        [record+submit]========GPU========|
slot 2:              [record+submit]========GPU========|
         CPU can be recording slot 2 while the GPU is still drawing slots 0 and 1
```

### Why each detail is the way it is

| Detail | Reason |
|---|---|
| `vkResetFences` **after** a successful acquire | If acquire returns `OUT_OF_DATE` and we return early (to rebuild the swapchain), no submit happens. A fence reset beforehand would stay unsignaled and the next frame's wait would hang forever. |
| Wait stage `COLOR_ATTACHMENT_OUTPUT`, not `TOP_OF_PIPE` | Vertex work can run before the image is available; only writing color has to wait. A later stage lets the GPU overlap more work. |
| The submit's **wait stage** equals the barrier's **source stage** (Part 3, step 1) | The semaphore wait and the layout transition then chain correctly. |
| Present waits on `renderComplete[imageIndex]` | The display must not read the image until drawing is finished. |
| Fence passed to **`vkQueueSubmit`** | The GPU signals it when that submit's work completes: this is the CPU's only way to know. |

### How this appears in the profiler (file 7)

`wait for frame fence` is the CPU waiting for step 1; `acquire swapchain image` is step 2 (where
the vsync wait showed up); `record commands` is step 3; `queue submit` / `queue present` are steps
4 and 5; `gpu frame` is the GPU executing step 3's buffer.

---

## Part 6: How these grow

| Today | What it grows into |
|---|---|
| One pipeline | **Many pipelines** (per shader x state combination). Store them in a material system and build them at load time. A **pipeline cache** (`VkPipelineCache`) saves compile time between runs. |
| Pipeline built in `Renderer` | A pipeline **manager** keyed by state; rebuild on **shader hot reload** |
| One primary command buffer per frame | Several buffers per frame; **secondary** command buffers; **recording on multiple threads** (one pool per thread, since pools are not thread-safe; Taskflow is already in `deps/`) |
| Hand-written barriers (files 1 and 3) | **`VK_KHR_synchronization2`** (core in 1.3: `vkCmdPipelineBarrier2`, clearer stage/access pairs) and eventually a **render graph** that derives barriers automatically from pass dependencies |
| Fences + binary semaphores | **Timeline semaphores** (Vulkan 1.2): one counter-based object that replaces several fences and semaphores and works for CPU waits too |
| One graphics queue | Separate **transfer** and **compute** queues (async asset upload, GPU culling), with semaphores between them |
| `immediateSubmit` (CPU waits for each upload) | Async transfers on their own queue, signalled by a semaphore |

`★ Insight ─────────────────────────────────────`
- The three objects map onto the three questions every GPU API has to answer: **what state** (Pipeline), **what work** (CommandBuffers), **what order** (SyncObjects). New engine features almost always grow one of these three, and a feature that "doesn't work" is usually one of them being out of step with the others (a layout mismatch, a format mismatch, or a missing wait).
- The two-index rule (`currentFrame` versus `imageIndex`) is the most common place to make a subtle error in a Vulkan renderer: it validates fine, then breaks only after a window resize or under a different present mode.
`─────────────────────────────────────────────────`

---

## Check yourself

1. Name three things that force a new `VkPipeline`, and three that do not.
2. Why doesn't `recreateSwapchain` rebuild the pipeline?
3. What is the relationship between the array passed to `Pipeline` as `pSetLayouts` and `set = 1`
   in `triangle.frag`?
4. Why is there one command buffer per **frame slot** and not per swapchain image?
5. In `record()`, why do the first barrier's stages and the submit's `waitDstStageMask` use the same
   stage?
6. Which synchronization object is indexed by `imageIndex`, which by `currentFrame`, and why do
   they differ?
7. Why are fences created with `VK_FENCE_CREATE_SIGNALED_BIT`?
8. Why is `vkResetFences` called **after** a successful acquire rather than at the top of the frame?
9. What happens to the semaphores if `SyncObjects`' constructor throws on the fifth semaphore, and
   which objects does the guard *not* cover?
10. A frame's `wait for frame fence` zone suddenly takes 5 ms while `acquire swapchain image` takes
    0.1 ms. What does that tell you, compared with last night's capture?

<details>
<summary>Answer key</summary>

1. Forces a new pipeline: a different shader, a different cull/blend/depth setting, a different
   vertex layout or topology, a different attachment format. Does not: window size (dynamic
   viewport/scissor), a different texture or uniform value, a different mesh.
2. Nothing baked into it depends on the window size: viewport and scissor are dynamic, and the
   attachment *formats* don't change on resize.
3. The array index is the set number: `{frameLayout, materialLayout}` makes the material layout
   set 1, which is what the shader's `layout(set = 1, ...)` refers to.
4. A command buffer can't be reset while the GPU may still be executing it. The per-slot fence
   proves that slot's last submission finished, so the buffer is free to re-record; a swapchain
   image index gives no such proof.
5. The submit makes the GPU wait for the image-available semaphore at
   `COLOR_ATTACHMENT_OUTPUT`; the barrier's source stage is the same, so the layout transition
   happens after that wait and not before the image is really available.
6. Render-complete semaphores are indexed by `imageIndex` (the display holds them per image, and
   only the driver-chosen image index says which one); fences, image-available semaphores and
   command buffers are indexed by `currentFrame` (you can prove their reuse with your own fence).
7. The first frame waits on the fence before anything has ever been submitted. If it started
   unsignaled, that wait would never finish.
8. If acquire fails with `OUT_OF_DATE` and the function returns to rebuild the swapchain, no
   submit follows, so the fence would stay unsignaled and the next frame's wait would hang.
9. The guard destroys the semaphores created so far (the destructor of a half-built object does
   not run). It does not cover fences: fences already created before a failing `vkCreateFence`
   would leak.
10. A long fence wait means the CPU is waiting for the GPU to finish earlier work: the GPU is the
    bottleneck. Last night it was the opposite (tiny fence wait, long acquire), meaning vsync was
    pacing the loop.

</details>

## Exercises

1. **Predict, then observe.** Change `MAX_FRAMES_IN_FLIGHT` from 3 to 1 in `SyncObjects.hpp`.
   Before running, write down what you expect to happen to `wait for frame fence` and the frame
   time in Tracy, and why. Then rebuild and look. Change it back.
2. **Winding and culling.** Set `cullMode = VK_CULL_MODE_BACK_BIT` in `Pipeline.cpp`. One side of
   each quad will disappear. Explain why (the Y flip in the projection reverses winding), then fix
   it by flipping `frontFace`, and say which face is now the "front".
3. **Trace one frame.** On paper, draw the timeline for three consecutive frames with
   `MAX_FRAMES_IN_FLIGHT = 3`, marking when each fence is signaled, waited on and reset, and which
   command buffer each frame uses.
