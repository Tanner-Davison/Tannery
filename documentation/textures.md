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
- No mipmaps (`maxLod = 0`); distant textures will shimmer. No anisotropy (needs the
  `samplerAnisotropy` device feature).
- If `immediateSubmit` throws inside the `Texture` constructor, the image leaks (the destructor does
  not run for a half-built object).

## References

- vulkan-tutorial: [Images](https://vulkan-tutorial.com/Texture_mapping/Images),
  [Image view and sampler](https://vulkan-tutorial.com/Texture_mapping/Image_view_and_sampler),
  [Combined image sampler](https://vulkan-tutorial.com/Texture_mapping/Combined_image_sampler)
- Spec: [vkCmdCopyBufferToImage](https://registry.khronos.org/vulkan/specs/latest/man/html/vkCmdCopyBufferToImage.html),
  [VkSamplerCreateInfo](https://registry.khronos.org/vulkan/specs/latest/man/html/VkSamplerCreateInfo.html),
  [VkDescriptorImageInfo](https://registry.khronos.org/vulkan/specs/latest/man/html/VkDescriptorImageInfo.html)
- Vulkan 3D Graphics Rendering Cookbook (2nd ed.): images and textures chapter
- stb_image: header comment in `/usr/include/stb/stb_image.h`
