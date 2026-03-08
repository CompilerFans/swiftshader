# Multi Solid Triangle Validation Implementation Plan

> **For Claude:** Execute with TDD and keep the default single-draw path unchanged.

**Goal:** Add a reusable multi-draw hook to `DrawTester` and verify multiple solid-color triangles render correctly.

**Architecture:** Extend test-side command recording only. Keep runtime, pipeline creation, and production renderer behavior unchanged.

**Tech Stack:** C++17, Vulkan-Hpp test wrapper, GoogleTest.

---

### Task 1: Add failing multi-triangle draw test
- Modify: `tests/VulkanUnitTests/DrawTests.cpp`
- Step 1: Add a test that records three independent draws and checks three red pixels.
- Step 2: Run the focused `draw-unittests` case and verify it fails because `DrawTester` has no multi-draw hook.

### Task 2: Add minimal draw-record hook
- Modify: `tests/VulkanWrapper/DrawTester.hpp`
- Modify: `tests/VulkanWrapper/DrawTester.cpp`
- Step 1: Add one optional callback that receives the command buffer and records draw commands.
- Step 2: Preserve the current single-draw default path if the callback is unset.
- Step 3: Re-run the focused test and make it pass.

### Task 3: Re-run regression tests and commit
- Run: `draw-unittests --gtest_filter=DrawTest.SolidColorTriangle:DrawTest.MultipleSolidColorTriangles`
- Run: `draw-unittests` in the existing custom fast build directory as a regression check.
- Commit the change once both tests pass.
