# Findings & Decisions

## Requirements
- Review two existing plan documents using new user-provided feedback.
- Decide technically whether the design doc and implementation plan need modification.
- Modify the documents when warranted.
- Keep documents bilingual.
- Persist reasoning in project-root planning files.

## Research Findings
- Current design doc already covers architecture, IR, runtime, synchronization, WSI/present, and phased rollout.
- Current implementation plan already has 14 tasks with TDD structure and precise file paths.
- `src/Pipeline/SpirvShader.*` is strongly coupled to Reactor code emission today: `SpirvShader.hpp` directly exposes `emit*`, `SpirvEmitter`, `rr::Value`, `SpirvRoutine`, and many `Emit*` methods. This validates the concern that a simple member method like `SpirvShader::buildSemanticIR()` would likely accumulate technical debt.
- `README.md` is clearly public-facing project documentation, not an internal bring-up note. Adding custom backend instructions there early would be risky unless the backend becomes a public/project-level feature.
- Current design doc mentions quad/helper-lane formation, but does not yet spell out execution invariants such as 2x2 residency, helper-lane execution without export, discard/demote behavior, or derivative constraints.
- Current design doc mentions image/sampler categories, but does not distinguish combined image sampler, separate sampler, storage image, or non-uniform descriptor indexing.
- Current design doc says the CUDA-like source and LLVM IR paths share ABI, but does not define a verification mechanism.
- Current design doc reserves a native displayable-image fast path, but does not define clear eligibility conditions.
- Current implementation plan adds `ResourceStateTracker` before graphics/present stubs, but does not explicitly require those later tasks to integrate with it.
- Current implementation plan adds a compute compile path, but does not yet require fake dispatch validation of launch parameters and buffer binding.

## Decisions
- **Accept:** Expand design doc with explicit quad/helper-lane fragment execution invariants.
- **Accept:** Expand design doc with explicit `SemanticIR` modeling rules for combined image sampler, separate sampler, storage image, and non-uniform descriptor access.
- **Accept:** Add an explicit ABI conformance verification mechanism shared by CUDA-like source and LLVM IR codegen paths.
- **Accept:** Clarify native displayable-image fast-path eligibility versus fallback copy/blit conditions.
- **Accept:** Change implementation plan so `SemanticIR` lowering is built as a standalone builder/visitor, not as a `SpirvShader` member API.
- **Accept:** Strengthen the compute bring-up task with fake dispatch validation, not only executable creation.
- **Accept:** Require `GraphicsBackend` and `PresentAdapter` tasks to integrate with `ResourceStateTracker`.
- **Accept:** Remove near-term `README.md` edits from the implementation plan; keep backend bring-up docs in internal or specialized docs first.
- **Partial accept:** The existing design is directionally correct, so these changes are refinements and risk reductions, not architectural reversals.

## Open Questions
- Whether ABI equivalence should be validated through textual ABI header comparison, structured metadata comparison, or both.
- Whether quad/helper-lane semantics deserve a dedicated subsection or should extend the existing `KernelIR` section only.
- How to thread the real CUDA runtime through the Vulkan shared-library compute path without stalling queue submission.

## Relevant Files
- `docs/plans/2026-03-07-cuda-vulkan-icd-design.md`
- `docs/plans/2026-03-07-custom-gpu-vulkan-icd-implementation.md`
- `src/Pipeline/SpirvShader.hpp`
- `src/Vulkan/VkQueue.cpp`
- `src/Vulkan/VkCommandBuffer.hpp`
- `src/WSI/VkSurfaceKHR.hpp`
- `README.md`
- `docs/plans/2026-03-08-custom-gpu-cuda-bootstrap-design.md`
- `docs/plans/2026-03-08-custom-gpu-cuda-bootstrap-implementation.md`
- `src/Backend/CudaCompilerDriver.hpp`
- `src/Backend/CudaCompilerDriver.cpp`
- `src/Backend/CudaRuntimeAPI.hpp`
- `src/Backend/CudaRuntimeAPI.cpp`
- `tests/BackendUnitTests/RuntimeAPITests.cpp`
- `tests/VulkanUnitTests/DrawTests.cpp`

