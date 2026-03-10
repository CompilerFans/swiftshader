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
- The current CPU baseline crashes on the `noperspective` draw test in both CPU-only and CUDA-enabled builds. Until that upstream CPU-path issue is understood, `noperspective` should be treated as a blocked parity item rather than forced through the bootstrap path.
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

- A minimal `noperspective` varying-color draw case still crashes in both the CPU baseline and the CUDA build before producing a frame. This remains a real blocker and should stay out of committed test suites until the crash is root-caused.

- `indexed POINT_LIST` composes cleanly not only with constant-color output but also with the existing `PointCoordGradient` fragment path; no extra runtime changes were required beyond the earlier deindex + point-size work.

- The repository already contains a real Vulkan textured-triangle setup in `tests/VulkanBenchmarks/TriangleBenchmarks.cpp`; the shortest path for texture support is to reuse that descriptor/image infrastructure instead of inventing a fake texture system.

- The existing Vulkan descriptor payload is sufficient for a narrow bootstrap texture path: `SampledImageDescriptor` already contains prepared `sw::Texture`, dimensions, and `samplerId`, so the bootstrap only needs to copy level-0 texels plus a reduced sampler-state view.

- The texture benchmark already existed, but its fragment shader and descriptor write path used different binding numbers. Aligning both to binding 1 makes the benchmark exercise the same narrow texture path used by the new draw tests.

- The initial `linear/repeat` texture assertion failed because the chosen sample point assumed a near-white center mix. Actual CPU/CUDA output is a stable red/blue-heavy blend with low green at the center, so the robust test needs multiple sample points rather than a naive all-channels-high expectation.

- The original CPU-side test harness does not clear the non-multisample color attachment by default: `tests/VulkanWrapper/DrawTester.cpp` uses `AttachmentLoadOp::eDontCare` there. So undefined background is expected unless a test explicitly opts into clearing. The right fix is an opt-in clear path for tests, not changing the default semantics globally.

- For visible benchmarks, explicit clear is necessary for reliable visual output. Unlike unit tests, users interpret the window contents frame-to-frame, so leaving the background undefined makes the benchmark look broken even when draw results are technically valid. This is a harness-level presentation requirement, not a reason to globally change Vulkan clear semantics.

- Matching `noperspective` on both VS output and FS input does not unblock the local reproducer. The issue remains inside the renderer/compiler path rather than being explained away by qualifier mismatch alone.

- A narrow push-constant path is low-risk and high-yield for feature expansion: the harness and command buffer path already supported Vulkan push constants, so adding explicit test-side pipeline layout ranges/data was enough to unlock real VS/FS push-constant coverage without touching the `noperspective` blocker.

- `gl_InstanceIndex` is an easy low-risk coverage gain even before custom bootstrap-specific lowering: the existing CPU/CUDA draw paths already render instanced geometry correctly through normal Vulkan command recording.

- `drawIndexed(..., vertexOffset, ...)` is another low-risk coverage gain: the existing CPU/CUDA draw paths already honor `baseVertex`, and the only real work needed here was choosing sample points inside the shifted triangle.

- The local sample scan revealed that `hello_triangle_1_3` already depends on dynamic rendering (`vkCmdBeginRendering`), so it should not be grouped with plain `hello_triangle` in planning. That makes dynamic rendering a higher-priority missing capability than the previous rough roadmap suggested.

- A minimal `separate image + sampler` draw case currently crashes in both CPU and CUDA builds. That makes `separate_image_sampler` a real missing capability rather than just an unimplemented bootstrap optimization.

- `VK_VERTEX_INPUT_RATE_INSTANCE` is another low-risk compatibility gain: the underlying renderer already handles the Vulkan path correctly, and the main work was extending the test harness from one vertex buffer to a narrow two-binding case.

- A minimal `separate image + sampler` draw probe currently crashes in both CPU and CUDA builds, so `separate_image_sampler` remains a real missing capability rather than a bootstrap-only optimization gap.

- `separate image + sampler` has now been explicitly confirmed as a blocker: a minimal local reproducer crashes in both CPU and CUDA builds, so we should not keep circling back to it until it becomes a dedicated investigation item.

- `draw(..., firstInstance)` is another low-risk external-sample-aligned gain: the renderer already honors `gl_InstanceIndex` with non-zero firstInstance, and the only adjustment needed was choosing sample points inside the resulting shifted geometry.

- A minimal `dynamic rendering` draw probe currently crashes in both CPU and CUDA builds. This makes `hello_triangle_1_3` / `dynamic_rendering` a real missing capability, not just an untested path.

- The existing multisample harness path is already strong enough for an initial `msaa` sample-aligned gate. A simple resolved solid triangle passes in both CPU and CUDA builds without requiring new backend work.

- A minimal `dynamic uniform buffer` graphics draw probe currently fails before producing the expected output in both CPU and CUDA builds. This makes `dynamic_uniform_buffers` another real missing capability, not a low-risk extension of the current short-term path.

- `vertex_dynamic_state` turned out to be a low-risk sample-aligned gain: SwiftShader already exposes `vkCmdSetVertexInputEXT`, so the main missing piece was test-harness support for the extension, feature enablement, and dynamic vertex-input command recording.

- `vertex_dynamic_state` is now covered beyond the trivial solid triangle: a multi-attribute interpolated-color triangle also passes through `vkCmdSetVertexInputEXT` in both CPU and CUDA builds, which increases confidence before broader sample-style expansion.

- A local `textureLod(..., 1.0)` + two-level mip draw probe does not produce the expected level-1 color even in the CPU baseline. That makes `texture_mipmap_generation` more than a simple bootstrap gap, and not part of the current low-risk feature lane.

- `vertex_dynamic_state` combines cleanly with `VK_VERTEX_INPUT_RATE_INSTANCE` in the current harness: the same dynamic binding/attribute path works for both per-vertex and per-instance inputs in CPU and CUDA builds.

- `vkCmdClearAttachments` is a good low-risk entry point for the `render_passes` sample family: it exercises in-render-pass attachment manipulation without immediately needing the harder subpass/lifecycle cases.
