# CPU SwiftShader Stage Feature Audit

## Goal

Audit what the original CPU SwiftShader path already supports in `VS / Raster / FS`, then use that as the reference matrix for the next CUDA backend phase.

## Recommended Planning Approach

### Option A: Stage-parity matrix first (recommended)
- Audit CPU support by stage and by feature family.
- Group next-phase work by “must preserve correctness first” instead of by source file.
- Result: lower architectural risk and fewer dead-end bootstrap extensions.

### Option B: Expand current bootstrap opportunistically
- Keep adding whatever test is easiest next.
- Result: fast local wins, but feature ordering drifts and ABI debt accumulates.

### Option C: Compiler-first planning
- Focus on broad SPIR-V lowering first, postpone raster/depth/blend details.
- Result: good shader story, weak graphics bring-up sequencing.

**Recommendation:** use **Option A**. The current codebase is already at the point where stage ABI and feature ordering matter more than isolated demos.

## CPU Reference Matrix

### 1. Vertex Shader / VS

**Clearly supported in the CPU path**
- Stage enablement: `Vertex`, `Fragment`, and `Compute` are wired; `TessellationControl`, `TessellationEvaluation`, and `Geometry` are not enabled in `executionModelToStage()`: `src/Pipeline/SpirvShader.cpp:2814`
- Vertex input fetch across Vulkan formats via `VertexRoutine::readStream()`, with robust buffer access handling: `src/Pipeline/VertexRoutine.cpp:174`
- Builtins explicitly wired in `VertexProgram`:
  - `VertexIndex`: `src/Pipeline/VertexProgram.cpp:67`
  - `InstanceIndex`: `src/Pipeline/VertexProgram.cpp:39`
  - `ViewIndex`: `src/Pipeline/VertexProgram.cpp:34`
  - `SubgroupSize`: `src/Pipeline/VertexProgram.cpp:45`
- Push constants, descriptors, and device constants are available in the stage wrapper: `src/Pipeline/VertexProgram.cpp:50`
- Outputs written by the CPU vertex path:
  - `Position`: `src/Pipeline/VertexRoutine.cpp:118`
  - `PointSize`: `src/Pipeline/VertexRoutine.cpp:641`
  - user varyings: `src/Pipeline/VertexRoutine.cpp:691`
  - `ClipDistance`: `src/Pipeline/VertexRoutine.cpp:653`
  - `CullDistance`: `src/Pipeline/VertexRoutine.cpp:667`
- Per-vertex clipping and depth-clip range handling are already present: `src/Pipeline/VertexRoutine.cpp:130`

**Implication for CUDA**
- The next CUDA VS phase must not stop at `Position + color`. The real compatibility target includes:
  - robust input fetch
  - `VertexIndex` / `InstanceIndex`
  - `PointSize`
  - clip/cull distance payload
  - push constants and descriptor-backed reads

### 2. Raster / Setup / Primitive Assembly

**Clearly supported in the CPU path**
- Input assembly and primitive shaping:
  - `PointList`, `LineList`, `LineStrip`, `TriangleList`, `TriangleStrip`, `TriangleFan`: `src/Device/Context.cpp:565`
  - indexed and non-indexed draws: `src/Device/Context.cpp:249`
  - primitive restart: `src/Device/Context.cpp:262`
  - provoking vertex mode is part of draw-state processing: `src/Device/Renderer.cpp:165`
- Polygon modes:
  - fill / line / point are selected in `Renderer::draw()`: `src/Device/Renderer.cpp:487`
- Front face and cull mode are handled in setup: `src/Pipeline/SetupRoutine.cpp:74`
- Viewport/scissor/depth-bias state is already part of setup/raster preparation: `src/Device/Renderer.cpp:420`
- Clip/cull distance rejection is integrated into setup: `src/Pipeline/SetupRoutine.cpp:492`
- Gradient setup for fragment interpolation is explicit, including `Flat` and perspective choice: `src/Pipeline/SetupRoutine.cpp:481`
- MSAA and Bresenham-line exceptions are already modeled in setup state: `src/Device/SetupProcessor.cpp:84`

**Implication for CUDA**
- The current CUDA raster bootstrap only covers “single-sample triangle bbox coverage”.
- The real next-phase target must explicitly stage the following expansions:
  - indexed triangle list
  - strip/fan and primitive restart
  - point/line raster modes
  - provoking vertex
  - front-facing and cull mode
  - depth bias
  - interpolation mode metadata

### 3. Fragment Shader / FS