## New Findings
- The custom GPU backend can now use the host machine's real CUDA toolchain: `nvcc` is available at `/usr/local/cuda/bin/nvcc`, `libcuda.so.1` is visible through the system loader, and a visible GPU is present.
- Driver API dynamic loading must prefer versioned symbols such as `cuMemAlloc_v2`, `cuMemFree_v2`, `cuMemcpyHtoD_v2`, and `cuMemcpyDtoH_v2`. Using the unversioned names led to `CUDA_ERROR_INVALID_CONTEXT` during the first real runtime tests.
- Vulkan wrapper tests that rely on the generated SwiftShader ICD must be run from their corresponding build directories. Running them from the repository root produces misleading loader failures and false crash symptoms.
- `backend-unittests` cannot directly depend on the full `SpirvShader.cpp` object graph without pulling hidden/internal Vulkan C++ symbols into the test link. Splitting `SemanticIRBuilder`'s lightweight `SpirvBinary` path from the heavier `SpirvShader` overload keeps backend tests small and linkable while preserving the runtime-facing overload for `VkPipeline`.
- The repository already contains a suitable starting point for simple-but-real draw performance measurement in `tests/VulkanBenchmarks/TriangleBenchmarks.cpp`; the missing piece is not a benchmark framework, but a staged baseline policy and a heavier batched triangle case that can later be reused unchanged by the GPU draw path.
- On non-Windows platforms, `tests/VulkanWrapper/VulkanHeaders.hpp` forces `USE_HEADLESS_SURFACE=1`, so the new FPS observer can provide live FPS printing immediately but cannot show a native visible window without broader platform window-support work. That limitation is pre-existing and independent of the new performance gate.
- Interface compile definitions from `vk_base` do not automatically reach standalone benchmark executables like `draw-fps-observer`, so tool-local behavior such as CUDA dump suppression must not rely on `SWIFTSHADER_CUSTOM_GPU_USE_CUDA` being defined in the benchmark translation unit.
- For the CUDA-like vertex emitter, preserving only `Location 0` is not enough; the SPIR-V input vector width also has to survive lowering so `vec2` vertex inputs do not trigger an out-of-contract read of `position[2]`.
- The Vulkan runtime vertex-validation layer benefits from a direct `std::vector<uint32_t>` shader-module helper in `DrawTester`; it keeps GLSL-based tests and explicit SPIR-V module tests on the same lightweight draw harness without dropping to raw setup code in each case.
- A fragment standalone bootstrap can reuse the same runtime/module/launch pattern as the vertex bootstrap if it lowers to an explicit invocation list plus a linear RGBA8 buffer; helper/export suppression can be expressed as per-invocation flags without blocking the later raster integration.
- `gl_FragCoord`-based fragment validation does not need new harness code in this repository: a fullscreen triangle plus four quadrant sample points is already enough to produce a visually useful BMP artifact and stable assertions.
- For an ordinary triangle with `gl_FragCoord` quadrant coloring, top-half coverage narrows quickly near the apex, so stable assertions should sample upper pixels close to the screen center; background color should not be asserted with the current `DrawTester` because the non-multisample color attachment still uses `eDontCare`.
- The current SwiftShader CPU raster stack is valuable as a semantic oracle, but its `SetupProcessor` / `QuadRasterizer` / `PixelProcessor` implementation shape is not a good direct fit for early CUDA bring-up; a simple in-house CUDA raster with CPU-reference tests is the lower-risk path.
- For the first raster bootstrap, writing a dense per-pixel `FragmentBootstrapInvocation` grid on the GPU and compacting it on the host is simpler and less risky than introducing an atomic append buffer up front; it keeps the CUDA kernel minimal while still allowing direct comparison against the CPU reference oracle.
- The first end-to-end triangle bootstrap failure was caused by raster launch geometry, not by the sample point: launching `raster_entry` as a single `64x64` CUDA block meant only the vertex stage completed. Switching raster to small blocks over a 2D grid allowed the full `VS -> Raster -> FS` bootstrap chain to execute.
- Hard-coding fragment output in the three-stage bootstrap would stall the path before it can consume real draw state; adding a narrow `TrianglePipelineBootstrapConfig` for framebuffer size and RGBA output is a low-risk way to start replacing hard-coded stage behavior with explicit inputs.
- The next practical step toward real draw integration is not exposing full Vulkan state, but reusing the existing raw-vertex fetch contract (`rawVertexData + vertexCount + GraphicsBootstrapBindingConfig`) inside `TrianglePipelineBootstrap`; that moves the bootstrap path onto the same minimal position-fetch surface without widening the API too early.
- The first safe bridge from real Vulkan draw state into the CUDA bootstrap path is `sw::Stream`, not the broader `DrawCall` or queue submit layer: after `Inputs::bindVertexInputs()`, `stream.buffer` already points at the bound attribute bytes with binding and attribute offsets applied.
- For the current triangle bootstrap, the narrowest correct support window is `VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST` with exactly one primitive and a vertex-rate `Location 0` position stream in `VK_FORMAT_R32G32_SFLOAT` or `VK_FORMAT_R32G32B32_SFLOAT`; widening beyond that should come after the real draw-fed path is stable.
- Queue-submit-time graphics bootstrap hides the real draw inputs behind a hard-coded triangle. Moving the first hardware-backed bootstrap trigger into `Renderer::draw()` makes the dumped CUDA stage wrappers correspond to an actual bound draw call while still leaving final rasterization on the CPU path.
- A practical next step beyond the first real-draw bridge is to widen from one triangle to many triangles before attempting richer shader semantics. The bridge can safely copy `primitiveCount * 3` bound vertices for non-indexed triangle-list draws without needing broader Vulkan state.
- For early bring-up, it is cheaper to reuse the existing single-triangle `RasterBootstrap`/`FragmentBootstrap` pair per triangle and compose the RGBA8 results on the host than to redesign the raster kernel around multi-triangle batching immediately. That keeps the code simple while enabling a first multi-triangle CUDA path.
- The first useful fragment-side semantic expansion after constant color is a tiny `shaderKind` switch in `FragmentBootstrapConfig`. That keeps the launch ABI stable while allowing the emitted CUDA source to model multiple fragment behaviors.
- For the current simple raster bootstrap, a triangle-pipeline integration test should not assume too many interior sample points. A more robust gate is: one known-covered pixel plus confirmation that the last compiled fragment CUDA module actually contains the requested shader body.
- In the Vulkan draw tests, `backend::CudaRuntimeAPI::globalLastModuleSource()` is not a reliable observation point because `draw-unittests` and `libvk_swiftshader.so` hold separate copies of the runtime's static capture state. Cross-library CUDA source verification needs an external channel such as a file dump.
- When doing lightweight SPIR-V analysis from `SpirvShader`, iterate with `shader.begin()/end()` or range-based-for. Walking `shader.insns` directly includes the 5-word SPIR-V header and can stall or misparse the module.
- `sw::Stream` already exposes enough metadata to bridge a minimal second vertex attribute into the CUDA bootstrap path. If `stream[0]` and `stream[1]` share binding, rate, and stride, `colorOffset` can be derived as `stream1.buffer - stream0.buffer` without widening the Vulkan-side draw plumbing yet.
- `FragmentBootstrap` originally launched all invocations in a single CUDA block. Once raster started feeding larger invocation lists for interpolated-color triangles, that exceeded practical block limits and produced silent zero output. The fragment stage needs the same multi-block launch treatment as raster.
- On this Linux environment, a visible benchmark window is feasible through XCB: `DISPLAY=:0`, `libxcb` is available, and `libvk_swiftshader.so` exports `vkCreateXcbSurfaceKHR` / `vkGetPhysicalDeviceXcbPresentationSupportKHR`. The missing piece was only the test wrapper forcing headless mode.
- For backend selection, the honest near-term interface is a wrapper script that chooses between a CPU build directory and a CUDA-enabled build directory. The repository still does not support a true runtime CPU/CUDA switch inside one binary.
- The first visible animated-triangle benchmark already shows a large CPU/CUDA gap on this machine (`~297 FPS` vs `~0.8 FPS` over short runs). That does not yet prove the final design is wrong, but it is exactly the kind of result the staged performance gate is supposed to surface early.
- The original CPU SwiftShader path is already much richer than the current CUDA bootstrap in three concentrated areas: input assembly / topology breadth, interpolation + sample semantics, and fragment fixed-function integration. Next-phase planning should therefore be organized by stage-parity families, not by whichever demo is easiest next.
- The current CPU reference scope for near-term parity is clear: keep `Vertex/Fragment/Compute` only, postpone `TCS/TES/GS`, and expand CUDA in the order `indexed draws -> barycentrics/interpolation -> fragment builtins -> depth/stencil/blend -> topology breadth -> resource-heavy shaders`.
- The narrowest practical path for indexed bootstrap support is to deindex on the host side before launching the current `VS -> Raster -> FS` CUDA chain. That preserves the existing runtime ABI and still expands real Vulkan draw-state coverage.
- The current interpolated-color shortcut can be replaced incrementally: because `TrianglePipelineBootstrap` still executes per triangle, raster can emit first-class barycentrics while fragment receives the three vertex colors through `FragmentBootstrapConfig`. This upgrades the stage contract without requiring a full multi-triangle fragment ABI yet.
- `gl_FrontFacing` can be brought up with a very small ABI expansion: raster only needs one per-triangle facing bit, and fragment can consume it through a dedicated binary-colors mode before full arbitrary FrontFacing expression lowering exists.
- `gl_FrontFacing` does not require a full arbitrary fragment-expression lowering to be useful early. A single per-triangle facing bit from raster plus a binary front/back color mode is enough to validate the end-to-end path and align with the CPU renderer's orientation rules.
- A practical early discard path does not require general expression lowering. For the current bootstrap, `FragCoord + ContainsDiscard` can be validated with a dedicated half-screen discard mode, which exercises real fragment kill semantics without widening the raster ABI.
- A minimal early `FragDepth` path can be made real without full depth/stencil parity: let `FragmentBootstrap` optionally write a depth buffer, then let `TrianglePipelineBootstrap` perform host-side depth composition while the stage ABI is still narrow.
- The shortest way to bring up point-list before `PointCoord` is to expand each point into two host-side triangles and reuse the existing triangle raster/bootstrap chain. This is a transitional implementation, but it widens real draw-state coverage immediately.
- Once point-list bootstrap exists, `PointCoord` becomes a pure fragment-payload problem. The shortest path is to populate `pointCoordX/Y` directly in the point branch and expose a narrow `PointCoordGradient` fragment mode before tackling general point fragment lowering.
- `flat` 插值在当前 bootstrap 里不需要新的 raster payload；只要在 fragment 侧直接选 provoking vertex 的颜色即可。结合当前默认 `FIRST_VERTEX` 规则，这是一条非常短的 parity 增量路径。
- The earlier `noperspective` draw-test crash was a glslang (GLSL -> SPIR-V) segfault triggered by `#version 310 es` shaders using `noperspective`. Switching the reproducer to Vulkan GLSL `#version 450` avoids the crash, and `DrawTest.FragmentShaderUsesNoPerspectiveColor` now provides repo-local coverage that validates `NoPerspective` interpolation differs from perspective-correct interpolation.
- `triangle strip` can be brought up with the same transitional strategy used for indexed draws and point lists: expand to triangle-list on the host side using the CPU default `FIRST_VERTEX` strip ordering, then reuse the existing triangle bootstrap chain.
- `IndexedTriangleStripWithPrimitiveRestart` currently fails in the CPU-only baseline as well as the CUDA build, so primitive restart should be tracked as a CPU-reference blocker rather than a CUDA-bootstrap regression until the baseline path is understood.
- `IndexedTriangleStripWithPrimitiveRestart` currently fails in the CPU-only baseline, so primitive restart remains a CPU-reference blocker and should not be used as a CUDA regression signal yet.
- `triangle fan` is another good fit for host-side topology expansion: keep the center vertex fixed, expand `(0, i+1, i+2)` per primitive, and reuse the existing triangle bootstrap chain unchanged.
- `gl_SampleMask` output currently crashes in the CPU-only baseline as well as the CUDA build. Like the earlier `noperspective` case, it should be tracked as a CPU-reference blocker before being used as a CUDA parity milestone.
- `LineListConstantColor` needed an explicit wide line in the draw harness to become a stable CPU baseline. For topology bring-up, test geometry should be chosen to avoid mistaking thin-line raster rules for backend regressions.

