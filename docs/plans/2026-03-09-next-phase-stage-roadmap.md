# Next-Phase Stage Roadmap Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Expand the current CUDA bootstrap from demo-grade triangle rendering into a staged subset of CPU SwiftShader feature parity for `VS / Raster / FS`.

**Architecture:** Keep the existing `GraphicsBootstrap -> RasterBootstrap -> FragmentBootstrap -> TrianglePipelineBootstrap` layering, but widen it in the same order the CPU path composes the stages: draw-state coverage first, interpolation model second, fragment correctness third, then fixed-function integration.

**Tech Stack:** SwiftShader Vulkan frontend, CPU reference path in `src/Device` + `src/Pipeline`, CUDA runtime/compiler path in `src/Backend`, GoogleTest draw/backend tests.

---

### Task 1: Freeze the next-phase feature matrix

**Files:**
- Read: `docs/plans/2026-03-09-cpu-stage-feature-audit-design.md`
- Modify: `task_plan.md`
- Modify: `findings.md`

**Steps:**
1. Record the accepted next-phase order in `task_plan.md`.
2. Record any scope exclusions explicitly in `findings.md`.
3. Commit.

**Scope exclusions to record now:**
- No `TCS/TES/GS` in the next phase.
- No “single binary runtime CPU/CUDA switch”; backend choice remains build/script selected.
- No full descriptor-heavy shader support before depth/stencil/blend basics.

### Task 2: Expand draw-state coverage to indexed triangle-list

**Files:**
- Modify: `src/Backend/TrianglePipelineBootstrap.hpp`
- Modify: `src/Backend/TrianglePipelineBootstrap.cpp`
- Modify: `src/Device/Renderer.cpp`
- Test: `tests/BackendUnitTests/TrianglePipelineBootstrapTests.cpp`
- Test: `tests/VulkanUnitTests/DrawTests.cpp`

**Step pattern:**
1. Write failing backend tests for indexed triangle-list config extraction and raw indexed rendering.
2. Write a failing Vulkan draw test using an index buffer.
3. Implement minimal index extraction for `triangle list`.
4. Verify focused tests pass.
5. Commit.

### Task 3: Replace the current varying shortcut with first-class barycentrics

**Files:**
- Modify: `src/Backend/RasterBootstrap.hpp`
- Modify: `src/Backend/RasterBootstrap.cpp`
- Modify: `src/Backend/FragmentBootstrap.hpp`
- Modify: `src/Backend/FragmentBootstrap.cpp`
- Test: `tests/BackendUnitTests/RasterBootstrapTests.cpp`
- Test: `tests/BackendUnitTests/FragmentBootstrapTests.cpp`
- Test: `tests/VulkanUnitTests/DrawTests.cpp`

**Step pattern:**
1. Write failing backend tests that require barycentric payload generation.
2. Write a failing draw test that distinguishes perspective interpolation from a pre-baked color shortcut.
3. Implement barycentric storage in raster invocation payload.
4. Switch `InterpolatedColor` to consume barycentrics plus per-vertex values.
5. Verify focused tests pass.
6. Commit.

### Task 4: Add flat / perspective / noperspective interpolation modes

**Files:**
- Modify: `src/Backend/FragmentBootstrap.hpp`
- Modify: `src/Backend/FragmentBootstrap.cpp`
- Modify: `src/Backend/TrianglePipelineBootstrap.cpp`
- Test: `tests/VulkanUnitTests/DrawTests.cpp`

**Step pattern:**
1. Add a failing draw test for flat interpolation.
2. Add a failing draw test for `noperspective`.
3. Implement minimal interpolation-mode metadata plumbing.
4. Verify focused tests pass.
5. Commit.

### Task 5: Add `FrontFacing` and `PointCoord` fragment builtins

**Files:**
- Modify: `src/Backend/RasterBootstrap.hpp`
- Modify: `src/Backend/RasterBootstrap.cpp`
- Modify: `src/Backend/FragmentBootstrap.cpp`
- Modify: `src/Device/Renderer.cpp`
- Test: `tests/VulkanUnitTests/DrawTests.cpp`

**Step pattern:**
1. Add a failing front-facing draw test.
2. Add a failing point-coord draw test.
3. Implement payload generation in raster/bootstrap.
4. Verify focused tests pass.
5. Commit.

### Task 6: Add discard / demote / helper-lane correctness

