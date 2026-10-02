# Index Buffers

**Status:** COMPLETE (roadmap step 4a). Renders an indexed quad (4 vertices, 6 indices), clean under validation.

## Overview

An index buffer stores numbers that say "use vertex N", so shared vertices are stored once.
A quad is two triangles but only 4 distinct corners: `indices = {0, 1, 2, 2, 3, 0}`.
Every mesh loaded later is a vertex buffer plus an index buffer.

## What was implemented

- `createDeviceLocalBuffer(...)`: one shared staging-and-copy helper (data pointer, size, usage flags;
  adds `TRANSFER_DST` itself) used by both `createVertexBuffer` and `createIndexBuffer`.
- `createIndexBuffer`: `uint16_t` indices, usage `VK_BUFFER_USAGE_INDEX_BUFFER_BIT`.
- `indexBuffer` member after `vertexBuffer`; its handle and index count are passed at both
  `CommandBuffers` build sites (constructor and `recreateSwapchain`).
- `CommandBuffers` records `vkCmdBindIndexBuffer(..., VK_INDEX_TYPE_UINT16)` and
  `vkCmdDrawIndexed(indexCount, 1, 0, 0, 0)`.

## Mistakes worth remembering

- Passing the index buffer's **size in bytes** (12) as the index **count** (6): the draw read past the
  buffer. The picture looked right; only validation reported it. Always run with validation.
- Refactoring into a helper: move the code, then re-point every reference at the helper's
  parameters (`vertices` -> `pData`, hardcoded usage -> `pUsage`).

## Possible follow-ups

- `uint16_t` appears in three places that must agree: the index data, `VK_INDEX_TYPE_UINT16`, and
  the `sizeof(uint16_t)` used to derive the count. Consider carrying the index count with the buffer.
- `uint32_t` indices once a mesh exceeds 65,536 vertices.
