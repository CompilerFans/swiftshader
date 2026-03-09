# Texture Bootstrap Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add a narrow, real texture sampling path to the CUDA bootstrap graphics backend and validate it with Vulkan draw tests that dump images.

**Architecture:** Reuse the existing bootstrap chain (`GraphicsBootstrap -> RasterBootstrap -> FragmentBootstrap`) and extend it with a minimal `uv` varying plus a narrow `Texture2DColor` fragment mode. Reuse existing Vulkan descriptor/image setup from test infrastructure instead of inventing a fake texture system.

**Tech Stack:** SwiftShader Vulkan frontend, existing bootstrap backend, GoogleTest, DrawTester, Vulkan descriptors/images/samplers.

---

### Task 1: Add failing bootstrap texture tests

**Files:**
- Modify: `tests/BackendUnitTests/FragmentBootstrapTests.cpp`
- Modify: `tests/VulkanUnitTests/DrawTests.cpp`

**Step 1: Write the failing test**
- Add one backend test for a narrow textured fragment mode.
- Add one Vulkan draw test for a textured triangle with BMP dump.

**Step 2: Run test to verify it fails**
- Run: `./build-cuda-bootstrap/backend-unittests --gtest_filter='FragmentBootstrap.*Texture*'`
- Run: `./build-cuda-bootstrap/draw-unittests --gtest_filter='DrawTest.TexturedTriangleNearest'`
- Expected: fail because no texture bootstrap mode exists.

**Step 3: Write minimal implementation**
- No implementation in this task.

**Step 4: Re-run to confirm failure is the right failure**
- Expected: missing bootstrap texture path, not unrelated build errors.

**Step 5: Commit**
- `git commit -m "tests: add failing texture bootstrap coverage"`

### Task 2: Add `uv` varying to bootstrap VS path

**Files:**
- Modify: `src/Backend/GraphicsBootstrap.hpp`
- Modify: `src/Backend/GraphicsBootstrap.cpp`
- Modify: `tests/BackendUnitTests/GraphicsBootstrapTests.cpp`

**Step 1: Write the failing test**
- Add tests for emitted `u/v` output and runtime propagation.

**Step 2: Run test to verify it fails**
- Run: `./build-cuda-bootstrap/backend-unittests --gtest_filter='GraphicsBootstrap.*TexCoord*'`

**Step 3: Write minimal implementation**
- Extend `GraphicsBootstrapVertexInput/Output` and generated CUDA source with `u/v`.

**Step 4: Run test to verify it passes**
- Re-run the focused backend tests.

**Step 5: Commit**
- `git commit -m "Backend: add texcoord bootstrap varying"`

### Task 3: Add narrow texture sampler mode to fragment bootstrap

**Files:**
- Modify: `src/Backend/FragmentBootstrap.hpp`
- Modify: `src/Backend/FragmentBootstrap.cpp`
- Modify: `tests/BackendUnitTests/FragmentBootstrapTests.cpp`

**Step 1: Write the failing test**
- Add a backend test using a tiny checkerboard texture.

**Step 2: Run test to verify it fails**
- Run: `./build-cuda-bootstrap/backend-unittests --gtest_filter='FragmentBootstrap.*Texture2D*'`

**Step 3: Write minimal implementation**
- Add `Texture2DColor` mode.
- Add narrow texture params: width, height, texel pointer, filter, address mode.
- Upload texture bytes to device memory before launch.

**Step 4: Run test to verify it passes**
- Re-run the focused backend tests.

**Step 5: Commit**
- `git commit -m "Backend: add texture fragment bootstrap mode"`

### Task 4: Thread `uv` through triangle bootstrap interpolation

**Files:**
- Modify: `src/Backend/TrianglePipelineBootstrap.hpp`
- Modify: `src/Backend/TrianglePipelineBootstrap.cpp`
- Modify: `tests/BackendUnitTests/TrianglePipelineBootstrapTests.cpp`

**Step 1: Write the failing test**
- Add a triangle-bootstrap test that checks interpolated `uv` reaches fragment texture mode.

**Step 2: Run test to verify it fails**
- Run: `./build-cuda-bootstrap/backend-unittests --gtest_filter='TrianglePipelineBootstrap.*Texture*'`

**Step 3: Write minimal implementation**
- Carry per-vertex `uv` alongside barycentrics.
- Populate fragment texture config with three vertex `uv` values.

**Step 4: Run test to verify it passes**
- Re-run focused backend tests.

**Step 5: Commit**
- `git commit -m "Backend: thread texture coordinates through bootstrap"`

### Task 5: Detect narrow texture shader family in renderer

**Files:**
- Modify: `src/Device/Renderer.cpp`
- Check: `src/Vulkan/VkDescriptorSet.hpp`
- Check: `tests/VulkanBenchmarks/TriangleBenchmarks.cpp`

**Step 1: Write the failing test**
- Use the Vulkan textured triangle draw test added in Task 1.

**Step 2: Run test to verify it fails**
- Run: `./build-cuda-bootstrap/draw-unittests --gtest_filter='DrawTest.TexturedTriangleNearest'`

**Step 3: Write minimal implementation**
- Detect the narrow `texture(tex, uv)` shader family.
- Extract bound texture bytes and minimal sampler state from the descriptor set.
- Route the draw through the bootstrap texture path.

**Step 4: Run test to verify it passes**
- Re-run the focused draw test.

**Step 5: Commit**
- `git commit -m "Device: route narrow texture shader to bootstrap"`

### Task 6: Add indexed texture draw coverage and artifact verification

**Files:**
- Modify: `tests/VulkanUnitTests/DrawTests.cpp`
- Modify: `progress.md`
- Modify: `task_plan.md`
- Modify: `findings.md`

**Step 1: Write the failing test**
- Add `DrawTest.IndexedTexturedTriangleNearest` with BMP dump.

**Step 2: Run test to verify it fails**
- Run: `./build-cuda-bootstrap/draw-unittests --gtest_filter='DrawTest.IndexedTexturedTriangleNearest'`

**Step 3: Write minimal implementation**
- Reuse the same narrow texture bootstrap path for indexed draws.

**Step 4: Run test to verify it passes**
- Re-run both textured draw tests.

**Step 5: Commit**
- `git commit -m "tests: add indexed texture bootstrap coverage"`

### Task 7: Validate and push

**Files:**
- Modify: `progress.md`
- Modify: `task_plan.md`
- Modify: `findings.md`

**Step 1: Run focused validation**
- `cmake --build build-cuda-bootstrap --target backend-unittests draw-unittests --parallel $(nproc)`
- `./build-cuda-bootstrap/backend-unittests --gtest_filter='GraphicsBootstrap.*TexCoord*:FragmentBootstrap.*Texture2D*:TrianglePipelineBootstrap.*Texture*'`
- `./build-cuda-bootstrap/draw-unittests --gtest_filter='DrawTest.TexturedTriangleNearest:DrawTest.IndexedTexturedTriangleNearest'`

**Step 2: Confirm artifacts exist**
- Check `build-cuda-bootstrap/draw-test-artifacts/` for the new BMPs.

**Step 3: Record findings**
- Update `progress.md`, `task_plan.md`, `findings.md`.

**Step 4: Commit**
- `git commit -m "docs: record texture bootstrap milestone"`

**Step 5: Push**
- `git push origin HEAD:master`
