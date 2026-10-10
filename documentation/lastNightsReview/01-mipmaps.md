# 1. Mipmaps: generation loop and per-level layouts

Files: `core/src/Texture.cpp`, `core/src/Sampler.cpp`, `core/include/Texture.hpp`
Longer reference: `documentation/textures.md`

## Big picture

A **mip level** is one image of the texture at half the width and height of the one before,
each texel being the average of a 2x2 block above it. The whole chain is a **mipmap**.

```
level 0: 8x8   original            ████████
level 1: 4x4   averaged copy       ████
level 2: 2x2                       ██
level 3: 1x1   average of all      █
```

Costs about 33% more memory (1 + 1/4 + 1/16 + ... = 4/3).

### Why

A texture is sampled per screen pixel. A far-away quad may cover 2x2 screen pixels but have 64
texels, so each pixel picks one spot and the spot changes as the camera moves: shimmer.

```
Without mips:  screen pixel -> 1 texel of a 64-texel image   (random one: flicker)
With mips:     screen pixel -> a level where 1 texel ~ 1 pixel, already averaged  (stable)
```

The GPU chooses the level itself: in the fragment shader it compares each pixel's UV with its
neighbours' UVs to see how many texels fit under one pixel.

### Where it fits

Texture work happens at **two different times**, in two different parts of the GPU:

```
LOAD TIME (once, Texture constructor)               DRAW TIME (every frame)
 PNG -> staging buffer -> copy into level 0          vertex shader -> rasterizer
 MIP GENERATION: level 0 -> 1 -> 2 -> 3 (transfer)        |
 every level ends in SHADER_READ_ONLY  ------------>  fragment shader: texture(texSampler, uv)
                                                      (picks a mip level here)
```

"Pipeline" is used in two senses here:

| Meaning | Where | Mip role |
|---|---|---|
| The **graphics pipeline** (`VkPipeline`: vertex -> rasterizer -> fragment) | `Pipeline.cpp`, `data/shaders/triangle.frag` | The **use** side: sampling happens in its fragment shader |
| **Pipeline stages** (`VK_PIPELINE_STAGE_*`) used in barriers | `Texture.cpp` barriers | The **build** side: copies/blits are `TRANSFER` stage work, run at load time inside `immediateSubmit` |

### What this replaced

| | Before | Now |
|---|---|---|
| Image levels | 1 | `mipLevels` (4 for the 8x8 checker) |
| Upload | copy, then one barrier to `SHADER_READ_ONLY` | copy, then the loop below |
| View `levelCount` | 1 | `mipLevels` |
| Sampler `maxLod` | `0` (locked to level 0) | `VK_LOD_CLAMP_NONE` |

## Steps

### Step 1: how many levels

```cpp
// Texture.cpp, in the constructor, right after the image size is known
// Halve until 1x1: e.g. 512x256 -> floor(log2(512)) + 1 = 10 levels
mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(size.width, size.height)))) + 1;
```

- `max(width, height)`: the chain stops when **both** sides reach 1, and the bigger side takes
  longest.
- `log2`: how many times you can halve that number to reach 1.
- `floor`: for non-powers of two (500 -> 8.97) only full halvings count.
- `+ 1`: the original image is level 0.
- Vulkan will not guess: the count is fixed at image creation.

### Step 2: create the image with every level, and allow it to be a blit source

```cpp
imageCreateInfo.mipLevels = mipLevels;
// TRANSFER_SRC too: each mip level is read as the source of the next level's blit
imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
                      | VK_IMAGE_USAGE_SAMPLED_BIT;
```

Usage flags are promises: `TRANSFER_SRC` is needed because every level except the last is the
source of a blit.

### Step 3: make every level a write target (barrier A)

```cpp
// A: UNDEFINED -> TRANSFER_DST, so the copy is allowed to write into the image
toTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
// aspect, baseMip, levelCount, baseLayer, layerCount: every level becomes a write target
toTransfer.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, mipLevels, 0, 1};
toTransfer.srcAccessMask    = 0;                             // nothing earlier to wait on
toTransfer.dstAccessMask    = VK_ACCESS_TRANSFER_WRITE_BIT;  // the copy writes
vkCmdPipelineBarrier(cmd,
                     VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,  // srcStageMask: wait on nothing
                     VK_PIPELINE_STAGE_TRANSFER_BIT,     // dstStageMask: the copy waits
                     0, 0, nullptr, 0, nullptr, 1, &toTransfer);
```

