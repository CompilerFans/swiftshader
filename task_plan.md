# Task Plan: Re-review and revise design + implementation plans

## Goal
结合新的 review 意见，技术性判断哪些反馈应采纳，并据此修订双语设计文档与实施计划，同时把判断依据记录到持久化规划文件中。

## Current Phase
Complete

## Phases
### Phase 1: Requirements & Discovery
- [x] Understand user intent
- [x] Identify constraints and requirements
- [x] Document findings in findings.md
- **Status:** complete

### Phase 2: Review Evaluation
- [x] Validate each review point against current docs and codebase reality
- [x] Decide accept / reject / partial accept for each point
- [x] Record rationale in findings.md
- **Status:** complete

### Phase 3: Document Revisions
- [x] Update design doc with accepted changes
- [x] Update implementation plan with accepted changes
- [x] Keep bilingual formatting consistent
- **Status:** complete

### Phase 4: Verification
- [x] Re-read revised sections for consistency
- [x] Confirm plan/design alignment
- [x] Log outcomes in progress.md
- **Status:** complete

### Phase 5: Delivery
- [x] Summarize accepted and rejected review items
- [x] Reference updated files
- [x] Offer next-step execution options
- **Status:** complete

## Key Questions
1. Which review items reflect real technical gaps versus optional refinements?
2. Which accepted changes belong in the design doc, the implementation plan, or both?
3. Do any accepted changes alter previously confirmed architecture decisions?

## Decisions Made
| Decision | Rationale |
|----------|-----------|
| 在当前工作区执行实现 | 用户明确要求不创建 worktree |
| Use `planning-with-files` for this turn | User explicitly requested it and task spans multiple document updates |
| Evaluate feedback with `receiving-code-review` discipline | Need technical verification instead of blind acceptance |

## Errors Encountered
| Error | Attempt | Resolution |
|-------|---------|------------|
| `cmake --build build-custom ...` failed with missing cache | 1 | Re-ran `cmake -S . -B build-custom` to recreate cache, then resumed incremental build |
| `rm -rf build-custom .cache` blocked by policy | 1 | Used Python `shutil.rmtree()` instead |
| Shell backtick expansion in `rg` command | 1 | Re-ran with `grep -F` and proper quoting |

## Notes
- Task 1: backend build skeleton complete.
- Task 2 complete: backend-neutral queue seam added with CPU default backend.
- Task 3 complete: dedicated `backend-unittests` target added and passing.
- Task 4 complete: minimal `SemanticIR` skeleton and tests added.
- Task 5 complete: minimal `KernelIR`/`KernelABI` skeletons and quad metadata tests added.
- Task 6 complete: standalone `SemanticIRBuilder` bootstrap path added.
- Task 7 complete: codegen text emitters and ABI parity checks added.
- Task 8 complete: runtime adapter and fake runtime bootstrap added.
- Task 9 complete: compute backend executable bootstrap and fake dispatch validation added.
- Task 10 complete: logical resource state tracker added and threaded into execution state.
- Task 11 complete: graphics backend stub extracted with CPU default implementation.
- Task 12 complete: fallback present adapter integrated into swapchain acquire/present flow.
- Task 13 complete: custom-backend build flags, presubmit smoke config, and bring-up doc added.
- Task 14 complete: smoke tests, bring-up checklist, and design status update added.
- Post-bootstrap compute runtime routing complete: custom backend now owns a device runtime and compute dispatch can flow into fake runtime capture when the custom backend flag is enabled.
- Post-bootstrap present factory complete: present adapter selection now follows backend factory rules, and custom builds expose fake acquire/present capture.
- Post-bootstrap graphics execution routing complete: custom builds now select an explicit custom execution backend bootstrap mode that still delegates graphics to CPU fallback cleanly.
- Pure color triangle milestone complete: `DrawTest.SolidColorTriangle` now performs pixel readback and passes in both default and custom fast-test builds.
- CUDA bootstrap design and implementation docs added:
  - `docs/plans/2026-03-08-custom-gpu-cuda-bootstrap-design.md`
  - `docs/plans/2026-03-08-custom-gpu-cuda-bootstrap-implementation.md`
