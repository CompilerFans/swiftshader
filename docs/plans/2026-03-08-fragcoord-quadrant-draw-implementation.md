# FragCoord Quadrant Draw Test Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add a `DrawTest` case that validates `gl_FragCoord`-driven fragment colors and saves a BMP artifact.

**Architecture:** Keep the implementation inside `tests/VulkanUnitTests/DrawTests.cpp` so it reuses the existing draw harness, shader compilation helpers, image readback, and BMP export. No production rendering code changes are expected unless the test exposes a real gap.

**Tech Stack:** GoogleTest, `DrawTester`, GLSL-to-SPIR-V helper, Vulkan runtime draw path.

---

### Task 1: Add the failing draw test

**Files:**
- Modify: `tests/VulkanUnitTests/DrawTests.cpp`

**Step 1: Write the failing test**
- Add `DrawTest.FragmentShaderUsesFragCoordQuadrantColors`
- Use a fullscreen triangle
- Fragment shader colors the four quadrants differently from `gl_FragCoord`
- Save `draw-test-artifacts/fragcoord-quadrant-colors.bmp`
- Assert four representative pixels

**Step 2: Run test to verify behavior**

Run: `cd build-cuda-bootstrap && ./draw-unittests --gtest_filter=DrawTest.FragmentShaderUsesFragCoordQuadrantColors`

Expected:
- If the feature already works, the test passes and no production code is needed
- If not, the failure identifies the rendering gap to fix

### Task 2: Fix only if the test exposes a gap

**Files:**
- Modify only if necessary: existing draw helper or shader setup code

**Step 1: Implement the minimal fix**
- Make the smallest change needed for the new test to pass

**Step 2: Re-run the targeted test**

Run: `cd build-cuda-bootstrap && ./draw-unittests --gtest_filter=DrawTest.FragmentShaderUsesFragCoordQuadrantColors`

Expected: PASS

### Task 3: Run adjacent regression coverage

**Files:**
- No new files

**Step 1: Re-run adjacent draw tests**

Run: `cd build-cuda-bootstrap && ./draw-unittests --gtest_filter='DrawTest.FragmentShaderUsesFragCoordQuadrantColors:DrawTest.SolidColorTriangle:DrawTest.MultipleSolidColorTriangles'`

Expected: PASS

### Task 4: Record, commit, and push

**Files:**
- Modify: `progress.md`
- Modify: `task_plan.md`
- Modify: `findings.md` if a new technical finding appears

**Step 1: Record the result**
- Update the tracking files with the new fragment-position test milestone

**Step 2: Commit**

```bash
git add docs/plans/2026-03-08-fragcoord-quadrant-draw-design.md \
        docs/plans/2026-03-08-fragcoord-quadrant-draw-implementation.md \
        tests/VulkanUnitTests/DrawTests.cpp progress.md task_plan.md findings.md
git commit -m "tests: add fragcoord quadrant draw test"
```

**Step 3: Push**

```bash
git push origin HEAD
```
