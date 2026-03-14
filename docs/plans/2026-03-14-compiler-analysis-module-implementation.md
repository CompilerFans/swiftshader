# Compiler Analysis Module Extraction Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Extract graphics-route compiler analysis into a standalone pipeline module and add layered compiler IR tests covering both supported and gated shader features.

**Architecture:** Introduce `ShaderCompilerAnalysis` in `src/Pipeline/` as the owner of general shader analysis results, make `GraphicsExecutable` consume that module, and grow tests in three layers: direct compiler analysis, KernelIR propagation, and emitter/ABI parity. Keep bootstrap-specific shader-template logic in backend code for this phase.

**Tech Stack:** C++17, SwiftShader Vulkan/runtime codebase, `SpirvShader`, GoogleTest backend unit tests, existing `SemanticIR` / `KernelIR` / emitter scaffolding.

---

### Task 1: Create RED tests for standalone compiler analysis

**Files:**
- Create: `tests/BackendUnitTests/SpirvToCompilerAnalysisTests.cpp`
- Modify: `tests/BackendUnitTests/CMakeLists.txt`
- Test: `tests/BackendUnitTests/SpirvToCompilerAnalysisTests.cpp`

**Step 1: Write the failing tests**

Add a new test file with cases grouped by:

- supported:
  - combined image sampler
  - separate image/sampler
  - descriptor array constant index
- gated/unsupported:
  - storage image read/write
  - discard
  - image query/fetch
  - derivatives
  - atomics
  - subgroup
  - buffer descriptors present
  - non-constant descriptor array element

Each test should build a minimal real shader fixture and assert compiler-analysis result fields such as:

- `fragmentFeatureMask`
- `unsupportedReasonMask`
- texture/resource/image plan shape

The tests should include `Pipeline/ShaderCompilerAnalysis.hpp`, which does not exist yet.

**Step 2: Run tests to verify they fail**

Run:

```bash
cmake --build build-cuda-bootstrap --target backend-unittests --parallel 1
./build-cuda-bootstrap/backend-unittests --gtest_filter='SpirvToCompilerAnalysis.*'
```

Expected:
- build RED because `ShaderCompilerAnalysis` does not exist yet

**Step 3: Write minimal implementation**

Add only the minimum build wiring needed so the new test target can see the forthcoming module files. Do not implement analysis behavior yet.

**Step 4: Run tests to verify the failure is now behavioral**

Run:

```bash
cmake --build build-cuda-bootstrap --target backend-unittests --parallel 1
./build-cuda-bootstrap/backend-unittests --gtest_filter='SpirvToCompilerAnalysis.*'
```

Expected:
- compile succeeds or gets further
- tests still RED because no real analysis has been implemented

**Step 5: Commit**

```bash
git add tests/BackendUnitTests/SpirvToCompilerAnalysisTests.cpp tests/BackendUnitTests/CMakeLists.txt
git commit -m "Tests: add RED compiler analysis coverage"
```

### Task 2: Add the standalone `ShaderCompilerAnalysis` module

**Files:**
- Create: `src/Pipeline/ShaderCompilerAnalysis.hpp`
- Create: `src/Pipeline/ShaderCompilerAnalysis.cpp`
- Modify: `src/Pipeline/CMakeLists.txt`
- Modify: `src/Pipeline/BUILD.gn`
- Test: `tests/BackendUnitTests/SpirvToCompilerAnalysisTests.cpp`

**Step 1: Write the failing test**

Reuse the RED tests from Task 1. The module should still be missing result types and entry points.

**Step 2: Run test to verify it fails**

Run:

```bash
./build-cuda-bootstrap/backend-unittests --gtest_filter='SpirvToCompilerAnalysis.*'
```

Expected:
- RED because the result/context API and analysis entry point are not implemented

**Step 3: Write minimal implementation**

Implement:

- `ShaderCompilerAnalysisContext`
- neutral result types:
  - `ShaderDescriptorRef`
  - `ShaderTexturePlan`
  - `ShaderImageResourcePlan`
  - `ShaderResourcePlan`
  - `ShaderFragmentFeature`
  - `ShaderUnsupportedReason`
  - `ShaderCompilerAnalysisResult`
- analysis entry point:
  - `analyzeGraphicsFragmentShader(const sw::SpirvShader &, const ShaderCompilerAnalysisContext &)`