`levelCount` is `mipLevels`, so **all** levels start in `TRANSFER_DST`. Then
`vkCmdCopyBufferToImage` fills level 0 only.

### Step 4: the state each level goes through

```
        +-------- copy/blit writes into it --------+
        v                                           |
  TRANSFER_DST --(barrier 1)--> TRANSFER_SRC --(barrier 3)--> SHADER_READ_ONLY
   "write here"                 "read from here"                "final: shaders sample it"
                                     |
                             the blit reads it here (2)
```

Different levels of one image can be in **different layouts at the same time**. While level 0
is read to make level 1, level 1 is written. That is why every barrier here names **one level**
in `subresourceRange`.

### Step 5: per-pass setup

```cpp
// Size of the level we are reading from (level i-1) at the top of each pass
int32_t mipWidth  = static_cast<int32_t>(size.width);
int32_t mipHeight = static_cast<int32_t>(size.height);

// Shared by the per-level and last-level barriers; each use sets its own fields
VkImageMemoryBarrier barrier{};
barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
barrier.image               = image;
```

`int32_t` because `VkImageBlit` offsets are signed. One barrier struct is reused as a template:
the fields that never change are set once, and each use sets the rest.

### Step 6: the loop, one pass (`i = 1 .. mipLevels - 1`)

**6a. Barrier 1: level `i-1` becomes a read source**

```cpp
for (uint32_t i = 1; i < mipLevels; i++) {
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, i - 1, 1, 0, 1};
    barrier.oldLayout        = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;  // keeps pixels
    barrier.newLayout        = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcAccessMask    = VK_ACCESS_TRANSFER_WRITE_BIT;  // the copy/blit wrote it
    barrier.dstAccessMask    = VK_ACCESS_TRANSFER_READ_BIT;   // the blit reads it
    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);
```

Rule for any barrier: **"what write must finish, and what read must wait?"** Here a transfer
write (the copy, or the previous blit) must finish before a transfer read (the next blit).
`oldLayout` is the real current layout; `UNDEFINED` here would throw the pixels away.

**6b. The blit: level `i-1` into level `i` at half size**

```cpp
    // Blit level i-1 -> level i at half size (never below 1)
    const int32_t nextWidth  = mipWidth > 1 ? mipWidth / 2 : 1;
    const int32_t nextHeight = mipHeight > 1 ? mipHeight / 2 : 1;

    VkImageBlit blit{};
    blit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, i - 1, 0, 1};
    blit.srcOffsets[0]  = {0, 0, 0};
    blit.srcOffsets[1]  = {mipWidth, mipHeight, 1};
    blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, i, 0, 1};
    blit.dstOffsets[0]  = {0, 0, 0};
    blit.dstOffsets[1]  = {nextWidth, nextHeight, 1};

    vkCmdBlitImage(cmd,
                   image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   1, &blit,
                   VK_FILTER_LINEAR);
```

A **blit** copies a region and can **resize** it; `VK_FILTER_LINEAR` averages neighbours when
shrinking. A plain copy (`vkCmdCopyBufferToImage`) moves bytes 1:1 and cannot resize.
The two layouts passed in must match what the barriers set. `z` is 1 for 2D; the "never below 1"
rule handles non-square images (e.g. 512x256: the height reaches 1 at level 8, where the width is
still 2, so level 8 is 2x1 and level 9 is 1x1).

**6c. Barrier 3: level `i-1` is finished, hand it to the fragment shader**

```cpp
    barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;  // the blit read it
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;    // texture() will read it
    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);
```

Stages `TRANSFER -> FRAGMENT_SHADER` is the hand-off from the load-time work to the
graphics pipeline's fragment stage.

**6d. Level `i` becomes next pass's source**

```cpp
    mipWidth  = nextWidth;
    mipHeight = nextHeight;
}
```

