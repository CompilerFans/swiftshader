# Graphics Executable Texture Bootstrap Separate-Image-Sampler Boundary Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add explicit regression coverage that separate image/sampler descriptors are outside the current texture-bootstrap metadata path.

**Architecture:** Reuse the existing `GraphicsBackendPipelineTests` + `PipelineIntrospection` harness to compile a real graphics pipeline with separate image/sampler bindings and assert that `GraphicsExecutable` does not expose a bootstrap texture binding.

**Tech Stack:** C++, GoogleTest, Vulkan unit tests, SwiftShader GLSL/SPIR-V compilation

---

### Task 1: Add the negative Vulkan pipeline test

**Files:**
- Modify: `tests/VulkanUnitTests/GraphicsBackendPipelineTests.cpp`

**Step 1: Write the test**
- Add a pipeline helper using separate image/sampler fragment bindings.
- Assert `graphicsPipelineBootstrapTextureState(...).hasBinding == false`.

**Step 2: Run focused test**
Run: `cmake --build build-cuda-bootstrap --target vk-unittests --parallel 1 && ./build-cuda-bootstrap/vk-unittests --gtest_filter='GraphicsBackendPipeline.RejectsTextureBootstrapBindingForSeparateImageSampler'`
Expected: PASS

### Task 2: Update tracking docs and verify broader filter

**Files:**
- Modify: `task_plan.md`
- Modify: `progress.md`
- Modify: `findings.md`
- Modify: `docs/BackendBringup.md`

**Step 1: Record the boundary**
- State explicitly that the current narrow texture-bootstrap path excludes separate image/sampler resources.

**Step 2: Run verification**
Run:
- `cmake --build build-cuda-bootstrap --target vk-unittests --parallel 1`
- `./build-cuda-bootstrap/vk-unittests --gtest_filter='GraphicsBackendPipeline.*:BackendSmoke.*:GraphicsBackendSelection.*'`
- `git diff --check`