Move only the general analysis logic out of `GraphicsExecutable.cpp`:

- feature mask
- sampled/storage descriptor collection
- texture plan
- image resource plan
- resource plan
- unsupported reason assembly

Do not move point-size or static bootstrap fragment-template analysis.

**Step 4: Run tests to verify they pass**

Run:

```bash
cmake --build build-cuda-bootstrap --target backend-unittests --parallel 1
./build-cuda-bootstrap/backend-unittests --gtest_filter='SpirvToCompilerAnalysis.*'
```

Expected:
- PASS for the new compiler-analysis tests

**Step 5: Commit**

```bash
git add src/Pipeline/ShaderCompilerAnalysis.hpp src/Pipeline/ShaderCompilerAnalysis.cpp src/Pipeline/CMakeLists.txt src/Pipeline/BUILD.gn tests/BackendUnitTests/SpirvToCompilerAnalysisTests.cpp
git commit -m "Pipeline: add standalone shader compiler analysis module"
```

### Task 3: Make `GraphicsExecutable` consume the new analysis module

**Files:**
- Modify: `src/Backend/GraphicsExecutable.hpp`
- Modify: `src/Backend/GraphicsExecutable.cpp`
- Modify: `tests/BackendUnitTests/GraphicsExecutableTests.cpp`
- Modify: `tests/VulkanUnitTests/GraphicsBackendPipelineTests.cpp`
- Modify: `tests/VulkanUnitTests/PipelineIntrospection.hpp`
- Modify: `tests/VulkanUnitTests/PipelineIntrospection.cpp`
- Test: `tests/BackendUnitTests/GraphicsExecutableTests.cpp`
- Test: `tests/VulkanUnitTests/GraphicsBackendPipelineTests.cpp`

**Step 1: Write the failing tests**

Add or tighten tests to ensure:

- `GraphicsExecutable` still exposes the same observable resource/feature/reason information
- Vulkan pipeline introspection still reports the same values for:
  - texture plan/resource plan
  - fragment feature mask
  - unsupported reason mask

**Step 2: Run tests to verify they fail**

Run:

```bash
cmake --build build-cuda-bootstrap --target backend-unittests vk-unittests --parallel 1
./build-cuda-bootstrap/backend-unittests --gtest_filter='GraphicsExecutable.*'
./build-cuda-bootstrap/vk-unittests --gtest_filter='GraphicsBackendPipeline.*'
```

Expected:
- RED because `GraphicsExecutable` still owns old analysis types/logic

**Step 3: Write minimal implementation**

Refactor `GraphicsExecutable` to:

- include `Pipeline/ShaderCompilerAnalysis.hpp`
- build a `ShaderCompilerAnalysisContext`
- call the new analysis entry point
- store or map analysis results into its existing public surface

Keep bootstrap-specific logic in `GraphicsExecutable.cpp`:

- point size inference
- static bootstrap fragment template inference

Do not expand compiler scope in this task beyond the extracted module.

**Step 4: Run tests to verify they pass**

Run:

```bash
./build-cuda-bootstrap/backend-unittests --gtest_filter='SpirvToCompilerAnalysis.*:GraphicsExecutable.*'
./build-cuda-bootstrap/vk-unittests --gtest_filter='GraphicsBackendPipeline.*'
```

Expected:
- all commands PASS

**Step 5: Commit**

```bash
git add src/Backend/GraphicsExecutable.hpp src/Backend/GraphicsExecutable.cpp tests/BackendUnitTests/GraphicsExecutableTests.cpp tests/VulkanUnitTests/GraphicsBackendPipelineTests.cpp tests/VulkanUnitTests/PipelineIntrospection.hpp tests/VulkanUnitTests/PipelineIntrospection.cpp
git commit -m "Backend: consume standalone compiler analysis in graphics executable"
```

### Task 4: Add Layer 2 `CompilerAnalysis -> KernelIR` tests

**Files:**
- Modify: `src/Pipeline/KernelIR.hpp`
- Modify: `src/Pipeline/KernelIRLowering.hpp`
- Modify: `tests/BackendUnitTests/KernelIRTests.cpp`
- Test: `tests/BackendUnitTests/KernelIRTests.cpp`

**Step 1: Write the failing tests**