- `LINE_STRIP` needed a frame-level readback assertion instead of a single hard-coded sample point. The rendered V-shape leaves the screen center empty, so stable verification should count red pixels across the dumped frame rather than assume one interior pixel is covered.

- `gl_PointSize` for the current bootstrap can be brought up with a narrow constant-path extractor: scan the builtin output block for `PointSize`, follow `OpAccessChain` pointers to the matching member, and accept only uniform constant `OpStore` values. That is enough to remove the old hard-coded point-size fallback for simple point shaders.

- The current host-side deindexing step is already general enough to support indexed `LINE_STRIP` and indexed `TRIANGLE_FAN` in the bootstrap path, as long as primitive restart is not enabled. That makes primitive restart the real remaining blocker in this topology family.

- After the `PointSize` bridge landed, indexed `POINT_LIST` required no extra runtime changes: the existing deindex-before-bootstrap path already composes correctly with the point quad expansion and point-fragment payload generation.

- `IndexedTriangleStripWithPrimitiveRestart` was not a real renderer blocker in the current draw harness: the initial failure came from sampling below the actual covered region. After correcting the sample points, the case passes in the CPU baseline.

- `IndexedTriangleStripWithPrimitiveRestart` is currently covered at the draw-harness level and passes in both CPU and CUDA builds; the previously observed failure was caused by sampling below the actual covered region, not by primitive restart semantics being broken in the renderer path.

