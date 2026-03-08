# CUDA Source Dump Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Print CUDA kernel source to `stderr` during module compilation, with a runtime switch and default-on behavior for the current bootstrap stage.

**Architecture:** Keep the feature entirely inside `CudaRuntimeAPI::createModule()` so it only affects the real CUDA path. Use one environment variable to control the dump and validate behavior with focused runtime unit tests that capture `stderr`.

**Tech Stack:** C++17, CUDA Driver API bootstrap path, GoogleTest.

---

### Task 1: Add failing default-on dump test

**Files:**
- Modify: `tests/BackendUnitTests/RuntimeAPITests.cpp`

**Step 1: Write the failing test**
- Capture `stderr`.
- Create a CUDA module through `CudaRuntimeAPI`.
- Assert the captured output contains the kernel source text and dump markers.

**Step 2: Run test to verify it fails**

Run: `cd build-cuda-bootstrap && cmake --build . --target backend-unittests -j$(nproc) && ./backend-unittests --gtest_filter=RuntimeAPI.CudaRuntimePrintsKernelSourceByDefault`  
Expected: FAIL because no dump is printed yet.

### Task 2: Implement minimal source dump

**Files:**
- Modify: `src/Backend/CudaRuntimeAPI.cpp`

**Step 1: Write minimal implementation**
- Add a small helper that reads `SWIFTSHADER_CUDA_DUMP_SOURCE`.
- Print the source to `stderr` with begin/end markers when dumping is enabled.
- Keep default behavior enabled when the environment variable is unset.

**Step 2: Run test to verify it passes**

Run: `cd build-cuda-bootstrap && ./backend-unittests --gtest_filter=RuntimeAPI.CudaRuntimePrintsKernelSourceByDefault`  
Expected: PASS.

### Task 3: Add failing disable-switch test

**Files:**
- Modify: `tests/BackendUnitTests/RuntimeAPITests.cpp`

**Step 1: Write the failing test**
- Set `SWIFTSHADER_CUDA_DUMP_SOURCE=0`.
- Capture `stderr` around module creation.
- Assert the dump markers are absent.

**Step 2: Run test to verify it fails**

Run: `cd build-cuda-bootstrap && ./backend-unittests --gtest_filter=RuntimeAPI.CudaRuntimeSuppressesKernelSourceWhenDisabled`  
Expected: FAIL because the dump still prints.

### Task 4: Implement disable switch and regressions

**Files:**
- Modify: `src/Backend/CudaRuntimeAPI.cpp`
- Modify: `progress.md`
- Modify: `task_plan.md`

**Step 1: Write minimal implementation**
- Treat `0`, `false`, `off`, and `no` as disabled values.

**Step 2: Run focused regression**

Run: `cd build-cuda-bootstrap && ./backend-unittests --gtest_filter=RuntimeAPI.CudaRuntimePrintsKernelSourceByDefault:RuntimeAPI.CudaRuntimeSuppressesKernelSourceWhenDisabled:RuntimeAPI.CudaRuntimeCompilesLaunchesAndReadsBackDeviceMemory`  
Expected: PASS.

**Step 3: Commit**

```bash
git add docs/plans/2026-03-08-cuda-source-dump-design.md \
        docs/plans/2026-03-08-cuda-source-dump-implementation.md \
        src/Backend/CudaRuntimeAPI.cpp \
        tests/BackendUnitTests/RuntimeAPITests.cpp \
        progress.md task_plan.md
git commit -m "Backend: dump CUDA kernel source by default"
```