### Step 7: the last level, after the loop

```cpp
// 5. The last level was only ever a blit destination, so it is still TRANSFER_DST
barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, mipLevels - 1, 1, 0, 1};
barrier.oldLayout        = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
barrier.newLayout        = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
barrier.srcAccessMask    = VK_ACCESS_TRANSFER_WRITE_BIT;  // the blit (or copy) wrote it
barrier.dstAccessMask    = VK_ACCESS_SHADER_READ_BIT;
vkCmdPipelineBarrier(cmd,
                     VK_PIPELINE_STAGE_TRANSFER_BIT,
                     VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                     0, 0, nullptr, 0, nullptr, 1, &barrier);
```

The loop only transitions level `i-1`, and the last level is never `i-1` of anything. It is the
easiest transition to forget. It also covers the 1x1 case (`mipLevels == 1`): the loop never
runs and level 0 gets this barrier.

### Step 8: view and sampler must agree

```cpp
// Texture.cpp: the view covers every level
viewCreateInfo.subresourceRange.levelCount = mipLevels;

// Sampler.cpp: trilinear blend between two levels, and don't cap the LOD range
samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
samplerInfo.minLod     = 0.0f;
samplerInfo.maxLod     = VK_LOD_CLAMP_NONE;
```

Four settings must agree: image `mipLevels`, view `levelCount`, barrier ranges, and sampler LOD
range. The sampler is shared by many textures with different level counts, so it doesn't
hard-code a limit; the image's own count is the limit.

## The ordering mistake (worth remembering)

An earlier draft had the `DST -> SRC` barrier **after** the blit and the last-level barrier
**inside** the loop. The correct order per pass is: barrier 1, blit, barrier 3, update sizes;
then the last-level barrier **once**, after the loop.

## Gotchas

- `oldLayout = UNDEFINED` discards the pixels. Name the real layout when the contents matter.
- Blit offsets are `int32_t`.
- `vkCmdBlitImage` needs the format to support linear blits (`R8G8B8A8_SRGB` does here). A
  portable engine should check `vkGetPhysicalDeviceFormatProperties`.
- A tiny test image barely shows the effect; a large photo shows it clearly.

## Check yourself

1. For a 512x256 image, how many levels are there, and what size is the last?
2. Level 2 is `TRANSFER_DST` and empty. The loop is on pass `i = 3`. What layout is level 2 in at
   the start of that pass, and what is the first thing the pass does to it?
3. Why does the loop transition one level at a time instead of the whole image once?
4. Why is step 7 (the last level) outside the loop?
5. If the sampler's `maxLod` were `0`, what would you see on a far-away quad, and why? (The
   image would still have all its levels.)
6. In which graphics-pipeline stage is the mip level actually selected?

<details>
<summary>Answer key</summary>

1. `floor(log2(512)) + 1 = 10` levels. The last is 1x1. (The height, being smaller, reaches 1
   first, at level 8, and stays at 1 while the width finishes halving: level 8 is 2x1.)
2. Level 2 was the destination of pass 2's blit, so it is in `TRANSFER_DST` and now has pixels.
   Pass 3's first action is barrier 1: `TRANSFER_DST -> TRANSFER_SRC` for level `i-1 = 2`, so it
   can be read by the blit into level 3.
3. Because different levels need different layouts at the same moment: one is being read (SRC)
   while another is written (DST). One whole-image transition can't express that.
4. The loop only transitions level `i-1`. The final level is never `i-1` of any pass, so it is
   still `TRANSFER_DST` when the loop ends and needs its own transition.
5. The GPU is locked to level 0, so a far-away quad still reads the full-resolution image and
   shimmers, exactly as if there were no mips. The levels exist but are never used.
6. The **fragment shader** stage: `texture(texSampler, fragUV)` in `triangle.frag`, using the
   UV differences between neighbouring pixels.

</details>

## Rewrite it yourself

Close the file, delete the loop body in `Texture.cpp` (keep a copy), and rewrite steps 6a-6d and 7
from the state diagram in step 4. The check: the textured quad still renders with no validation
errors, and it is stable when the camera moves back.