**Files:**
- Modify: `src/Backend/FragmentBootstrap.hpp`
- Modify: `src/Backend/FragmentBootstrap.cpp`
- Modify: `src/Backend/RasterBootstrap.cpp`
- Test: `tests/BackendUnitTests/FragmentBootstrapTests.cpp`
- Test: `tests/VulkanUnitTests/DrawTests.cpp`

**Step pattern:**
1. Write failing backend tests for helper-lane/export interaction.
2. Write a failing draw test for fragment discard.
3. Implement minimal discard/demote handling in FS bootstrap.
4. Verify focused tests pass.
5. Commit.

### Task 7: Add `FragDepth` and depth-test/write integration

**Files:**
- Modify: `src/Backend/FragmentBootstrap.hpp`
- Modify: `src/Backend/FragmentBootstrap.cpp`
- Modify: `src/Backend/TrianglePipelineBootstrap.cpp`
- Modify: `src/Device/Renderer.cpp`
- Test: `tests/VulkanUnitTests/DrawTests.cpp`

**Step pattern:**
1. Add a failing draw test that requires fragment depth output to affect visibility.
2. Implement a narrow depth buffer path in the bootstrap pipeline.
3. Verify focused tests pass.
4. Commit.

### Task 8: Add stencil and blend integration

**Files:**
- Modify: `src/Backend/TrianglePipelineBootstrap.cpp`
- Modify: `src/Backend/FragmentBootstrap.cpp`
- Test: `tests/VulkanUnitTests/DrawTests.cpp`

**Step pattern:**
1. Add a failing stencil gate test.
2. Add a failing blend test.
3. Implement minimal fixed-function integration.
4. Verify focused tests pass.
5. Commit.

### Task 9: Expand topology and line/point coverage

**Files:**
- Modify: `src/Backend/TrianglePipelineBootstrap.cpp`
- Modify: `src/Backend/RasterBootstrap.cpp`
- Modify: `src/Device/Renderer.cpp`
- Test: `tests/VulkanUnitTests/DrawTests.cpp`

**Step pattern:**
1. Add failing tests for strip/fan.
2. Add failing tests for line/point rendering.
3. Add failing test for provoking vertex behavior.
4. Implement one topology family at a time.
5. Verify focused tests pass.
6. Commit after each small family.

### Task 10: Add sample-related fragment semantics

**Files:**
- Modify: `src/Backend/RasterBootstrap.hpp`
- Modify: `src/Backend/RasterBootstrap.cpp`
- Modify: `src/Backend/FragmentBootstrap.cpp`
- Test: `tests/VulkanUnitTests/DrawTests.cpp`

**Step pattern:**
1. Add failing tests for `SampleId`, `SamplePosition`, and sample-mask input/output.
2. Implement sample payload fields.
3. Verify focused tests pass.
4. Commit.

### Task 11: Add push constants and descriptor-backed shader inputs

**Files:**
- Modify: `src/Backend/GraphicsBootstrap.*`
- Modify: `src/Backend/FragmentBootstrap.*`
- Modify: `src/Backend/TrianglePipelineBootstrap.*`
- Modify: `src/Device/Renderer.cpp`
- Test: `tests/VulkanUnitTests/DrawTests.cpp`

**Step pattern:**
1. Add a failing push-constant-driven draw test.
2. Add a failing descriptor-backed color/transform test.
3. Implement one resource family at a time.
4. Verify focused tests pass.
5. Commit.

### Task 12: Fold benchmark results into the gate

**Files:**
- Modify: `tests/VulkanBenchmarks/AnimatedTriangleBenchmark.cpp`
- Modify: `tests/VulkanBenchmarks/run-animated-triangle-benchmark.sh`
- Modify: `progress.md`
- Modify: `findings.md`

**Step pattern:**
1. Re-run the visible benchmark on both CPU and CUDA after each major milestone.
2. Record FPS deltas in `progress.md`.
3. If CUDA remains materially slower on the animated triangle after depth/blend/basic descriptors are in place, pause feature expansion and profile.

## Recommended Execution Order

1. Indexed triangle-list
2. Barycentrics
3. Interpolation modes
4. `FrontFacing` / `PointCoord`
5. discard / demote
6. `FragDepth`
7. stencil / blend
8. topology breadth
9. sample semantics
10. push constants / descriptors
11. performance gate review

## Success Criteria for the Next Phase

- The CUDA path covers more than one “happy-path” triangle demo.
- The stage ABI is stable enough to carry real raster/fragment payloads.
- CPU feature parity is still partial, but the missing parts are explicit and ordered.
- The visible animated benchmark remains runnable on both CPU and CUDA and is used as an ongoing performance checkpoint.