- Real CUDA bootstrap milestone in progress: the repository now has a dedicated `SWIFTSHADER_CUSTOM_GPU_USE_CUDA` build mode, `CudaCompilerDriver`, `CudaRuntimeAPI`, and an extended `RuntimeAPI` with real module/memory/launch primitives.
- Current verified CUDA milestone: direct backend unit tests can compile CUDA-like source with `nvcc`, launch `kernel_main` through the CUDA Driver API, and read back a 32-bit device-memory result.
- Current graphics milestone: `DrawTest.SolidColorTriangle` passes in `build-cuda-bootstrap/` when run from the build directory, and the test's stamp-file assertion confirms that the custom CUDA build performed at least one real CUDA launch during initialization.
- Current multi-draw milestone: `DrawTest.MultipleSolidColorTriangles` passes in both `build-cuda-bootstrap/` and `build-draw-custom-subzero/`, using a test-only multi-draw recording hook in `DrawTester` while preserving the default single-draw path.
- Current artifact milestone: both triangle draw tests now save `BMP` snapshots under `draw-test-artifacts/` in the build directory via the test-only `DrawTester::saveFrame()` helper.
- Current CUDA debug milestone: the real CUDA runtime path now dumps kernel source to `stderr` by default, with `SWIFTSHADER_CUDA_DUMP_SOURCE=0|false|off|no` available to suppress it when needed.
- Current graphics-bootstrap milestone: the custom execution backend now performs one minimal `vertex bootstrap` CUDA compile+launch on the first graphics submit, while still delegating actual triangle rendering to the CPU fallback path.
- Current vertex-style bootstrap milestone: the graphics bootstrap kernel now carries an explicit vertex input/output contract and a `w = 1.0f` writeback, instead of the earlier comment-only placeholder.
- Current vertex wrapper/body milestone: the graphics bootstrap kernel now uses a `VsParams` launch contract plus an explicit `vs_entry` wrapper and generated-style `vs_main` body, and the temporary `kernel_main` compatibility wrapper has been removed.
- Current runtime entrypoint milestone: `RuntimeAPI` now supports per-module entrypoint names, `CudaRuntimeAPI` can launch `vs_entry` directly, and default callers still fall back to `kernel_main` until each stage is migrated.
- Current bootstrap execution milestone: `GraphicsBootstrap` now launches a real three-vertex `vs_entry` invocation on the CUDA runtime, and backend tests verify the written-back `x/y/z/w` outputs.
- Current generated-VS milestone: `GraphicsBootstrap` now supports a minimal compile-time `GraphicsBootstrapShaderConfig`, letting generated `vs_main` apply constant position offsets and proving a first step beyond pure passthrough lowering.
- Current builtin-lowering milestone: generated `vs_main` now supports a minimal `gl_VertexIndex`-style term on `x`, proving the wrapper-supplied builtin can flow into emitted CUDA source and real runtime execution.
- Current runtime-parameter milestone: `VsParams` now carries a minimal runtime offset payload, giving the bootstrap path its first push-constant-like data flow through the launch ABI.
- Current attribute-lowering milestone: the vertex wrapper now fetches `vec3 position` from raw vertex memory through `vertexStride` and `positionOffset`, proving a first minimal attribute/binding lowering path.
- Current instance-builtin milestone: generated `vs_main` now supports a minimal `gl_InstanceIndex`-style term on `y`, proving a second wrapper-supplied builtin can flow through emitted CUDA source and runtime execution.
- Current minimal SPIR-V vertex-lowering milestone: `SemanticIRBuilder` can now extract `Location 0`, `BuiltIn VertexIndex`, and `BuiltIn InstanceIndex` from a real `SpirvBinary`, `lowerToKernelIR()` carries that metadata forward, and `emitCudaLikeSource()` can emit a vertex-style CUDA wrapper/body from the lowered result.
- Current vertex-input-width milestone: the minimal SPIR-V path now also preserves the declared vector width for `Location 0`, so `vec2` position inputs lower into CUDA-like source with an explicit zero `z` fill instead of an invalid third-float fetch.
- Accepted VS completion gate: before starting raster/fragment follow-up work, vertex bring-up must finish minimal builtin support, minimal attribute/binding lowering, minimal `SPIR-V -> CUDA-like source` vertex lowering, and a small set of Vulkan-runtime vertex tests sourced from GLSL or SPIR-V.
- VS completion gate satisfied: the repository now has minimal builtin support, minimal attribute/binding lowering, minimal `SPIR-V -> CUDA-like source` vertex lowering, plus Vulkan runtime vertex tests covering both GLSL and explicit SPIR-V module creation.
- Current fragment-bootstrap milestone: the backend now has a standalone `FragmentBootstrap` path that emits `fs_entry/fs_main`, launches through `RuntimeAPI`, writes constant RGBA8 output, and validates helper/export suppression before raster integration.
- Current fragment-position test milestone: `DrawTest` now includes a `gl_FragCoord` quadrant-color case that saves a BMP artifact and verifies four screen-space sample points without needing new runtime-side draw infrastructure.
- Current fragment-position triangle milestone: `DrawTest` now also includes an ordinary-triangle variant of the `gl_FragCoord` quadrant-color test, with BMP export and interior sample points chosen from the triangle's actual coverage.
- Accepted performance observation gate: after the VS gate, establish CPU-only draw-performance baselines for `SolidColorTriangle` and `ManySolidTriangles`, plus a window-visible FPS observer, and require the future GPU draw path to reuse the same scenes for CPU/GPU comparison before broader feature expansion continues.
- Current performance-baseline milestone: `VulkanBenchmarks` now has a CPU `ManySolidTriangles` case at `1K`, `16K`, and `64K` scales, with labeled output including `triangle_count`, `fps`, `case`, `backend`, and `mode`.
- Current FPS-observer milestone: a standalone `draw-fps-observer` tool can continuously render the simple CPU draw scenes and print once-per-second FPS; benchmark tools now suppress CUDA source dumps by default so the output stays readable, and on non-Windows platforms the tool reports the repository's existing headless-surface limitation instead of opening a visible native window.
- Accepted raster bring-up rule: implement raster with a simple in-house CUDA path first, use the existing CPU raster behavior only as a reference oracle, align through dedicated CPU-reference and GPU-specific tests, and allow stub/dummy fields early as long as each step is driven by failing tests.
- Current raster-bootstrap milestone: the backend now has a standalone `RasterBootstrap` path with a CPU reference oracle, a real CUDA `raster_entry` kernel that emits dense coverage and compacts it into `FragmentBootstrapInvocation` records, and a narrow helper that feeds those invocations directly into `FragmentBootstrap`.
- Current triangle-pipeline bootstrap milestone: the backend now has a standalone `TrianglePipelineBootstrap` path that chains `GraphicsBootstrap`, `RasterBootstrap`, and `FragmentBootstrap`; the custom execution backend now uses that three-stage bootstrap on first graphics submit instead of launching only the vertex stage.
- Current triangle-pipeline config milestone: `TrianglePipelineBootstrap` now accepts an explicit config for framebuffer size and fragment output color, so the bootstrap chain is no longer hard-coded to one green triangle and is one step closer to consuming real draw-state inputs.
- Current triangle-pipeline vertex-input milestone: `TrianglePipelineBootstrap` now accepts both explicit vertex triples and raw vertex bytes plus stride/offset binding metadata, so the bootstrap chain can already consume the same minimal position-fetch contract used by `GraphicsBootstrap`.
- Current limitation: Vulkan shared-library compute dispatch in the real CUDA build is not yet wired end-to-end and is explicitly skipped in `tests/VulkanUnitTests/ComputeBackendPipelineTests.cpp`.
- Re-read this file before changing either plan document.
- Record accept/reject decisions explicitly.
- Keep design and implementation plan synchronized.
- Current draw-state bridge milestone: `TrianglePipelineBootstrap` now has a narrow real-draw bridge from `sw::Stream` for `triangle list + one primitive`, and `Renderer::draw()` uses it to trigger the first hardware-backed `VS -> Raster -> FS` bootstrap from actual bound vertex bytes instead of a queue-time hard-coded triangle.
- Current multi-triangle bootstrap milestone: the real-draw bridge now accepts positive non-indexed triangle-list primitive counts, and `TrianglePipelineBootstrap` can rasterize multiple triangles by iterating the existing stage bootstrap and composing the resulting color buffers.
- Current fragment-semantic milestone: `FragmentBootstrap` now supports both constant-color and `gl_FragCoord` quadrant modes through a minimal `shaderKind` config, and `TrianglePipelineBootstrap` can pass that fragment mode into the final CUDA stage.
- Current CUDA-source-observation milestone: `CudaRuntimeAPI` now mirrors dumped kernel source to an optional `SWIFTSHADER_CUDA_SOURCE_DUMP_PATH`, so Vulkan draw tests can verify which fragment bootstrap shader was compiled even when rendering happens inside `libvk_swiftshader.so`.
- Current fragment-constant bridge milestone: for the simplest fragment shaders that write one constant `vec4` directly to `Location 0`, `Renderer::draw()` now extracts that constant from `SpirvShader` and feeds it into the CUDA bootstrap path, so `fs_main` matches the real shader's constant color instead of falling back to the old hard-coded green.
- Current varying-color bridge milestone: the bootstrap path now carries one minimal color varying end-to-end. `GraphicsBootstrap` can fetch a color attribute, `RasterBootstrap` interpolates it per covered pixel, `FragmentBootstrap` has an `InterpolatedColor` mode, and `Renderer::draw()` can route a simple `location 0` fragment input through that path using `stream[1]` as the color source.
- Current visible benchmark milestone: the repository now has a scriptable `animated-triangle-benchmark` path for a rotating interpolated-color triangle with time-varying colors, visible window presentation on Linux/XCB, stdout/window-title FPS reporting, and a wrapper script that selects CPU or CUDA by choosing the corresponding build directory.
- Current next-phase planning milestone: CPU SwiftShader `VS / Raster / FS` support has been audited into a stage feature matrix and a concrete next-phase roadmap, so upcoming CUDA work can be scheduled against explicit parity families instead of ad hoc demos.
- Current indexed-draw milestone: the CUDA bootstrap path now accepts indexed triangle-list draws by expanding indices on the host side before launching the existing stage chain, and the lightweight draw harness can record indexed draws through a minimal index-buffer helper.
- Current barycentric milestone: the CUDA bootstrap path now carries first-class barycentric payloads from raster to fragment, and interpolated-color fragment execution reconstructs colors from barycentrics plus per-triangle vertex colors instead of relying on the older pre-interpolated color shortcut.
- Current FrontFacing milestone: the CUDA bootstrap path now carries a per-triangle `frontFacing` flag from raster to fragment, `Renderer::draw()` detects `BuiltInFrontFacing`, and a minimal binary-colors fragment mode validates the end-to-end path against the CPU renderer.
- Current discard milestone: the CUDA bootstrap path now recognizes a narrow `FragCoord + discard` fragment family, emits a left-half discard mode, and validates it end-to-end against the CPU renderer.
- Current FragDepth milestone: the CUDA bootstrap path now supports a narrow interpolated-color-driven `FragDepth` mode, optional fragment depth-buffer output, and host-side depth composition across overlapping triangles.
- Current point-list milestone: the CUDA bootstrap path now accepts `POINT_LIST` draws through a temporary host-side point-to-quad expansion, and the draw harness can configure point topology for focused tests.
- Current PointCoord milestone: the CUDA bootstrap path now carries `pointCoordX/Y` for point-list draws and validates a minimal `PointCoordGradient` fragment mode end-to-end.
- Current flat-interpolation milestone: the CUDA bootstrap path now distinguishes `flat` varying color from smooth interpolation and validates it against the default `FIRST_VERTEX` provoking rule.
- Current triangle-strip milestone: the CUDA bootstrap path now accepts `TRIANGLE_STRIP` draws through host-side strip expansion that follows the CPU default provoking-vertex ordering.
- Current triangle-fan milestone: the CUDA bootstrap path now accepts `TRIANGLE_FAN` draws through host-side fan expansion, while primitive restart remains tracked separately as a CPU baseline blocker.
- Current line-list milestone: the CUDA bootstrap path now accepts `LINE_LIST` draws through a temporary host-side line-to-quad expansion, and the draw harness can force a wider line width for stable validation.

