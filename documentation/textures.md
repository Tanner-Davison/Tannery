# Textures

A texture is a `VkImage` of pixel data that the fragment shader reads with `texture(sampler, uv)`.
It replaces flat per-vertex color with image detail.

## The pipeline, end to end

```
PNG on disk ─stbi_load─► RGBA8 bytes (CPU) ─memcpy─► staging Buffer
   ─barrier A─► image in TRANSFER_DST ─vkCmdCopyBufferToImage─► pixels in image
   ─barrier B─► SHADER_READ_ONLY ─► ImageView + Sampler ─► descriptor (set 1, binding 0)
   ─► fragment shader: texture(texSampler, fragUV)
```

## What was implemented

- **UVs** (`Vertex::uv`, location 2, `VK_FORMAT_R32G32_SFLOAT`): a 0..1 address into the image per
  vertex. Interpolated across the triangle like `color`. Vulkan's image origin is the top-left; world
  +Y is up on screen (the projection flips Y), so `v = 0` is placed on the top vertices.
- **`stb_image.cpp`**: the one translation unit with `STB_IMAGE_IMPLEMENTATION` (same pattern as
  `vma.cpp`). `libstb-dev` via apt, public domain.
- **`GraphicsContext::immediateSubmit(fn)`**: temporary pool + one-time command buffer, runs `fn(cmd)`,
  submits, waits. Replaced the old `copyBuffer.cpp`; `createDeviceLocalBuffer` and `Texture` both use it.
- **`Texture`** (RAII): `loadPixels` → staging buffer → `vmaCreateImage` (`R8G8B8A8_SRGB`,
  `TRANSFER_DST | SAMPLED`) → upload (barrier, copy, barrier) → image view (`ASPECT_COLOR`).
- **`Sampler`** (RAII, shared): `LINEAR` filters, `REPEAT` addressing, anisotropy off, LOD 0..0.
- **`MaterialDescriptors`** (set 1): layout with one `COMBINED_IMAGE_SAMPLER` binding (fragment stage),
  a pool of one set, written with `VkDescriptorImageInfo{sampler, view, SHADER_READ_ONLY_OPTIMAL}`.
- **`Pipeline`** takes `std::span<const VkDescriptorSetLayout>` (index = set number);
  **`CommandBuffers::record`** binds sets 0 and 1 in one call.
- **`App`** owns `Texture`, `Sampler`, `MaterialDescriptors` (declared after `Mesh`, before
  `Renderer`); `TEXTURE_DIR` compile definition points at `data/textures/`.
- **Test image** `data/textures/uv_checker.png`: 8×8 checker, corners red (top-left), green
  (top-right), blue (bottom-left), yellow (bottom-right), so flips/rotations are obvious.

## Key ideas

- **Layouts**: the GPU stores images tiled; a layout says what the next job needs. Upload goes
  `UNDEFINED → TRANSFER_DST_OPTIMAL → SHADER_READ_ONLY_OPTIMAL`.
- **Barriers are hand-offs**: src = what must finish, dst = what waits. A's dst equals B's src
  (`TRANSFER` / `TRANSFER_WRITE`).

  | | src stage / access | dst stage / access |
  |---|---|---|
  | A | `TOP_OF_PIPE` / `0` | `TRANSFER` / `TRANSFER_WRITE` |
  | B | `TRANSFER` / `TRANSFER_WRITE` | `FRAGMENT_SHADER` / `SHADER_READ` |

- **Sampler ≠ image**: a sampler is reading rules (filter, wrap, LOD) with no pixels. Few samplers,
  many textures.
- **Descriptor sets by update frequency**: set 0 per frame (camera), set 1 per material (textures).
  Draw loop: bind set 0 once, then per object bind its set 1 and draw.
- **`_SRGB` format**: PNGs are gamma-encoded; sampling an `_SRGB` image returns linear values.
- `vkCmdCopyBufferToImage` un-flattens row-major bytes into the tiled image;
  `bufferRowLength = 0` means rows are tightly packed.

## Mipmaps

A **mip level** is a half-size, pre-averaged copy of the image. The chain runs from level 0 (the
original) down to 1x1. The GPU picks the level where one texel is about one screen pixel, so distant
surfaces read a stable average instead of flickering between texels.

```
512x256 -> 256x128 -> 128x64 -> ... -> 2x1 -> 1x1     (10 levels; level 0 = original)
```

- **Count:** `mipLevels = floor(log2(max(w, h))) + 1`. The `+ 1` is the original image. Vulkan will not
  guess; the count is fixed at `vkCreateImage` time. 8x8 checker = 4 levels. Memory cost is about +33%.
- **Four settings must agree:** image `mipLevels`, view `levelCount`, barrier `levelCount`/`baseMipLevel`,
  and the sampler LOD range (`maxLod = VK_LOD_CLAMP_NONE` so one shared sampler fits every texture).
