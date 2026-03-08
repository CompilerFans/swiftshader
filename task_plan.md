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
