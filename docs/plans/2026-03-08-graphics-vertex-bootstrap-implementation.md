# Graphics Vertex Bootstrap Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Make the first graphics-submit CUDA bootstrap source look like a real vertex stage contract while keeping actual rendering on the CPU fallback path.

**Architecture:** Add a small backend helper that owns both the bootstrap CUDA source text and the dummy runtime launch. Use focused backend unit tests for source content and launch ABI, then wire `CustomExecutionBackend` through that helper.

**Tech Stack:** C++17, CUDA Driver API bootstrap path, GoogleTest.

---

### Task 1: Add failing bootstrap-source tests

**Files:**
- Create: `tests/BackendUnitTests/GraphicsBootstrapTests.cpp`
- Modify: `tests/BackendUnitTests/CMakeLists.txt`

**Step 1: Write the failing tests**
- Add one test that expects the graphics bootstrap source to contain:
  - `struct VertexInput`
  - `struct VertexOutput`
  - `outVertices[vertexIndex].w = 1.0f;`
- Add one test that expects the bootstrap launch to use three kernel arguments.

**Step 2: Run test to verify it fails**

Run: `cd build-cuda-bootstrap && cmake --build . --target backend-unittests -j$(nproc) && ./backend-unittests --gtest_filter=GraphicsBootstrap.*`  
Expected: FAIL because the helper does not exist yet.

### Task 2: Implement minimal graphics bootstrap helper

**Files:**
- Create: `src/Backend/GraphicsBootstrap.hpp`
- Create: `src/Backend/GraphicsBootstrap.cpp`
- Modify: `src/Backend/CMakeLists.txt`
- Modify: `src/Backend/BUILD.gn`
- Modify: `src/Backend/CustomExecutionBackend.cpp`

**Step 1: Write minimal implementation**
- Add `graphicsBootstrapCudaSource()`.
- Add `launchGraphicsBootstrap(RuntimeAPI &)`.
- Launch with three arguments: input pointer, output pointer, and vertex count.
- Use `vertexCount = 0` with valid dummy allocations so launch succeeds without needing real vertex data.

**Step 2: Run test to verify it passes**

Run: `cd build-cuda-bootstrap && ./backend-unittests --gtest_filter=GraphicsBootstrap.*`  
Expected: PASS.

### Task 3: Clean draw-path dump output

**Files:**
- Modify: `tests/VulkanUnitTests/DrawTests.cpp`

**Step 1: Minimal test-side cleanup**
- Disable runtime warmup in the multi-triangle test too, so draw-path dumps reflect the graphics bootstrap kernel cleanly.

**Step 2: Run focused draw verification**

Run: `cd build-cuda-bootstrap && ./draw-unittests --gtest_filter=DrawTest.SolidColorTriangle:DrawTest.MultipleSolidColorTriangles`  
Expected: PASS and the simple triangle dump should show the vertex-style bootstrap kernel.

### Task 4: Regressions and commit

**Files:**
- Modify: `progress.md`
- Modify: `task_plan.md`

**Step 1: Run focused regressions**

Run: `cd build-cuda-bootstrap && ./backend-unittests --gtest_filter=GraphicsBootstrap.*:RuntimeAPI.CudaRuntimePrintsKernelSourceByDefault:RuntimeAPI.CudaRuntimeSuppressesKernelSourceWhenDisabled:RuntimeAPI.CudaRuntimeCompilesLaunchesAndReadsBackDeviceMemory`  
Expected: PASS.

**Step 2: Commit**

```bash
git add docs/plans/2026-03-08-graphics-vertex-bootstrap-design.md \
        docs/plans/2026-03-08-graphics-vertex-bootstrap-implementation.md \
        src/Backend/GraphicsBootstrap.hpp \
        src/Backend/GraphicsBootstrap.cpp \
        src/Backend/CMakeLists.txt \
        src/Backend/BUILD.gn \
        src/Backend/CustomExecutionBackend.cpp \
        tests/BackendUnitTests/GraphicsBootstrapTests.cpp \
        tests/BackendUnitTests/CMakeLists.txt \
        tests/VulkanUnitTests/DrawTests.cpp \
        progress.md task_plan.md
git commit -m "Backend: add vertex-style graphics bootstrap"
```