- A minimal `noperspective` varying-color draw case no longer blocks the suite once authored as Vulkan GLSL `#version 450`: the repo-local `DrawTest.FragmentShaderUsesNoPerspectiveColor` passes in both CPU and CUDA builds and provides a concrete regression gate.

- `indexed POINT_LIST` composes cleanly not only with constant-color output but also with the existing `PointCoordGradient` fragment path; no extra runtime changes were required beyond the earlier deindex + point-size work.

- The repository already contains a real Vulkan textured-triangle setup in `tests/VulkanBenchmarks/TriangleBenchmarks.cpp`; the shortest path for texture support is to reuse that descriptor/image infrastructure instead of inventing a fake texture system.

- The existing Vulkan descriptor payload is sufficient for a narrow bootstrap texture path: `SampledImageDescriptor` already contains prepared `sw::Texture`, dimensions, and `samplerId`, so the bootstrap only needs to copy level-0 texels plus a reduced sampler-state view.

- The texture benchmark already existed, but its fragment shader and descriptor write path used different binding numbers. Aligning both to binding 1 makes the benchmark exercise the same narrow texture path used by the new draw tests.

- The initial `linear/repeat` texture assertion failed because the chosen sample point assumed a near-white center mix. Actual CPU/CUDA output is a stable red/blue-heavy blend with low green at the center, so the robust test needs multiple sample points rather than a naive all-channels-high expectation.

