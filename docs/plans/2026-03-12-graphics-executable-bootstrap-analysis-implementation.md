# Graphics Executable Bootstrap Analysis Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 把 triangle bootstrap 的静态 shader 分析迁入 `GraphicsExecutable`，让 draw helper 改为消费 pipeline-time metadata。

**Architecture:** `GraphicsExecutable` 增加 bootstrap metadata，并在 graphics pipeline 创建期利用 `SpirvShader` 完成静态分析。`TriangleBootstrapDraw` 只保留 draw-time descriptor 相关 texture 物化和 launch 逻辑。

**Tech Stack:** C++17, SwiftShader Vulkan runtime, SpirvShader analysis, GoogleTest, SPIRV-Tools

---

### Task 1: Add failing bootstrap-analysis tests

**Files:**
- Modify: `tests/BackendUnitTests/GraphicsExecutableTests.cpp`

**Step 1: Write the failing test**

```cpp
TEST(GraphicsExecutable, ExtractsBootstrapPointSizeFromVertexShader)
{
    // Calls executable->bootstrapPointSize(), which does not exist yet.
}
```

**Step 2: Run test to verify it fails**

Run: `cmake --build build-cuda-bootstrap --target backend-unittests --parallel 1`
Expected: FAIL because bootstrap analysis accessors / create-info path do not exist

**Step 3: Write minimal implementation**

- add bootstrap metadata fields and accessors to `GraphicsExecutable`
- analyze real `SpirvShader` objects when provided

**Step 4: Run focused test**

Run: `./build-cuda-bootstrap/backend-unittests --gtest_filter=GraphicsExecutable.*`
Expected: PASS

### Task 2: Switch triangle bootstrap helper to executable metadata

**Files:**
- Modify: `src/Backend/GraphicsExecutable.hpp`
- Modify: `src/Backend/GraphicsExecutable.cpp`
- Modify: `src/Vulkan/VkPipeline.hpp`
- Modify: `src/Vulkan/VkPipeline.cpp`
- Modify: `src/Backend/TriangleBootstrapDraw.cpp`

**Step 1: Write the failing integration expectation**

Use the backend tests from Task 1 plus existing draw smoke as the regression oracle.

**Step 2: Implement minimal plumbing**

- add `GraphicsPipeline` getter for backend executable
- pass `SpirvShader` pointers into `GraphicsExecutable` creation
- remove direct vertex point-size / non-texture fragment bootstrap analysis from `TriangleBootstrapDraw`

**Step 3: Run focused validation**

Run:
- `cmake --build build-cuda-bootstrap --target backend-unittests draw-unittests vk-unittests --parallel 1`
- `./build-cuda-bootstrap/backend-unittests --gtest_filter=GraphicsExecutable.*`
- `./build-cuda-bootstrap/draw-unittests --gtest_filter=DrawTest.SolidColorTriangle`
- `./build-cuda-bootstrap/vk-unittests --gtest_filter='GraphicsBackendPipeline.*:BackendSmoke.*'`

Expected: PASS

### Task 3: Update tracking docs

**Files:**
- Modify: `task_plan.md`
- Modify: `progress.md`
- Modify: `findings.md`
- Modify: `docs/BackendBringup.md`

**Step 1: Record the slice**

- `GraphicsExecutable` now owns bootstrap metadata
- `TriangleBootstrapDraw` now consumes executable metadata instead of re-analyzing shaders

**Step 2: Re-run broader verification**

Run:
- `./build-cuda-bootstrap/backend-unittests`
- `./build-cuda-bootstrap/draw-unittests --gtest_filter=DrawTest.SolidColorTriangle`
- `./build-cuda-bootstrap/vk-unittests --gtest_filter='GraphicsBackendPipeline.*:BackendSmoke.*:GraphicsBackendSelection.*'`

Expected: PASS
