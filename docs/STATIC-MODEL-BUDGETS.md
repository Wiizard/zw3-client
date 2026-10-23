# x64 static-model render budgets

The static-model lighting cache, model-material streams, and render worker queues
are independent of address-space size and of the existing skinned/index buffer
patches. Raising map cull distances alone can increase pressure on all three.

`RendererBudgets.hpp` extends these paths for the supported x64 2.0.13 engine:

- The native lighting-atlas initializer gets a minimum of 8,192 static slots.
  The actual capacity still follows the native power-of-two atlas calculation:
  14,336 / 12,288 / 10,240 static slots for tested configuration values 1 / 2 / 3.
  The live client uses value 1. Every free-handle, owner, timestamp and model-bitset access is relocated
  with its backing array. Handles/model indices remain 16-bit. Native lighting,
  cache invalidation and the GPU atlas size calculation are preserved.
- The dependent model-lighting patch list grows from 4,096 to 24,576 entries
  per native frame buffer. Both producers and the backend upload handoff use
  relocated storage, with the original `0x13D700` frame spacing. The 52-byte
  entries fit within each separate allocation slice; the frame structures,
  atomic counter, reset and upload loop remain unchanged. Both bounds and all
  three storage references are in the same atomic patch group as the lighting
  cache. Raising only the cache had left this list stock and caused
  `modelLightingPatchList ran out of elements` when more models were lit.
- Worker types 9 and 11 each get 4,096 commands (previously 256), with matching
  98,304 / 131,072 byte allocations. Payload strides, atomics and overload
  fallback execution are unchanged.
- Model command records are independent of material streams. The original
  primary command arena held only 16 KiB, and material tokens encoded a
  14-bit byte offset plus a 4-bit surface number using four-byte alignment.
  Enlarging the arena alone would wrap these tokens and select wrong geometry.
  All six producer calls now use fixed 512-byte record slots; all six backend
  decoders resolve the corresponding 12-bit record index. The 16-bit token and
  surface number remain unchanged. Each pass supports 4,096 records, including
  native 128-model batches and both cached/pretessellated and uncached headers.
  Original native frame-arena offsets select disjoint pass storage. The pool
  reserves/commits 512 MiB of virtual memory (256 pass slices); physical pages
  are touched only as records are written. Allocation/guard failure leaves
  producers and decoders stock together. An unexpected runtime stream layout
  is rejected with the original model warning rather than mixing formats.
- All nine calls to the native static-model material encoder use a wrapper.
  Only a newly initialized, empty stream is redirected to 16x storage. The
  original encoder, packed headers and draw ordering are unchanged. Storage
  is keyed by the original front-end buffer address, so its reuse follows
  native double-buffer reuse rather than a guessed frame count or stack address.
  The total allocation is bounded at 128 MiB; allocation failures retain the
  original buffer and its original overflow warning.

Each patch group validates complete instruction bytes and every page permission
before writing any instruction. An unsupported engine stays stock for that group.
Warnings are not suppressed. `r_modelBudgets` prints actual lighting allocation,
patch-list usage/capacity, worker capacities, material storage and allocation
fallback attempts, plus command-record peaks and rejected stream layouts.

## Reproducible offline tests

The tests need a read-only archived `.text` and `.rdata` capture from the supported
engine (`0-0.text`, `rdata.bin`, `metadata.json`); proprietary engine code is not
included in this repository. No game process is opened or started.

Compile `scripts/test-renderer-budgets.cpp` with x64 MSVC, C++20 and `/EHsc`, then
run the executable with the capture directory and an optional JSON report path.
The isolated test process executes the real native initializer, allocator and
material encoder against private memory, with heap/copy/logging/worker fallbacks
stubbed. The native patch producers and full upload loop also run against a
mock texture lock/unlock interface and private atlas memory. Tests cover
mismatched/idempotent patch rejection, native lighting-cache
allocation and eviction, highest world-model indices, signed near-image offsets,
8,192 queued payloads, full-queue fallback, word-for-word material output, buffer
reuse, bounded allocation failure and concurrent stream initialization.
The patch-list regression reproduces the original 4,096-entry fatal boundary,
writes and uploads 49,152 entries over both frame buffers, checks every atlas
texel, verifies frame isolation/reset/reuse and canaries, and exercises both
retained fatal guards at the new boundary in isolated child processes. The
simulated texture interface is not a hardware Direct3D test.
Command tests compare native headers and model IDs byte-for-byte, cover both
frame arenas and reuse, execute the actual cached index-copy path, fill 8,192
records, retain the overflow warning, and execute all six patched decoder
instructions for all 65,536 tokens (393,216 decoder cases).

An optional third argument after the JSON report selects a binary Zod replay
fixture: two uint32 counts, 256 native view-global bytes, `count * 88` draw
bytes with model-type indices in place of pointers, `count` visibility bytes,
`types * 4` surface counts and `count * 36` instance bounds. The replay uses
minimal geometry/material headers, not the original GPU resources. It executes
the actual native primary submission routine with the archived placements and
visibility, compares stock/expanded coverage, and tests 32 bounding-box camera
frustums at four reported problem locations. This tests command admission and
encoding, not pixel parity, material appearance or hardware rendering.

`python scripts/audit-renderer-budgets.py CAPTURE --report REPORT.json` additionally
checks all 43 instruction guards and complete direct array/call-site coverage,
including all patch-list references and the unchanged counter lifecycle,
plus all six model-command producers and all six token decoder patterns,
against the archived code (requires Capstone).

These are CPU-side/offline validations, not a gameplay or GPU-rendering claim.
Actual Zod movement, GPU atlas use and remaining cull/portal/other renderer limits
still require an in-game test. Do not label every kind of pop-in eliminated solely
because these tests pass.