- The original CPU-side test harness does not clear the non-multisample color attachment by default: `tests/VulkanWrapper/DrawTester.cpp` uses `AttachmentLoadOp::eDontCare` there. So undefined background is expected unless a test explicitly opts into clearing. The right fix is an opt-in clear path for tests, not changing the default semantics globally.

- For visible benchmarks, explicit clear is necessary for reliable visual output. Unlike unit tests, users interpret the window contents frame-to-frame, so leaving the background undefined makes the benchmark look broken even when draw results are technically valid. This is a harness-level presentation requirement, not a reason to globally change Vulkan clear semantics.

- Matching `noperspective` on both VS output and FS input was never the real issue; the root cause was glslang crashing on the `#version 310 es` + `noperspective` combination instead of emitting a compile error or valid SPIR-V.

- A narrow push-constant path is low-risk and high-yield for feature expansion: the harness and command buffer path already supported Vulkan push constants, so adding explicit test-side pipeline layout ranges/data was enough to unlock real VS/FS push-constant coverage without being blocked on `noperspective`.

- `gl_InstanceIndex` is an easy low-risk coverage gain even before custom bootstrap-specific lowering: the existing CPU/CUDA draw paths already render instanced geometry correctly through normal Vulkan command recording.

- `drawIndexed(..., vertexOffset, ...)` is another low-risk coverage gain: the existing CPU/CUDA draw paths already honor `baseVertex`, and the only real work needed here was choosing sample points inside the shifted triangle.

- The local sample scan revealed that `hello_triangle_1_3` already depends on dynamic rendering (`vkCmdBeginRendering`), so it should not be grouped with plain `hello_triangle` in planning. Repo-local `dynamic_rendering` coverage now exists (`DrawTest.DynamicRenderingSolidColorTriangle`), and Vulkan Samples `hello_triangle_1_3` now runs headless against SwiftShader.

- `separate_image_sampler` was initially blocked by a DrawTester limitation: the descriptor pool was hard-coded for `eCombinedImageSampler`, causing `vk::Device::allocateDescriptorSets` to throw `ErrorOutOfPoolMemory` for separate image/sampler layouts. After sizing the pool from the descriptor set layout bindings, `DrawTest.TexturedTriangleSeparateImageSamplerNearest` passes in both CPU and CUDA builds.

- `VK_VERTEX_INPUT_RATE_INSTANCE` is another low-risk compatibility gain: the underlying renderer already handles the Vulkan path correctly, and the main work was extending the test harness from one vertex buffer to a narrow two-binding case.

- `draw(..., firstInstance)` is another low-risk external-sample-aligned gain: the renderer already honors `gl_InstanceIndex` with non-zero firstInstance, and the only adjustment needed was choosing sample points inside the resulting shifted geometry.

- `dynamic_rendering` now has repo-local draw coverage via `DrawTest.DynamicRenderingSolidColorTriangle`; the DrawTester path uses `vkCmdBeginRendering`/`vkCmdEndRendering` with explicit swapchain-image layout transitions. External sample execution is still pending.

- The existing multisample harness path is already strong enough for an initial `msaa` sample-aligned gate. A simple resolved solid triangle passes in both CPU and CUDA builds without requiring new backend work.

- `dynamic_uniform_buffers` was initially blocked by a DrawTester harness gap: dynamic descriptor sets were bound without supplying dynamic offsets, and there was no per-draw dynamic-offset rebind hook. After adding `DrawTester::bindDescriptorSet(...dynamicOffsets...)` plus `DrawTest.DynamicUniformBufferOffsetsSelectPerDrawColor`, dynamic UBO offsets now have repo-local coverage in both CPU and CUDA builds.

