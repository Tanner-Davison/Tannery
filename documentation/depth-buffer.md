# Depth Buffer

**Status:** COMPLETE (roadmap step 5). Depth image, depth test, per-frame clear and layout transition, rebuilt on resize; clean under validation.

## Overview

A depth buffer is an extra image, the same size as the screen, storing for each pixel the distance of
the closest thing drawn there so far. A new pixel is only drawn if it is closer than what is stored, so
the result is correct regardless of draw order. Depth runs from 0.0 (nearest) to 1.0 (farthest).

## What was implemented

- `DepthImage` (RAII): `vmaCreateImage` (`VK_FORMAT_D32_SFLOAT`, 2D, `DEPTH_STENCIL_ATTACHMENT` usage,
  dedicated memory) plus an image view with `VK_IMAGE_ASPECT_DEPTH_BIT`; the view is destroyed before the
  image. The format lives in one `static constexpr` shared by the image and the view.
- `Pipeline`: takes the depth format (`VkPipelineRenderingCreateInfo::depthAttachmentFormat`) and a
  `VkPipelineDepthStencilStateCreateInfo` (test on, write on, `VK_COMPARE_OP_LESS`).
- `CommandBuffers::record`: takes the depth image and view; a barrier `UNDEFINED ->
  DEPTH_ATTACHMENT_OPTIMAL` (early and late fragment tests stages) before `vkCmdBeginRendering`; a depth
  `VkRenderingAttachmentInfo` with `LOAD_OP_CLEAR`, `STORE_OP_DONT_CARE`, clear value 1.0.
- `Renderer`: owns the `DepthImage` as a `unique_ptr`; created from the swapchain extent, rebuilt in
  `recreateSwapchain()` after the new swapchain exists (it needs the new extent).

## Key ideas

- Depth is a `VkImage`, not a `VkBuffer`: a 2D grid with a format, usable only through an image view.
- Clear to 1.0 (farthest) so the first thing drawn at a pixel always passes.
- `storeOp` DONT_CARE for scratch data nobody reads after the frame; STORE if a later pass (for example
  a shadow map) reads it.
- Test and write are separate switches: a see-through object wants test on and write off.
- A barrier struct does nothing until it is passed to `vkCmdPipelineBarrier`.
- The depth image must match the swapchain extent, so it is rebuilt on resize.

## Mistakes worth remembering

- `imageCreateInfo.format` left unset (the format was only set on the view).
- Getters declared but never defined (link error), and an unrelated `VkSurfaceKHR` member where a format
  getter belonged.
- `VkImageSubresourceRange` brace lists: the field order is
  `{aspectMask, baseMipLevel, levelCount, baseArrayLayer, layerCount}`; a later whole-struct assignment
  silently overwrote the aspect and produced zero counts.
- Building a barrier struct without ever recording `vkCmdPipelineBarrier`.
- `depthAttachmentFormat` left at `VK_FORMAT_UNDEFINED` while a depth test was enabled.

## Next

Make the vertex position 3D (`vec3`) and draw geometry at different depths to see the depth test working,
then textures (a second descriptor type), model loading, and lighting.
