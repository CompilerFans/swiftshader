# Graphics Executable Sampled-Image Provenance Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Make `GraphicsExecutable` classify sampled-image resource shape from the descriptors that actually feed texture sampling instructions, ignoring unrelated fragment-shader descriptors.

**Architecture:** Extend `GraphicsExecutable.cpp` with sample-operand provenance helpers that recover the descriptor objects behind each texture sample. Use those recovered sampled-image descriptors to build the texture plan, while leaving bootstrap-output eligibility rules unchanged.

**Tech Stack:** C++, GoogleTest, Vulkan unit tests, SwiftShader GLSL/SPIR-V compilation

---

### Task 1: Add failing Vulkan regression coverage

**Files:**
- Modify: `tests/VulkanUnitTests/GraphicsBackendPipelineTests.cpp`

**Step 1: Write the failing tests**
- Add a combined-image-sampler pipeline with an unrelated UBO used for control flow.
- Add a separate image/sampler pipeline with an unrelated UBO used for control flow.
- Assert the texture plan ignores the UBO and still classifies the sampled-image resource correctly.

**Step 2: Run the focused tests to verify RED**
Run: `cmake --build build-cuda-bootstrap --target vk-unittests --parallel 1 && ./build-cuda-bootstrap/vk-unittests --gtest_filter='GraphicsBackendPipeline.IgnoresNonSampledDescriptorsWhenClassifyingCombinedTexturePlan:GraphicsBackendPipeline.IgnoresNonSampledDescriptorsWhenClassifyingSeparateTexturePlan'`
Expected: FAIL because unrelated descriptors currently force the plan to `Other`.

### Task 2: Implement sample-use provenance collection

**Files:**
- Modify: `src/Backend/GraphicsExecutable.cpp`

**Step 1: Resolve descriptor provenance from sample operands**
- Add helper logic that walks backward from a sample instruction operand through:
  - `OpSampledImage`
  - `OpCopyObject` / `OpCopyLogical`
  - function-local `OpLoad` from a trivially stored value
- Fall back to descriptor decorations only for the actual sampled-image operand when it represents a combined image sampler.

**Step 2: Rebuild texture-plan classification from sample-fed descriptors only**
- Replace the old “collect every descriptor decoration in the fragment shader” logic.
- Keep plan classification rules the same once the sampled-image descriptor set is known.
- Keep bootstrap support rules unchanged.

### Task 3: Verify focused and broader coverage

**Files:**
- Modify: `task_plan.md`
- Modify: `progress.md`
- Modify: `findings.md`
- Modify: `docs/BackendBringup.md`

**Step 1: Run focused verification**
Run:
- `./build-cuda-bootstrap/vk-unittests --gtest_filter='GraphicsBackendPipeline.IgnoresNonSampledDescriptorsWhenClassifyingCombinedTexturePlan:GraphicsBackendPipeline.IgnoresNonSampledDescriptorsWhenClassifyingSeparateTexturePlan'`

**Step 2: Run broader verification**
Run:
- `cmake --build build-cuda-bootstrap --target vk-unittests backend-unittests draw-unittests --parallel 1`
- `./build-cuda-bootstrap/vk-unittests --gtest_filter='GraphicsBackendPipeline.*:BackendSmoke.*:GraphicsBackendSelection.*'`
- `./build-cuda-bootstrap/backend-unittests --gtest_filter=GraphicsExecutable.*`
- `./build-cuda-bootstrap/draw-unittests --gtest_filter=DrawTest.SolidColorTriangle`
- `./build-cuda-bootstrap/backend-unittests`
- `git diff --check`