- `vertex_dynamic_state` turned out to be a low-risk sample-aligned gain: SwiftShader already exposes `vkCmdSetVertexInputEXT`, so the main missing piece was test-harness support for the extension, feature enablement, and dynamic vertex-input command recording.

- `vertex_dynamic_state` is now covered beyond the trivial solid triangle: a multi-attribute interpolated-color triangle also passes through `vkCmdSetVertexInputEXT` in both CPU and CUDA builds, which increases confidence before broader sample-style expansion.

- A local `textureLod(..., 1.0)` + two-level mip draw probe does not produce the expected level-1 color even in the CPU baseline. That makes `texture_mipmap_generation` more than a simple bootstrap gap, and not part of the current low-risk feature lane.

- `vertex_dynamic_state` combines cleanly with `VK_VERTEX_INPUT_RATE_INSTANCE` in the current harness: the same dynamic binding/attribute path works for both per-vertex and per-instance inputs in CPU and CUDA builds.

- `vkCmdClearAttachments` is a good low-risk entry point for the `render_passes` sample family: it exercises in-render-pass attachment manipulation without immediately needing the harder subpass/lifecycle cases.

- `MSAA` remains a good low-risk lane: after the earlier solid-color case, a resolved interpolated-color triangle also passes in both CPU and CUDA builds, so resolve coverage is no longer limited to a constant fragment color.

- `loadOp = LOAD` on swapchain-backed color attachments is another practical low-risk entry point for the `render_passes` sample family; in the current harness it works cleanly in both CPU and CUDA builds when the images are explicitly initialized first.

- Depth-aspect `vkCmdClearAttachments` is another stable low-risk `render_passes` building block: in the current harness it cleanly gates later draw calls through depth test in both CPU and CUDA builds.

- The current harness now confirms that `baseVertex`, `firstInstance`, and `VK_VERTEX_INPUT_RATE_INSTANCE` compose correctly together in a single indexed instanced draw on both CPU and CUDA paths; the earlier failure was only a bad sample point.

- A narrow two-subpass render pass is still a low-risk addition in the current harness: reusing the same shaders/state with a second subpass-specific pipeline is enough to validate `vkCmdNextSubpass` sequencing in both CPU and CUDA builds.

- `vkCmdSetVertexInputEXT` composes cleanly with the existing indexed-instancing path: the current harness passes the combined `indexed + baseVertex + firstInstance + instance-rate` scenario in both CPU and CUDA builds.

- Configurable `minImageCount` is a low-risk way to strengthen the `swapchain_images` sample line: the current harness can request triple buffering cleanly and still render/present correctly in both CPU and CUDA builds.

- Depth `loadOp = LOAD` is stable in the current harness when images are initialized first: both CPU and CUDA builds preserve prior depth values strongly enough to reject a farther triangle on later frames.

- The current harness confirms that depth attachment contents persist correctly across subpasses: a nearer triangle written in subpass 0 still blocks a farther triangle drawn in subpass 1 on both CPU and CUDA paths.

- The root cause of the initial MSAA+depth failure was in the test harness, not the renderer: `beginRenderPass` used a too-short `clearValues` array and placed the depth clear value in the wrong slot when both resolve and depth attachments were present.

- The current triple-buffered swapchain path remains stable across successive presents: after requesting `minImageCount = 3`, both CPU and CUDA builds correctly present updated contents over multiple frames rather than only the first frame.

- The current harness can safely update a bound graphics descriptor set between frames without re-recording command buffers: both CPU and CUDA builds picked up the new combined image sampler contents on the next submit.

- The current combined-image-sampler path composes cleanly with instancing: both CPU and CUDA builds rendered two offset textured instances correctly using the existing narrow texture bootstrap path.

- The current narrow texture path composes cleanly with dynamic vertex input and instancing together: both CPU and CUDA builds rendered instanced textured triangles correctly under `vkCmdSetVertexInputEXT`.

- 2026-03-10 plan pivot: for the next stretch, the primary acceptance signal is SwiftShader's own built-in unit tests under the CUDA-enabled build, not external Vulkan-Samples runs.

- Per user direction, CPU-baseline comparison should not drive prioritization for this round. Treat the repository-owned test failures in the CUDA build as the actionable queue unless a failure clearly proves the test itself is invalid.

- The current `task_plan.md` had gone stale: it still described an older document-review task even though the repository state and `progress.md` already reflect substantial CUDA/backend test work. Planning files now need to track unit-test stabilization as the active objective.

