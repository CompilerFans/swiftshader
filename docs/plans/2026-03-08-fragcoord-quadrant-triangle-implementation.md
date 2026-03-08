# FragCoord Quadrant Triangle Draw Test Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add a `DrawTest` case that renders a normal triangle, colors covered fragments by `gl_FragCoord` quadrants, and saves a BMP artifact.

**Architecture:** Keep the change inside `tests/VulkanUnitTests/DrawTests.cpp`, reusing `DrawTester`, `readbackPixel()`, and `saveFrame()`. The fragment shader remains screen-space based; only the geometry and assertions differ from the existing fullscreen quadrant test.

**Tech Stack:** GoogleTest, `DrawTester`, GLSL shaders, Vulkan draw runtime, BMP artifact export.

---

### Task 1: Add the failing triangle test

**Files:**
- Modify: `tests/VulkanUnitTests/DrawTests.cpp`

**Step 1: Write the failing test**
- Add `DrawTest.FragmentShaderUsesFragCoordQuadrantColorsInsideTriangle`
- Use a normal triangle centered on screen
- Keep fragment colors based on `gl_FragCoord`
- Save `draw-test-artifacts/fragcoord-quadrant-triangle.bmp`
- Assert multiple interior points plus one background point

**Step 2: Run test to verify behavior**

Run: `cd build-cuda-bootstrap && ./draw-unittests --gtest_filter=DrawTest.FragmentShaderUsesFragCoordQuadrantColorsInsideTriangle`

Expected:
- If the current draw path already supports it, the test passes directly
- Otherwise it fails with a color mismatch that identifies the gap

### Task 2: Fix only if the test exposes a rendering gap

**Files:**
- Modify only if needed: existing draw helper or shader setup code

**Step 1: Implement the minimal fix**
- Make the smallest change needed for the targeted test to pass

**Step 2: Re-run the targeted test**

Run: `cd build-cuda-bootstrap && ./draw-unittests --gtest_filter=DrawTest.FragmentShaderUsesFragCoordQuadrantColorsInsideTriangle`

Expected: PASS

### Task 3: Run adjacent regression coverage

**Files:**
- No new files

**Step 1: Run adjacent draw tests**

Run: `cd build-cuda-bootstrap && ./draw-unittests --gtest_filter='DrawTest.FragmentShaderUsesFragCoordQuadrantColorsInsideTriangle:DrawTest.FragmentShaderUsesFragCoordQuadrantColors:DrawTest.SolidColorTriangle'`

Expected: PASS

### Task 4: Record, commit, and push

**Files:**
- Modify: `progress.md`
- Modify: `task_plan.md`
- Modify: `findings.md` if a new technical finding appears

**Step 1: Record the result**
- Update the tracking files with the new ordinary-triangle fragment-position milestone

**Step 2: Commit**

```bash
git add docs/plans/2026-03-08-fragcoord-quadrant-triangle-design.md \
        docs/plans/2026-03-08-fragcoord-quadrant-triangle-implementation.md \
        tests/VulkanUnitTests/DrawTests.cpp progress.md task_plan.md findings.md
git commit -m "tests: add fragcoord quadrant triangle draw test"
```

**Step 3: Push**

```bash
git push origin HEAD
```