**Clearly supported in the CPU path**
- Interpolation modes:
  - `Flat`, `NoPerspective`, `Perspective`, `Centroid`: `src/Pipeline/PixelRoutine.cpp:237`
  - per-sample shading trigger paths are present: `src/Pipeline/PixelRoutine.cpp:24`
- Input builtins:
  - `FragCoord`: `src/Pipeline/PixelProgram.cpp:113`
  - `PointCoord`: `src/Pipeline/PixelProgram.cpp:120`
  - `ViewIndex`: `src/Pipeline/PixelProgram.cpp:108`
  - `FrontFacing`: `src/Pipeline/PixelProgram.cpp:146`
  - `HelperInvocation`: `src/Pipeline/PixelProgram.cpp:132`
  - `SampleMask` input/output: `src/Pipeline/PixelProgram.cpp:154`, `src/Pipeline/PixelProgram.cpp:222`
  - `SampleId`: `src/Pipeline/PixelProgram.cpp:175`
  - `SamplePosition`: `src/Pipeline/PixelProgram.cpp:184`
  - `FragDepth`: `src/Pipeline/PixelProgram.cpp:233`
- Fragment control semantics:
  - discard / kill / demote tracked in analysis: `src/Pipeline/SpirvShader.hpp:573`
  - helper-lane store suppression is part of the emitter model: `src/Pipeline/SpirvShader.hpp:979`
- Fixed-function interaction already integrated:
  - depth test / write / clamp: `src/Device/PixelProcessor.cpp:99`
  - stencil test: `src/Device/PixelProcessor.cpp:88`
  - depth bounds: `src/Device/PixelProcessor.cpp:92`
  - alpha-to-coverage: `src/Device/PixelProcessor.cpp:78`
  - blend state and color write masks: `src/Device/PixelProcessor.cpp:145`
  - sample shading and centroid needs: `src/Device/PixelProcessor.cpp:170`
- Resource-side semantics:
  - robust buffer access: `src/Device/PixelProcessor.cpp:72`
  - input attachments: `src/Pipeline/SpirvShader.cpp:1755`
  - storage image writes: `src/Pipeline/SpirvShader.cpp:777`
  - control barriers are analyzed: `src/Pipeline/SpirvShader.cpp:781`

**Implication for CUDA**
- Our current CUDA FS coverage is still intentionally narrow.
- The minimum “serious next phase” FS backlog is:
  - interpolation modes beyond the current varying-color shortcut
  - `FrontFacing`
  - `PointCoord`
  - `FragDepth`
  - discard/demote correctness
  - sample-related builtins and sample mask
  - depth/stencil/blend integration

## What Must Be Reserved Early

To avoid ABI churn in the next phase, the CUDA path should reserve the following even before full implementation:

- **VS output payload**
  - `Position`
  - `PointSize`
  - clip/cull distance spans
  - user varyings
- **Raster invocation payload**
  - barycentrics
  - interpolated/flat payload slots
  - `frontFacing`
  - sample id / sample position hooks
  - `pointCoord`
  - depth candidate
  - coverage / helper / export masks
- **FS launch contract**
  - descriptors
  - push constants
  - attachment/depth-stencil handles
  - late writeback choices (`FragDepth`, `SampleMask`)

## Recommended Next-Phase Roadmap

### Phase 1: Draw-state coverage expansion
- Indexed triangle-list
- Primitive restart off by default, explicit tests
- More than one vertex attribute
- Keep triangle-only focus

### Phase 2: Real interpolation model
- Replace the current “pre-interpolated color shortcut” with first-class barycentrics
- Support `Flat`, `Perspective`, `NoPerspective`, `Centroid`

### Phase 3: Critical FS correctness
- `FrontFacing`
- `PointCoord`
- `FragDepth`
- discard / demote
- sample-mask plumbing

### Phase 4: Raster and topology breadth
- strip/fan
- point and line paths
- provoking vertex
- line rasterization modes

### Phase 5: Depth / stencil / blend / MSAA
- depth test/write
- stencil operations
- blend
- per-sample paths

### Phase 6: Descriptor-heavy shaders
- push constants
- descriptor reads
- input attachments
- storage image writes

## Bottom Line

The CPU SwiftShader path is already much richer than the current CUDA bootstrap in three places:
- raster/input assembly breadth
- interpolation/sample semantics
- fragment fixed-function integration

So the next CUDA phase should not be “more random shader demos”. It should be **stage-parity expansion in a fixed order**, with raster/FS ABI reserved early and correctness tests grouped by feature family.

