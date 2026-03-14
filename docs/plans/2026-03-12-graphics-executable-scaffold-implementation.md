# Graphics Executable Scaffold Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 给 graphics pipeline 增加 metadata-only backend executable 脚手架，并接到 `vk::GraphicsPipeline`。

**Architecture:** backend 新增 `GraphicsExecutable` 作为 graphics pipeline 的最小元数据容器。pipeline 创建期通过 `SemanticIRBuilder` 提取 vertex/fragment semantic modules，并在存在 vertex stage 时创建 backend executable。draw 路径保持不变。

**Tech Stack:** C++17, SwiftShader Vulkan runtime, SemanticIRBuilder, GoogleTest

---

### Task 1: Add failing backend tests for GraphicsExecutable

**Files:**
- Create: `tests/BackendUnitTests/GraphicsExecutableTests.cpp`
- Modify: `tests/BackendUnitTests/CMakeLists.txt`

**Step 1: Write the failing test**

```cpp
TEST(GraphicsExecutable, PreservesVertexLoweringAndOptionalFragmentStage)
{
    // Includes Backend/GraphicsExecutable.hpp which does not exist yet.
}
```

**Step 2: Run test to verify it fails**

Run: `cmake --build build-cuda-bootstrap --target backend-unittests --parallel 1`
Expected: FAIL because `GraphicsExecutable.hpp/.cpp` do not exist

**Step 3: Write minimal implementation**

- Add `backend::GraphicsExecutable`
- Allow `vertex + optional fragment`
- Reject missing vertex / wrong stage combinations

**Step 4: Run focused test**

Run: `./build-cuda-bootstrap/backend-unittests --gtest_filter=GraphicsExecutable.*`
Expected: PASS

### Task 2: Add failing graphics pipeline hookup test

**Files:**
- Create: `tests/VulkanUnitTests/GraphicsBackendPipelineTests.cpp`
- Modify: `tests/VulkanUnitTests/CMakeLists.txt`
- Modify: `tests/VulkanWrapper/DrawTester.hpp`

**Step 1: Write the failing test**

```cpp
TEST(GraphicsBackendPipeline, BuildsBackendExecutableForGraphicsPipeline)
{
    // Calls DrawTester::getPipelineHandle(), which does not exist yet.
}
```

**Step 2: Run test to verify it fails**

Run: `cmake --build build-cuda-bootstrap --target vk-unittests --parallel 1`
Expected: FAIL because pipeline getter / graphics backend executable path are missing

**Step 3: Write minimal implementation**

- Expose a read-only pipeline handle getter in `DrawTester`
- Add `GraphicsPipeline::hasBackendExecutable()`
- Build backend executable during `GraphicsPipeline::compileShaders()`

**Step 4: Run focused test**

Run: `./build-cuda-bootstrap/vk-unittests --gtest_filter=GraphicsBackendPipeline.*`
Expected: PASS

### Task 3: Wire backend executable into graphics pipeline

**Files:**
- Create: `src/Backend/GraphicsExecutable.hpp`
- Create: `src/Backend/GraphicsExecutable.cpp`
- Modify: `src/Backend/CMakeLists.txt`
- Modify: `src/Backend/BUILD.gn`
- Modify: `src/Vulkan/VkPipeline.hpp`
- Modify: `src/Vulkan/VkPipeline.cpp`

**Step 1: Write minimal implementation**

- Create backend executable from semantic modules
- Store vertex lowering metadata
- Reset executable in pipeline destroy

**Step 2: Run focused validation**

Run:
- `cmake --build build-cuda-bootstrap --target backend-unittests vk-unittests --parallel 1`
- `./build-cuda-bootstrap/backend-unittests --gtest_filter=GraphicsExecutable.*`
- `./build-cuda-bootstrap/vk-unittests --gtest_filter=GraphicsBackendPipeline.*:BackendSmoke.*`

Expected: PASS

### Task 4: Update tracking docs

**Files:**
- Modify: `task_plan.md`
- Modify: `progress.md`
- Modify: `findings.md`

**Step 1: Record new slice**

- note that draw routing extraction is complete
- note that current slice is graphics executable scaffold
- record remaining follow-ups after scaffold

**Step 2: Re-run smoke verification**

Run:
- `./build-cuda-bootstrap/backend-unittests`
- `./build-cuda-bootstrap/vk-unittests --gtest_filter='GraphicsBackendPipeline.*:BackendSmoke.*:GraphicsBackendSelection.*'`

Expected: PASS
