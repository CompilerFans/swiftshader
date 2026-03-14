# Graphics Executable Texture Bootstrap Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 把 texture bootstrap 的 shader metadata 迁进 `GraphicsExecutable`，让 draw helper 只消费 executable metadata 和 draw-time descriptor state。

**Architecture:** `GraphicsExecutable` 在 pipeline 创建期识别 narrow texture bootstrap shader，并记录 descriptor set / binding。`TriangleBootstrapDraw` 只用这段 metadata 去当前 descriptor sets 中物化 texture config。

**Tech Stack:** C++17, SwiftShader Vulkan runtime, SpirvShader header-level analysis, GoogleTest

---

### Task 1: Add failing texture-metadata tests

**Files:**
- Modify: `tests/BackendUnitTests/GraphicsExecutableTests.cpp`
- Modify: `tests/VulkanUnitTests/PipelineIntrospection.hpp`
- Modify: `tests/VulkanUnitTests/PipelineIntrospection.cpp`
- Modify: `tests/VulkanUnitTests/GraphicsBackendPipelineTests.cpp`

**Step 1: Write the failing tests**

- backend unit test: no texture metadata by default
- Vulkan integration test: combined-image-sampler pipeline exposes texture bootstrap binding metadata

**Step 2: Run tests to verify they fail**

Run:
- `cmake --build build-cuda-bootstrap --target backend-unittests vk-unittests --parallel 1`

Expected: FAIL because texture metadata accessors / introspection hooks do not exist yet

### Task 2: Move texture binding metadata into GraphicsExecutable

**Files:**
- Modify: `src/Backend/GraphicsExecutable.hpp`
- Modify: `src/Backend/GraphicsExecutable.cpp`
- Modify: `src/Backend/TriangleBootstrapDraw.cpp`

**Step 1: Implement minimal metadata**

- add texture bootstrap binding struct + accessors
- analyze fragment shader at pipeline time for narrow texture bootstrap eligibility
- keep existing static fragment bootstrap metadata intact

**Step 2: Switch draw helper**

- make `TriangleBootstrapDraw` consume executable texture binding metadata
- remove fragment shader reads from draw helper

**Step 3: Run focused validation**

Run:
- `cmake --build build-cuda-bootstrap --target backend-unittests vk-unittests draw-unittests --parallel 1`
- `./build-cuda-bootstrap/backend-unittests --gtest_filter=GraphicsExecutable.*`
- `./build-cuda-bootstrap/vk-unittests --gtest_filter='GraphicsBackendPipeline.*:BackendSmoke.*'`
- `./build-cuda-bootstrap/draw-unittests --gtest_filter=DrawTest.SolidColorTriangle`

Expected: PASS

### Task 3: Update tracking docs

**Files:**
- Modify: `task_plan.md`
- Modify: `progress.md`
- Modify: `findings.md`
- Modify: `docs/BackendBringup.md`

**Step 1: Record the slice**

- `GraphicsExecutable` now owns texture bootstrap binding metadata too
- `TriangleBootstrapDraw` no longer directly reads fragment `SpirvShader`

**Step 2: Re-run broader validation**

Run:
- `./build-cuda-bootstrap/backend-unittests`
- `./build-cuda-bootstrap/vk-unittests --gtest_filter='GraphicsBackendPipeline.*:BackendSmoke.*:GraphicsBackendSelection.*'`

Expected: PASS
