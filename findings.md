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
