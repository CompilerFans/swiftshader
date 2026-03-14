# Graphics Executable Sampled-Image Plan Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Replace the old texture-bootstrap binding-only metadata with a broader sampled-image resource plan while preserving the current bootstrap execution behavior.

**Architecture:** `GraphicsExecutable` becomes the owner of sampled-image resource-shape metadata. `TriangleBootstrapDraw` keeps consuming only the compatibility accessor for the currently supported combined-image-sampler bootstrap path. Vulkan pipeline tests verify resource shape and support status through `PipelineIntrospection`.

**Tech Stack:** C++, GoogleTest, Vulkan unit tests, SwiftShader GLSL/SPIR-V compilation

---

### Task 1: Promote texture metadata to a sampled-image plan

**Files:**
- Modify: `src/Backend/GraphicsExecutable.hpp`
- Modify: `src/Backend/GraphicsExecutable.cpp`

**Step 1: Replace the old binding-only representation**
- Add a texture resource kind enum.
- Add a texture plan struct that stores resource shape plus the descriptor locations needed by current/future materialization.
- Keep `hasBootstrapTextureBinding()` as a derived compatibility accessor.

**Step 2: Build the plan during executable creation**
- Classify at least these shapes:
  - one `COMBINED_IMAGE_SAMPLER`
  - one `SAMPLED_IMAGE` + one `SAMPLER`
  - anything more complex as `Other`
- Preserve the existing narrow bootstrap support rule for the direct-sample combined-image-sampler case.

### Task 2: Expose the plan to test introspection

**Files:**
- Modify: `tests/VulkanUnitTests/PipelineIntrospection.hpp`
- Modify: `tests/VulkanUnitTests/PipelineIntrospection.cpp`
- Modify: `tests/VulkanWrapper/DrawTester.hpp`

**Step 1: Extend the introspection bridge**
- Return both texture-plan shape and bootstrap-binding compatibility state.
- Add a stable pipeline-address helper so tests do not depend on raw-handle casts.

**Step 2: Keep test dependencies narrow**
- Do not include backend internal metadata headers directly in the Vulkan test translation unit.
- Keep the bridge as the only place that knows about backend executable internals.

### Task 3: Expand Vulkan pipeline coverage to plan-level assertions

**Files:**
- Modify: `tests/VulkanUnitTests/GraphicsBackendPipelineTests.cpp`

**Step 1: Update existing texture tests**
- Assert `CombinedImageSampler` for the supported and narrow-negative combined-sampler cases.
- Assert `SeparateImageSampler` for the separate image/sampler case.

**Step 2: Add broader sampled-image shape coverage**
- Add a real graphics pipeline test with multiple combined image samplers.
- Assert that it classifies as `Other` and remains bootstrap-unsupported.

### Task 4: Update tracking docs and verify broadly

**Files:**
- Modify: `task_plan.md`
- Modify: `progress.md`
- Modify: `findings.md`
- Modify: `docs/BackendBringup.md`

**Step 1: Record the new boundary**
- State explicitly that `GraphicsExecutable` now models sampled-image resource shape separately from bootstrap support.

**Step 2: Run verification**
Run:
- `cmake --build build-cuda-bootstrap --target vk-unittests backend-unittests draw-unittests --parallel 1`
- `./build-cuda-bootstrap/vk-unittests --gtest_filter='GraphicsBackendPipeline.*:BackendSmoke.*:GraphicsBackendSelection.*'`
- `./build-cuda-bootstrap/backend-unittests --gtest_filter=GraphicsExecutable.*`
- `./build-cuda-bootstrap/draw-unittests --gtest_filter=DrawTest.SolidColorTriangle`
- `./build-cuda-bootstrap/backend-unittests`
- `git diff --check`
