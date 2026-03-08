# Draw Test Image Artifacts Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Save draw-test triangle results as viewable image artifacts without changing production rendering behavior.

**Architecture:** Extend `DrawTester` with a test-only frame-dump API that reuses existing readback flow and writes a simple `BMP` file into the build directory. Keep the current draw assertions unchanged and add artifact existence checks in the affected tests.

**Tech Stack:** C++17, Vulkan-Hpp test wrapper, GoogleTest, standard file I/O.

---

### Task 1: Add failing artifact assertions

**Files:**
- Modify: `tests/VulkanUnitTests/DrawTests.cpp`

**Step 1: Write the failing test**
- Add stable output paths for the single- and multi-triangle tests.
- Call a new `DrawTester::saveFrame()` API after `renderFrame()`.
- Assert that the expected `BMP` file exists.

**Step 2: Run test to verify it fails**

Run: `cd build-cuda-bootstrap && cmake --build . --target draw-unittests -j$(nproc)`  
Expected: FAIL because `DrawTester::saveFrame()` does not exist yet.

### Task 2: Add minimal BMP export support

**Files:**
- Modify: `tests/VulkanWrapper/DrawTester.hpp`
- Modify: `tests/VulkanWrapper/DrawTester.cpp`

**Step 1: Write minimal implementation**
- Add `saveFrame(const std::filesystem::path &path)`.
- Reuse the existing image-to-buffer readback path.
- Write a 32-bit `BMP` file with a minimal file/info header and BGRA pixel payload.
- Create parent directories as needed.

**Step 2: Run test to verify it passes**

Run: `cd build-cuda-bootstrap && ./draw-unittests --gtest_filter=DrawTest.SolidColorTriangle:DrawTest.MultipleSolidColorTriangles`  
Expected: PASS and both files appear under `draw-test-artifacts/`.

### Task 3: Re-run fast regression and commit

**Files:**
- Modify: `progress.md`
- Modify: `task_plan.md`

**Step 1: Re-run regression**

Run: `cd build-draw-custom-subzero && ./draw-unittests --gtest_filter=DrawTest.SolidColorTriangle:DrawTest.MultipleSolidColorTriangles`  
Expected: PASS.

**Step 2: Commit**

```bash
git add docs/plans/2026-03-08-draw-test-image-artifacts-design.md \
        docs/plans/2026-03-08-draw-test-image-artifacts-implementation.md \
        tests/VulkanUnitTests/DrawTests.cpp \
        tests/VulkanWrapper/DrawTester.hpp \
        tests/VulkanWrapper/DrawTester.cpp \
        progress.md task_plan.md
git commit -m "tests: save draw image artifacts"
```