- In `build-cuda-bootstrap/`, `ctest -N` reports `Total Tests: 0`. The actionable built-in test entry points are the standalone binaries such as `backend-unittests`, `draw-unittests`, and `vk-unittests`, not CTest registrations.

- First CUDA-built-in failure cluster from `./build-cuda-bootstrap/backend-unittests` is tightly grouped, not random:
  - `TrianglePipelineBootstrap.CudaRuntimeAppliesRequestedVertexGeometry`
  - `TrianglePipelineBootstrap.CudaRuntimeUsesRawVertexDataAndBinding`
  - `TrianglePipelineBootstrap.CudaRuntimeRendersMultipleTrianglesFromRawVertexData`
  - `TrianglePipelineBootstrap.CudaRuntimeAppliesFragCoordQuadrantFragmentMode`
  - `RasterBootstrap.CudaRuntimeMatchesCpuReference`
  - `RasterBootstrap.RasterFeedsFragmentBootstrap`

- The strongest current signal is in `RasterBootstrap.CudaRuntimeMatchesCpuReference`: for the same simple 8x8 triangle, CUDA output reports `28` fragment invocations while the CPU reference reports `10`. Downstream triangle-pipeline failures all look like “expected covered pixel stayed black”, which points more toward raster coverage / coordinate generation than toward fragment shading or basic CUDA runtime launch failures.

- Root-cause hypothesis for the first failure cluster is now concrete: the raster CUDA kernel writes `RasterInvocation`, but host code allocates and reads the buffer as `FragmentBootstrapInvocation`.
  - Device-side `RasterInvocation` in `src/Backend/RasterBootstrap.cpp` contains `x/y/exportMask/helperInvocation/frontFacing/barycentric0/1/2`.
  - Host-side `FragmentBootstrapInvocation` in `src/Backend/FragmentBootstrap.hpp` additionally contains `pointCoordX/pointCoordY` before the barycentrics.
  - That means the kernel uses a 32-byte stride while host code reads back with a 40-byte stride, which cleanly explains why coverage counts inflate (`28` vs `10`) and why downstream raster-fed triangle tests read incorrect covered pixels.

- The minimal root-cause fix is to restore layout parity at the raster/fragment boundary, not to special-case any individual failing test. Adding `pointCoordX/pointCoordY` to the raster CUDA-side invocation struct and zero-initializing them is sufficient because point rasterization already populates those fields elsewhere without using `runRasterBootstrap()`.

- `draw-unittests` in `build-cuda-bootstrap/` currently do not show a concrete failing test family. The earlier full-suite session died near `LineStripConstantColor` / later near `MultisampleSolidColorTriangle`, but those tests pass individually and the suite passes when sharded into smaller batches. For now, treat that as an execution-session limitation rather than a renderer or harness regression.

- The first concrete `vk-unittests` crash in the CUDA build is `ComputeBackendPipelineTest.BuildBackendExecutableWithoutDispatch`, exiting with signal 11 (`139`) before any assertion output.

- Root cause is a test-fixture control-flow bug, not a compute backend assertion failure:
  - `ComputeBackendPipelineTest::SetUpTestSuite()` calls `GTEST_SKIP()` under `SWIFTSHADER_CUSTOM_GPU_USE_CUDA` and therefore never calls `driver.loadSwiftShader()`.
  - GoogleTest still proceeds to run `BuildBackendExecutableWithoutDispatch`, which then dereferences the unresolved `driver` function table (`driver.vkCreateInstance(...)`) and segfaults.
  - The correct place to gate unsupported CUDA compute Vulkan tests is at the individual test level, while keeping suite setup responsible for initializing the shared driver fixture.

- The `vk-unittests` failure in `DrawTest.FragmentShaderDiscardsLeftHalfByFragCoord` is a test-stability bug, not a discard implementation regression. The left half of the frame is discarded and therefore preserves the color attachment's prior contents; with the default non-MSAA `eDontCare` load path, that background is undefined and can drift above the old `< 180` threshold after earlier tests.

- A stable fix for the discard case is to opt into an explicit color clear for that test and assert the known clear color on the discarded half. This matches the repository's existing `DrawTester::enableColorClear()` pattern and removes the sequence dependence.

- The CPU and CUDA draw tests use the same `DrawTester` / `VulkanTester` lifecycle harness; the CUDA-only repeat crash is therefore not explained by a different top-level test framework.

- Before this cycle, plain `DrawTester tester;` destruction was not safe because `DrawTester::~DrawTester()` and `VulkanTester::~VulkanTester()` unconditionally called Vulkan device methods even when `initialize()` had never run. Guarding those destructors is a valid harness hardening change and enables explicit construct/destruct-only coverage.