Add tests proving that key compiler-analysis metadata survives lowering:

- feature mask presence
- unsupported reason mask presence
- basic resource/texture-plan presence markers

If `KernelIR` currently cannot store this metadata, the new tests should fail because fields are missing.

**Step 2: Run tests to verify they fail**

Run:

```bash
cmake --build build-cuda-bootstrap --target backend-unittests --parallel 1
./build-cuda-bootstrap/backend-unittests --gtest_filter='KernelIR.*'
```

Expected:
- RED because `KernelIR` does not yet carry the new analysis metadata

**Step 3: Write minimal implementation**

Extend `KernelIR` only enough to preserve analysis metadata needed by the new tests.

Avoid full shader lowering in this task. Keep the change to “metadata survives lowering”.

**Step 4: Run tests to verify they pass**

Run:

```bash
./build-cuda-bootstrap/backend-unittests --gtest_filter='SpirvToCompilerAnalysis.*:KernelIR.*'
```

Expected:
- PASS

**Step 5: Commit**

```bash
git add src/Pipeline/KernelIR.hpp src/Pipeline/KernelIRLowering.hpp tests/BackendUnitTests/KernelIRTests.cpp
git commit -m "Pipeline: preserve compiler analysis metadata in KernelIR"
```

### Task 5: Add Layer 3 emitter / ABI parity coverage

**Files:**
- Modify: `tests/BackendUnitTests/CodegenEmitterTests.cpp`
- Modify: `tests/BackendUnitTests/AbiParityTests.cpp`
- Test: `tests/BackendUnitTests/CodegenEmitterTests.cpp`
- Test: `tests/BackendUnitTests/AbiParityTests.cpp`

**Step 1: Write the failing tests**

Add tests showing that for the same analysis-derived IR input:

- CUDA-like emitter and LLVM IR emitter agree on normalized ABI shape
- capability/resource-related header fields do not drift

Keep the tests focused on ABI/metadata agreement, not on restating semantic analysis assertions.

**Step 2: Run tests to verify they fail**

Run:

```bash
cmake --build build-cuda-bootstrap --target backend-unittests --parallel 1
./build-cuda-bootstrap/backend-unittests --gtest_filter='CodegenEmitter.*:AbiParity.*'
```

Expected:
- RED if the new metadata is not exposed consistently through existing emitters/ABI descriptors

**Step 3: Write minimal implementation**

Update emitter/ABI helper code only enough for normalized ABI descriptions to include the newly preserved metadata required by the tests.

Do not over-expand the emitters in this task.

**Step 4: Run tests to verify they pass**

Run:

```bash
./build-cuda-bootstrap/backend-unittests --gtest_filter='SpirvToCompilerAnalysis.*:KernelIR.*:CodegenEmitter.*:AbiParity.*'
```

Expected:
- PASS

**Step 5: Commit**

```bash
git add tests/BackendUnitTests/CodegenEmitterTests.cpp tests/BackendUnitTests/AbiParityTests.cpp
git commit -m "Tests: add compiler analysis ABI parity coverage"
```

### Task 6: Focused regression verification and docs sync

**Files:**
- Modify: `findings.md`
- Modify: `progress.md`
- Modify: `task_plan.md`
- Test: existing focused suites

**Step 1: Write the failing check if needed**

No new feature test here; this task is verification and project-memory sync.

**Step 2: Run focused verification**

Run:

```bash
cmake --build build-cuda-bootstrap --target backend-unittests vk-unittests --parallel 1
./build-cuda-bootstrap/backend-unittests --gtest_filter='SpirvToCompilerAnalysis.*:GraphicsExecutable.*:KernelIR.*:CodegenEmitter.*:AbiParity.*'
./build-cuda-bootstrap/vk-unittests --gtest_filter='GraphicsBackendPipeline.*'
git diff --check
```

Expected:
- all commands PASS

**Step 3: Write minimal documentation updates**

Update project memory files with:

- the new standalone compiler-analysis module
- which features are covered in which layer
- what remains bootstrap-specific and intentionally outside the new module

**Step 4: Re-run verification if docs touched code-adjacent files**

Run:

```bash
git diff --check
```

Expected:
- PASS

**Step 5: Commit**

```bash
git add findings.md progress.md task_plan.md
git commit -m "Docs: record standalone compiler analysis extraction"
```
