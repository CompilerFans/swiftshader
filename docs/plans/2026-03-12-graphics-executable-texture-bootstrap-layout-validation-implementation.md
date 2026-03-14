# Graphics Executable Texture Bootstrap Layout Validation Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 让 `GraphicsExecutable` 的 texture-bootstrap metadata 同时校验 descriptor layout 形状，并拒绝当前不支持的 descriptor array。

**Architecture:** `vk::GraphicsPipeline::compileShaders()` 把 `PipelineLayout` 传给 `GraphicsExecutableCreateInfo`。`GraphicsExecutable::create(...)` 在 texture metadata 提取时同时检查 shader 条件和 layout 条件，当前只接受 `COMBINED_IMAGE_SAMPLER` 且 `descriptorCount == 1` 的 binding。

**Tech Stack:** C++17, SwiftShader Vulkan runtime, GoogleTest

---

### Task 1: Add failing Vulkan integration test for descriptor arrays

**Files:**
- Modify: `tests/VulkanUnitTests/GraphicsBackendPipelineTests.cpp`

**Step 1: Write the failing test**

- add a textured graphics pipeline where the fragment shader uses `sampler2D texSampler[2]`
- keep `location 0` as `vec2`
- use a descriptor set layout binding with:
  - `binding = 1`
  - `descriptorType = eCombinedImageSampler`
  - `descriptorCount = 2`
- expect `graphicsPipelineBootstrapTextureState(...).hasBinding == false`

**Step 2: Run test to verify it fails**

Run:
- `cmake --build build-cuda-bootstrap --target vk-unittests --parallel 1`
- `./build-cuda-bootstrap/vk-unittests --gtest_filter='GraphicsBackendPipeline.*'`

Expected: FAIL because current metadata extraction does not look at `descriptorCount`

### Task 2: Make texture metadata extraction layout-aware

**Files:**
- Modify: `src/Backend/GraphicsExecutable.hpp`
- Modify: `src/Backend/GraphicsExecutable.cpp`
- Modify: `src/Vulkan/VkPipeline.cpp`

**Step 1: Write minimal implementation**

- extend `GraphicsExecutableCreateInfo` with `const vk::PipelineLayout *pipelineLayout`
- pass `layout` from `GraphicsPipeline::compileShaders()`
- in `tryGetBootstrapTextureBinding(...)`, after shader-based filtering:
  - validate `pipelineLayout != nullptr`
  - validate descriptor set / binding index ranges
  - validate descriptor type is `VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER`
  - validate `pipelineLayout->getDescriptorCount(set, binding) == 1`

**Step 2: Run focused verification**

Run:
- `cmake --build build-cuda-bootstrap --target vk-unittests backend-unittests draw-unittests --parallel 1`
- `./build-cuda-bootstrap/vk-unittests --gtest_filter='GraphicsBackendPipeline.*:BackendSmoke.*'`
- `./build-cuda-bootstrap/backend-unittests --gtest_filter=GraphicsExecutable.*`
- `./build-cuda-bootstrap/draw-unittests --gtest_filter=DrawTest.SolidColorTriangle`

Expected: PASS

### Task 3: Update tracking docs and run broader verification

**Files:**
- Modify: `task_plan.md`
- Modify: `progress.md`
- Modify: `findings.md`
- Modify: `docs/BackendBringup.md`

**Step 1: Record the new narrow-path boundary**

- texture bootstrap metadata now also requires the descriptor layout binding to have `descriptorCount == 1`

**Step 2: Re-run broader verification**

Run:
- `./build-cuda-bootstrap/backend-unittests`
- `./build-cuda-bootstrap/vk-unittests --gtest_filter='GraphicsBackendPipeline.*:BackendSmoke.*:GraphicsBackendSelection.*'`
- `git diff --check`

Expected: PASS