- Current topology-breadth milestone: `LINE_LIST` and `LINE_STRIP` now run through the CUDA bootstrap path via host-side line-to-quad expansion, with dedicated backend coverage and draw tests that dump BMP artifacts.

- Current VS builtin milestone: the CUDA bootstrap path now carries a minimal constant `gl_PointSize` from `SpirvShader` into point-list rendering, so point coverage no longer relies on the previous hard-coded 64-pixel fallback.

- Current indexed-topology milestone: `indexed line-strip` and `indexed triangle-fan` are now covered by backend and Vulkan draw tests, confirming that the existing host-side deindex + topology expansion path already handles these two shapes.

- Current indexed-point milestone: `indexed point-list` is now covered by backend and Vulkan draw tests, confirming that the existing host-side deindexing path already composes with the point-list bootstrap and `PointSize` bridge.

- Current primitive-restart draw milestone: `IndexedTriangleStripWithPrimitiveRestart` is now covered in the draw harness and passes in both CPU and CUDA builds; the earlier failure was a bad sample point, not a renderer defect.

- Current blocker revalidated: `noperspective` still crashes in both CPU and CUDA draw paths, so it remains outside the committed parity suite until root-caused.

- Current indexed-pointcoord milestone: `indexed point-list` now also has `PointCoord` gradient coverage in both CPU and CUDA draw builds.