- **Image usage** gains `TRANSFER_SRC`: each level is the blit source for the next.
- **Sampler:** `mipmapMode = LINEAR` blends between the two nearest levels (trilinear).

### Generation (inside `immediateSubmit`, after the base copy)

Different levels of one image can sit in different layouts at the same time. After the copy, every
level is `TRANSFER_DST` but only level 0 has pixels.

```
for i = 1 .. mipLevels-1:
    1. barrier  level i-1 : TRANSFER_DST -> TRANSFER_SRC     (write target -> read source)
    2. blit     level i-1 -> level i, destination half size (never below 1), VK_FILTER_LINEAR
    3. barrier  level i-1 : TRANSFER_SRC -> SHADER_READ_ONLY (finished for good)
    4. mipWidth/mipHeight = next sizes
after the loop, once:
    5. barrier  level mipLevels-1 : TRANSFER_DST -> SHADER_READ_ONLY   (was only ever a destination)
```

| Barrier | oldLayout -> newLayout | srcAccess -> dstAccess | stages |
|---|---|---|---|
| 1 | `DST -> SRC` | `TRANSFER_WRITE -> TRANSFER_READ` | `TRANSFER -> TRANSFER` |
| 3 | `SRC -> SHADER_READ_ONLY` | `TRANSFER_READ -> SHADER_READ` | `TRANSFER -> FRAGMENT_SHADER` |
| 5 | `DST -> SHADER_READ_ONLY` | `TRANSFER_WRITE -> SHADER_READ` | `TRANSFER -> FRAGMENT_SHADER` |

Barrier A (`UNDEFINED -> DST`) now uses `levelCount = mipLevels` so every level starts as a write
target. The old barrier B is replaced by steps 3 and 5.

Verified by moving the camera back (`lookAt` z from 2 to 8): the spinning checker stays stable instead
of crawling.

### Mip mistakes worth remembering

- `oldLayout = UNDEFINED` **discards the pixels**; name the real layout when the contents matter.
- Barrier order matters: the `DST -> SRC` barrier goes **before** the blit, not after.
- The last-level barrier goes **after** the loop (once), not inside it.
- Offsets in `VkImageBlit` are signed `int32_t`, and `z` extent is 1 for 2D.

## Mistakes worth remembering

- A trailing comma in a CMake source list becomes part of the filename.
- Statements (function calls) at file scope do not compile; only declarations live there.
- `path.filename()` drops the directory; use the full `path.string().c_str()`.
- `operator=(const T&&)` does not delete the real move assignment; it is `operator=(T&&)`.
- `deps/minlog` was a different, unfinished library (MuAlphaOmegaEpsilon/MinLog, README says GPL)
  than the Cookbook's `corporateshark/minilog` (MIT). Removed.

## Known limitations (future work)

- The material's set layout lives inside `MaterialDescriptors`; with many materials the layout should
  be owned once (shared) and each material only allocates a set.
- No anisotropy (needs the `samplerAnisotropy` device feature); oblique surfaces still look a bit
  blurry with plain trilinear filtering.
- `vkCmdBlitImage` requires the format to support linear blit (`FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR`
  with optimal tiling). `R8G8B8A8_SRGB` does on this GPU; a portable engine should check
  `vkGetPhysicalDeviceFormatProperties` first.
- If `immediateSubmit` throws inside the `Texture` constructor, the image leaks (the destructor does
  not run for a half-built object).

## References

- vulkan-tutorial: [Images](https://vulkan-tutorial.com/Texture_mapping/Images),
  [Image view and sampler](https://vulkan-tutorial.com/Texture_mapping/Image_view_and_sampler),
  [Combined image sampler](https://vulkan-tutorial.com/Texture_mapping/Combined_image_sampler)
- Mipmaps: [vulkan-tutorial: Generating Mipmaps](https://vulkan-tutorial.com/Generating_Mipmaps),
  [LearnOpenGL: Textures (Mipmaps)](https://learnopengl.com/Getting-started/Textures),
  [Wikipedia: Mipmap](https://en.wikipedia.org/wiki/Mipmap),
  [vkCmdBlitImage](https://registry.khronos.org/vulkan/specs/latest/man/html/vkCmdBlitImage.html)
- Spec: [vkCmdCopyBufferToImage](https://registry.khronos.org/vulkan/specs/latest/man/html/vkCmdCopyBufferToImage.html),
  [VkSamplerCreateInfo](https://registry.khronos.org/vulkan/specs/latest/man/html/VkSamplerCreateInfo.html),
  [VkDescriptorImageInfo](https://registry.khronos.org/vulkan/specs/latest/man/html/VkDescriptorImageInfo.html)
- Vulkan 3D Graphics Rendering Cookbook (2nd ed.): images and textures chapter
- stb_image: header comment in `/usr/include/stb/stb_image.h`
