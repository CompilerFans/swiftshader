# Graphics Executable Texture Bootstrap Narrowing Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 把当前 texture-bootstrap metadata 的适用边界收紧到 `location 0 == vec2 texcoord` 的窄路径。

**Architecture:** `GraphicsExecutable` 的 texture metadata 提取逻辑改为要求 fragment input location 0 恰好有 2 个分量。Vulkan integration test 负责覆盖 positive 和 negative pipeline cases。

**Tech Stack:** C++17, SwiftShader Vulkan runtime, GoogleTest

---

### Task 1: Add failing negative pipeline test

**Files:**
- Modify: `tests/VulkanUnitTests/GraphicsBackendPipelineTests.cpp`

**Step 1: Write the failing test**

- add a pipeline where fragment input location 0 is color and location 1 is texcoord
- expect no texture bootstrap binding metadata

**Step 2: Run test to verify it fails**

Run:
- `cmake --build build-cuda-bootstrap --target vk-unittests --parallel 1`
- `./build-cuda-bootstrap/vk-unittests --gtest_filter='GraphicsBackendPipeline.*'`

Expected: FAIL because current texture metadata extraction is too permissive

### Task 2: Narrow texture metadata extraction

**Files:**
- Modify: `src/Backend/GraphicsExecutable.cpp`

**Step 1: Write minimal implementation**

- require `getNumInputComponentsAtLocation(shader, 0) == 2` in `tryGetBootstrapTextureBinding(...)`

**Step 2: Run focused validation**

Run:
- `cmake --build build-cuda-bootstrap --target vk-unittests backend-unittests draw-unittests --parallel 1`
- `./build-cuda-bootstrap/vk-unittests --gtest_filter='GraphicsBackendPipeline.*:BackendSmoke.*'`
- `./build-cuda-bootstrap/backend-unittests --gtest_filter=GraphicsExecutable.*`
- `./build-cuda-bootstrap/draw-unittests --gtest_filter=DrawTest.SolidColorTriangle`

Expected: PASS

### Task 3: Update tracking docs

**Files:**
- Modify: `task_plan.md`
- Modify: `progress.md`
- Modify: `findings.md`
- Modify: `docs/BackendBringup.md`

**Step 1: Record the boundary**

- narrow texture bootstrap now explicitly means `location 0 == vec2` texcoord

**Step 2: Re-run broader verification**

Run:
- `./build-cuda-bootstrap/backend-unittests`
- `./build-cuda-bootstrap/vk-unittests --gtest_filter='GraphicsBackendPipeline.*:BackendSmoke.*:GraphicsBackendSelection.*'`

Expected: PASS