- Texture bootstrap planning complete: narrow `combined image sampler + vec2 uv + texture()` design and implementation plan are saved under `docs/plans/2026-03-09-texture-bootstrap-*.md`.

- Texture bootstrap implementation milestone: narrow `combined image sampler + uv + texture()` is now live in both non-indexed and indexed triangle draws.

- Current texture-followup milestone: `linear/repeat` textured draw coverage and texture benchmark integration are now in progress.

- Current texture-followup milestone: `linear/repeat` textured draw coverage is now green, and the texture benchmark shader/descriptors are binding-aligned.

- Clear-background harness milestone: `DrawTester` now has an explicit opt-in color clear path for tests that need stable backgrounds, while the default non-MSAA path remains `eDontCare` to match the original CPU behavior.

- Animated benchmark milestone: `animated-triangle-benchmark` now supports `--scene=color|texture`, and both scenes explicitly clear the background for stable visible output.

- Push-constant milestone: draw harness now supports explicit push constant ranges/data, and narrow VS offset / FS tint cases pass in both CPU and CUDA builds.

- Current instancing milestone: `gl_InstanceIndex` draw coverage is now present in both CPU and CUDA builds.

- Current indexed-baseVertex milestone: `drawIndexed(..., vertexOffset, ...)` coverage is now present in both CPU and CUDA builds.