- The remaining CUDA repeat crash now has a tighter boundary:
  - `DrawTest.ConstructThenDestroyWithoutInitialize` repeats cleanly in both CPU and CUDA builds.
  - `DrawTest.InitializeThenDestroyWithoutRender` repeats cleanly in both CPU and CUDA builds.
  - `DrawTest.VertexShaderNoPositionOutput` and `DrawTest.SolidColorTriangle` still crash under CUDA repeats once `renderFrame()` is involved.
  - Therefore the active fault is in the draw submit/present path or post-submit teardown after a real frame, not in plain construction or initialization.

- `DrawTest.RenderWithoutPresentThenDestroy` also crashes under CUDA repeats while passing on CPU, so `queuePresent()` / surface presentation is not the leading suspect anymore. The remaining fault boundary is “real frame submit after swapchain acquire”, not “presentation only”.

- `DrawTest.InitializeThenDestroyWithoutRender` still triggers one CUDA launch in the current CUDA build, which is consistent with the existing runtime warmup path. That warmup-only launch is stable across repeats; the crashing cases trigger additional real draw-stage launches. So the remaining bug is not “any CUDA launch”, but something specific to the actual draw bootstrap/submit path.

- `DrawTest.AcquireWithoutSubmitThenDestroy` repeats cleanly in both CPU and CUDA builds. This removes `acquireNextImage()` and swapchain image acquisition state from the primary suspicion set. The remaining CUDA-only repeat crash now narrows to the actual submitted frame work after acquire, not to initialization, warmup, acquire, or present in isolation.

- The remaining CUDA-only draw repeat crash is narrower than “any submitted frame”:
  - `DrawTest.SubmitWithoutDrawThenDestroy` repeats cleanly in both CPU and CUDA builds.
  - `DrawTest.DrawZeroVerticesThenDestroy` repeats cleanly in both CPU and CUDA builds.
  - `DrawTest.DrawOneVertexThenDestroy` repeats cleanly in both CPU and CUDA builds.
  - `DrawTest.DrawTwoVerticesThenDestroy` repeats cleanly in both CPU and CUDA builds.
  - `DrawTest.RenderWithoutPresentThenDestroy` still crashes under CUDA repeats with the default 3-vertex triangle draw.
  - Therefore the current fault boundary is no longer generic submit, command buffer execution, render-pass setup, or VS-only/incomplete-primitive work; it first appears once the draw path forms a complete triangle primitive and enters the triangle assembly / raster / fragment portion of the pipeline.

- `DrawTest.DrawDegenerateTriangleThenDestroy` still crashes under CUDA repeats even though the triangle has zero area and should not produce meaningful covered fragments; the same case repeats cleanly on CPU.
  - A one-shot launch stamp probe with `SWIFTSHADER_CUDA_DISABLE_WARMUP=1` shows `DrawTest.DrawTwoVerticesThenDestroy` produces `0` stamped launches, while `DrawTest.DrawDegenerateTriangleThenDestroy` produces `3`.
  - Therefore the current boundary is narrower than “actual fragment coverage”: the first complete 3-vertex triangle primitive is enough to trigger the CUDA bootstrap launches and the later repeat crash, even when the primitive is degenerate.

- Root cause of the backend degenerate-triangle bug is now confirmed in `RasterBootstrap`:
  - CPU reference happened to return zero coverage for zero-area triangles because the computed bounding box collapses (`bboxMin > bboxMax`) and the coverage loops do not execute.
  - The CUDA raster path had no degenerate-triangle guard, so it launched over the full render target, `pointInsideTriangle()` treated the zero-area triangle as inside everywhere, and barycentric interpolation divided by zero denominators.
  - A dedicated backend test now reproduced that mismatch directly: `RasterBootstrap.CudaRuntimeRejectsDegenerateTriangleLikeCpuReference` failed before the fix and passes after adding an explicit zero-area early return in `runRasterBootstrap()`.

- After the degenerate-raster fix, the draw crash landscape changed:
  - `DrawTest.RenderWithoutPresentThenDestroy` now repeats cleanly for 25 iterations in the CUDA build.
  - `DrawTest.DrawDegenerateTriangleThenDestroy` still crashes under CUDA repeats, but its warmup-disabled launch count dropped from `3` to `1`, confirming the raster/fragment degenerate path is no longer being launched.
  - `DrawTest.SolidColorTriangle` still crashes under CUDA repeats (iteration 18 in the latest run).
  - `DrawTest.VertexShaderNoPositionOutput` still crashes under CUDA repeats and now emits repeated `Unsupported Descriptor Type` warnings from `VkDescriptorSetLayout.cpp`, which is a strong hint that a separate descriptor-layout / lifetime corruption issue remains.
