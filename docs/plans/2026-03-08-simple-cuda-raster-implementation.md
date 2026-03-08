# Simple CUDA Raster Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 为当前 CUDA backend 增加最小 raster bootstrap，先用 CPU 参考测试锁定行为，再用真实 CUDA runtime 验证 raster 输出，最后接到现有 fragment bootstrap。 / Add a minimal raster bootstrap for the current CUDA backend: lock behavior with CPU reference tests first, validate raster output on the real CUDA runtime second, and then connect it to the existing fragment bootstrap.

**Architecture:** 新增 `RasterBootstrap`，输入为三个 post-VS 顶点，输出为 `FragmentBootstrapInvocation` 列表。测试分为 CPU 参考测试和 GPU 执行测试两层；早期接口允许 `stub` / `dummy` 字段，但要求每步均遵循 TDD。 / Add `RasterBootstrap` that takes three post-VS vertices and produces a `FragmentBootstrapInvocation` list. Tests are split into CPU reference tests and GPU execution tests; early interfaces may contain `stub` / `dummy` fields, but each step must follow TDD.

**Tech Stack:** C++17, GoogleTest, generated CUDA-like source, `CudaRuntimeAPI`, `FakeRuntimeAPI`, backend bootstrap helpers. / C++17, GoogleTest, generated CUDA-like source, `CudaRuntimeAPI`, `FakeRuntimeAPI`, and backend bootstrap helpers.

---

### Task 1: Add raster bootstrap interfaces and failing tests

**Files:**
- Create: `src/Backend/RasterBootstrap.hpp`
- Create: `tests/BackendUnitTests/RasterBootstrapTests.cpp`
- Modify: `tests/BackendUnitTests/CMakeLists.txt`

**Step 1: Write the failing tests**

- Add tests that require:
  - emitted CUDA source to contain `RasterVertex`, `RasterParams`, and `raster_entry`
  - a CPU reference helper to return a non-empty bbox and invocation list for a simple triangle
  - `launchRasterBootstrap()` to pass a single params struct to `FakeRuntimeAPI`

**Step 2: Run tests to verify they fail**

Run: `cmake --build build-cuda-bootstrap --target backend-unittests --parallel $(nproc) && (cd build-cuda-bootstrap && ./backend-unittests --gtest_filter=RasterBootstrap.*)`

Expected: FAIL because `RasterBootstrap` does not exist yet.

**Step 3: Write the minimal interfaces**

- Add `RasterBootstrapVertex`
- Add `RasterBootstrapConfig`
- Add `RasterBootstrapOutput`
- Add declarations for:
  - `rasterBootstrapCudaSource()`
  - `rasterBootstrapCpuReference()`
  - `runRasterBootstrap()`
  - `launchRasterBootstrap()`

**Step 4: Re-run tests**

Run the same `RasterBootstrap.*` command.

Expected: still FAIL, but now on missing implementation.

**Step 5: Commit**

Commit message: `Backend: add raster bootstrap interfaces`

### Task 2: Implement CPU reference bbox + coverage

**Files:**
- Modify: `src/Backend/RasterBootstrap.hpp`
- Create: `src/Backend/RasterBootstrap.cpp`
- Modify: `src/Backend/BUILD.gn`
- Modify: `src/Backend/CMakeLists.txt`
- Test: `tests/BackendUnitTests/RasterBootstrapTests.cpp`

**Step 1: Keep the CPU reference test failing**

- Extend the test to require:
  - bbox min/max coordinates
  - inside-triangle pixels exist
  - obvious outside pixels do not appear

**Step 2: Run test to verify RED**

Run: `cd build-cuda-bootstrap && ./backend-unittests --gtest_filter=RasterBootstrap.CpuReference*`

Expected: FAIL on incorrect or missing bbox/coverage output.

**Step 3: Implement minimal CPU reference**

- Use framebuffer-space vertices
- Compute integer bbox
- Run simple edge-function coverage at pixel centers
- Emit `FragmentBootstrapInvocation` records with `exportMask = 1` and `helperInvocation = 0`

**Step 4: Re-run tests to verify GREEN**

Run the same `CpuReference` filter.

Expected: PASS.

**Step 5: Commit**

Commit message: `Backend: add raster CPU reference`

### Task 3: Implement raster CUDA source and fake launch plumbing

**Files:**
- Modify: `src/Backend/RasterBootstrap.cpp`
- Test: `tests/BackendUnitTests/RasterBootstrapTests.cpp`

**Step 1: Add failing source/launch tests**

- Require emitted CUDA source to contain:
  - `struct RasterVertex`
  - `struct RasterInvocation`
  - `extern "C" __global__ void raster_entry`
- Require `FakeRuntimeAPI` launch metadata to show a single params argument

**Step 2: Run tests to verify RED**

Run: `cd build-cuda-bootstrap && ./backend-unittests --gtest_filter=RasterBootstrap.Emits*:RasterBootstrap.Launch*`

Expected: FAIL.

**Step 3: Implement minimal CUDA source and fake-launch path**

- Emit a simple bbox/edge-function raster kernel
- Add a launch helper that creates a module and submits one params argument through `RuntimeAPI`

**Step 4: Re-run tests**

Expected: PASS.

**Step 5: Commit**

Commit message: `Backend: add raster CUDA bootstrap source`

### Task 4: Validate real CUDA raster output against CPU reference

**Files:**
- Modify: `tests/BackendUnitTests/RasterBootstrapTests.cpp`
- Modify: `src/Backend/RasterBootstrap.cpp`

**Step 1: Add failing CUDA-vs-CPU alignment test**

- For one simple triangle:
  - run `rasterBootstrapCpuReference()`
  - run `runRasterBootstrap()` on `CudaRuntimeAPI`
  - compare invocation count and a stable subset of coordinates

**Step 2: Run test to verify RED**

Run: `cd build-cuda-bootstrap && ./backend-unittests --gtest_filter=RasterBootstrap.CudaRuntimeMatchesCpuReference`

Expected: FAIL.

**Step 3: Implement the minimal real-runtime path**

- Allocate vertex and output buffers
- Launch the raster kernel
- Read back invocation data and count

**Step 4: Re-run tests**

Expected: PASS.

**Step 5: Commit**

Commit message: `Backend: validate raster bootstrap against CPU reference`

### Task 5: Connect raster bootstrap to fragment bootstrap

**Files:**
- Modify: `src/Backend/RasterBootstrap.hpp`
- Modify: `src/Backend/RasterBootstrap.cpp`
- Modify: `tests/BackendUnitTests/RasterBootstrapTests.cpp`
- Modify: `tests/BackendUnitTests/FragmentBootstrapTests.cpp`

**Step 1: Add failing integration test**

- Require a simple triangle to:
  - rasterize into invocation output
  - feed that output into `runFragmentBootstrap()`
  - paint at least one expected pixel in the color buffer

**Step 2: Run test to verify RED**

Run: `cd build-cuda-bootstrap && ./backend-unittests --gtest_filter=RasterBootstrap.RasterFeedsFragmentBootstrap`

Expected: FAIL.

**Step 3: Implement the minimal integration**

- Add a helper that converts raster output into the existing fragment bootstrap input format
- Keep interfaces narrow; leave future barycentric/quad fields as explicit stubs

**Step 4: Re-run tests**

Expected: PASS.

**Step 5: Commit**

Commit message: `Backend: connect raster bootstrap to fragment bootstrap`