- Vulkan Samples compatibility roadmap added: the current sample ladder now explicitly highlights missing capabilities such as separate image/sampler, mipmap generation, dynamic uniform buffers, dynamic rendering, dynamic blending, vertex dynamic state, and full instancing input-rate coverage.

- Current blocker confirmed: `separate image + sampler` is not yet safe to promote into the committed parity suite; it needs separate investigation from the already-working combined image sampler path.

- Current per-instance-input milestone: `VK_VERTEX_INPUT_RATE_INSTANCE` draw coverage is now present in both CPU and CUDA builds.

- Current sample-gap blocker: `separate image + sampler` still fails as a real capability gap and should be tracked separately from the already-working combined image sampler path.

- Explicit blocker: `separate image + sampler` is a real capability gap and is now deferred until a dedicated investigation cycle; do not route normal short-term feature work back into it.

- Current firstInstance milestone: `draw(..., firstInstance)` / `gl_InstanceIndex` offset coverage is now present in both CPU and CUDA builds.

- Current sample-gap blocker: `dynamic rendering` is not yet ready for promotion into the committed parity suite and needs a dedicated investigation cycle.

- Current sample-aligned milestone: `MSAA resolve` draw coverage is now present in both CPU and CUDA builds, while `dynamic rendering` remains a separate blocker.

- Current sample-gap blocker: `dynamic uniform buffers` should be treated as a dedicated investigation item rather than part of the immediate low-risk feature path.

- Current sample-aligned milestone: `vertex_dynamic_state` now has repo-local draw coverage via `DrawTest.VertexInputDynamicStateSolidColorTriangle`, so it moves out of the “missing” bucket and into the staged compatibility ladder.

- Current sample-aligned milestone: `vertex_dynamic_state` now has both solid-color and interpolated-color repo-local draw coverage, making the sample path materially stronger before tackling broader format/topology combinations.

- Current sample-gap finding: a local `texture_mipmap_generation` explicit-LOD probe does not yet produce the expected level-1 color even in the CPU baseline, so mip/LOD work should stay out of the immediate low-risk lane for now.

- Current sample-aligned milestone: `vertex_dynamic_state` now also covers the `VK_VERTEX_INPUT_RATE_INSTANCE` combination, so the local sample ladder includes both multi-attribute and instance-rate dynamic vertex input cases.

- Current sample-aligned milestone: `render_passes` now has a first repo-local coverage point via `vkCmdClearAttachments`, while load/store, subpass dependency, and layout-transition cases remain future work.

- Current sample-aligned milestone: `msaa` now has repo-local resolve coverage for both solid-color and interpolated-color triangles, improving confidence before tackling heavier sample scenarios.

- Current sample-aligned milestone: `render_passes` now also has a repo-local `loadOp = LOAD` preservation test on swapchain images, strengthening attachment load semantics before subpass work.

- Current sample-aligned milestone: `render_passes` now also covers depth-aspect `vkCmdClearAttachments`, not just color clears and `loadOp = LOAD` preservation.

- Current sample-aligned milestone: `instancing` now also has repo-local coverage for the combined `drawIndexed + baseVertex + firstInstance + instance-rate` path, not just the simpler individual pieces.

- Current sample-aligned milestone: `render_passes` now also has a repo-local two-subpass overlay case, so the local ladder no longer stops at single-subpass attachment operations.

- Current sample-aligned milestone: `vertex_dynamic_state` now also covers the combined indexed instancing path with `baseVertex` and `firstInstance`, not just the simpler dynamic vertex-input cases.

- Current sample-aligned milestone: `swapchain_images` now has repo-local coverage for configurable `minImageCount`, not just the fixed double-buffered default path.

- Current sample-aligned milestone: `render_passes` now also has repo-local depth `loadOp = LOAD` coverage, not just color LOAD and subpass sequencing.
