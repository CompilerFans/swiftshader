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
