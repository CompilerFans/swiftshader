# Graphics Attachment Lifecycle Tracking Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add a minimal draw-time color attachment target contract so strict GPU triangle bootstrap write-back respects the active color attachment 0 state for both render pass and dynamic rendering.

**Architecture:** Extend `backend::GraphicsDrawCall` with an explicit color attachment target, populate it in `CmdDrawBase::draw()` from the active render pass or dynamic rendering state, and make `TriangleBootstrapDraw` consume that contract instead of probing attachment state ad hoc. Keep begin-phase clear behavior unchanged and use new tests to lock `storeOp` / layout gating and end-to-end strict GPU behavior.

**Tech Stack:** SwiftShader Vulkan runtime, backend draw bootstrap path, GoogleTest draw/backend unit tests, DrawTester harness.

---

### Task 1: Add RED coverage for `storeOp`-aware strict GPU write-back

**Files:**
- Modify: `tests/VulkanUnitTests/DrawTests.cpp`
- Modify: `tests/VulkanWrapper/DrawTester.hpp`
- Modify: `tests/VulkanWrapper/DrawTester.cpp`
- Test: `tests/VulkanUnitTests/DrawTests.cpp`

**Step 1: Write the failing tests**

Add these draw tests:

- `DrawTest.StrictGpuTriangleBootstrapRejectsRenderPassColorStoreDontCare`
- `DrawTest.StrictGpuTriangleBootstrapRejectsDynamicRenderingColorStoreDontCare`

Both should:
- enable strict GPU bootstrap (`SWIFTSHADER_GPU_RENDER_TRIANGLE_BOOTSTRAP=1`, `SWIFTSHADER_GPU_REQUIRE_TRIANGLE_BOOTSTRAP=1`)
- configure a simple solid-color triangle
- force the color attachment to use `storeOp = eDontCare`
- expect the strict GPU path to abort instead of silently reporting successful write-back

Also extend `DrawTester` with a minimal color-store-op configuration hook used only by these tests.

**Step 2: Run tests to verify they fail**

Run:

```bash
cmake --build build-cuda-bootstrap --target draw-unittests --parallel 1
./build-cuda-bootstrap/draw-unittests --gtest_filter='DrawTest.StrictGpuTriangleBootstrapRejectsRenderPassColorStoreDontCare:DrawTest.StrictGpuTriangleBootstrapRejectsDynamicRenderingColorStoreDontCare'
```

Expected:
- build or test RED because `DrawTester` does not yet expose `colorStoreOp`, or
- the tests fail because triangle bootstrap still ignores `storeOp`

**Step 3: Write minimal implementation**

Modify `DrawTester` so tests can set `colorStoreOp` for both render pass and dynamic rendering setup, but do not change backend behavior yet.

**Step 4: Run tests to verify they still fail for the right reason**

Run:

```bash
./build-cuda-bootstrap/draw-unittests --gtest_filter='DrawTest.StrictGpuTriangleBootstrapRejectsRenderPassColorStoreDontCare:DrawTest.StrictGpuTriangleBootstrapRejectsDynamicRenderingColorStoreDontCare'
```

Expected:
- compile succeeds
- tests still fail because backend draw has not been taught to respect `storeOp`

**Step 5: Commit**

```bash
git add tests/VulkanUnitTests/DrawTests.cpp tests/VulkanWrapper/DrawTester.hpp tests/VulkanWrapper/DrawTester.cpp
git commit -m "Tests: add storeOp coverage for strict GPU bootstrap"
```

### Task 2: Introduce the explicit draw-time color attachment target contract

**Files:**
- Modify: `src/Backend/GraphicsDraw.hpp`
- Modify: `src/Vulkan/VkCommandBuffer.hpp`
- Modify: `src/Vulkan/VkCommandBuffer.cpp`
- Test: `tests/VulkanUnitTests/DrawTests.cpp`

**Step 1: Write the failing test**

Reuse the RED tests from Task 1 as the contract-driving behavior. Add one more focused positive test if needed:

- `DrawTest.StrictGpuTriangleBootstrapWritesRenderPassColorStoreStore`

This should prove that `storeOp = eStore` remains valid after the contract is introduced.

**Step 2: Run test to verify it fails**

Run:

```bash
./build-cuda-bootstrap/draw-unittests --gtest_filter='DrawTest.StrictGpuTriangleBootstrapRejectsRenderPassColorStoreDontCare:DrawTest.StrictGpuTriangleBootstrapWritesRenderPassColorStoreStore'
```

Expected:
- `DontCare` case still fails because no draw-time attachment contract exists

**Step 3: Write minimal implementation**

Implement:

1. A minimal `backend::GraphicsColorAttachmentTarget` in `src/Backend/GraphicsDraw.hpp` with:
   - `vk::ImageView *imageView`
   - `VkImageLayout layout`
   - `VkAttachmentStoreOp storeOp`
   - `bool present`
2. A matching field in `backend::GraphicsDrawCall`
3. Extraction in `CmdDrawBase::draw()`:
   - render pass path:
     - current subpass color attachment 0 reference
     - framebuffer attachment image view
     - attachment description `storeOp`
     - attachment reference layout
   - dynamic rendering path:
     - `VkRenderingAttachmentInfo` imageView / storeOp / imageLayout

Keep this extraction helper narrow: only color attachment 0, no depth/stencil/resolve tracking.

**Step 4: Run tests to verify extraction compiles and positive path still works**

Run:

```bash
cmake --build build-cuda-bootstrap --target draw-unittests --parallel 1
./build-cuda-bootstrap/draw-unittests --gtest_filter='DrawTest.StrictGpuTriangleBootstrapWritesRenderPassColorStoreStore'
```

Expected:
- PASS

**Step 5: Commit**

```bash
git add src/Backend/GraphicsDraw.hpp src/Vulkan/VkCommandBuffer.hpp src/Vulkan/VkCommandBuffer.cpp tests/VulkanUnitTests/DrawTests.cpp
git commit -m "Vulkan: pass active color attachment target to backend draw"
```

### Task 3: Make `TriangleBootstrapDraw` respect `storeOp` and layout

**Files:**
- Modify: `src/Backend/TriangleBootstrapDraw.hpp`
- Modify: `src/Backend/TriangleBootstrapDraw.cpp`
- Modify: `tests/BackendUnitTests/TriangleBootstrapDrawTests.cpp`
- Modify: `tests/VulkanUnitTests/DrawTests.cpp`
- Test: `tests/BackendUnitTests/TriangleBootstrapDrawTests.cpp`
- Test: `tests/VulkanUnitTests/DrawTests.cpp`

**Step 1: Write the failing tests**

Add backend unit tests for the gate logic, for example:

- accepts `VK_ATTACHMENT_STORE_OP_STORE`
- rejects `VK_ATTACHMENT_STORE_OP_DONT_CARE`
- accepts `VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL`
- accepts `VK_IMAGE_LAYOUT_GENERAL`
- rejects `VK_IMAGE_LAYOUT_PRESENT_SRC_KHR` (or another non-write layout)

Keep these tests pure and local to `TriangleBootstrapDraw` policy/helper logic.

**Step 2: Run tests to verify they fail**

Run:

```bash
cmake --build build-cuda-bootstrap --target backend-unittests --parallel 1
./build-cuda-bootstrap/backend-unittests --gtest_filter='TriangleBootstrapDraw.*'
```

Expected:
- RED because the helper does not yet validate attachment `storeOp` / layout from the new draw contract

**Step 3: Write minimal implementation**

Implement a narrow validation helper inside `TriangleBootstrapDraw`:

- require `draw.colorAttachment0.present`
- require `draw.colorAttachment0.imageView != nullptr`
- require `draw.colorAttachment0.storeOp == VK_ATTACHMENT_STORE_OP_STORE`
- allow only:
  - `VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL`
  - `VK_IMAGE_LAYOUT_GENERAL`

Then update write-back to use `draw.colorAttachment0.imageView` instead of probing pipeline attachments directly.

Retain existing format / sample-count / row-pitch checks.

**Step 4: Run tests to verify they pass**

Run:

```bash
./build-cuda-bootstrap/backend-unittests --gtest_filter='TriangleBootstrapDraw.*'
./build-cuda-bootstrap/draw-unittests --gtest_filter='DrawTest.StrictGpuTriangleBootstrapRejectsRenderPassColorStoreDontCare:DrawTest.StrictGpuTriangleBootstrapRejectsDynamicRenderingColorStoreDontCare:DrawTest.StrictGpuTriangleBootstrapWritesRenderPassColorStoreStore'
```

Expected:
- backend unit tests PASS
- `DontCare` death tests PASS
- positive `STORE` case PASS

**Step 5: Commit**

```bash
git add src/Backend/TriangleBootstrapDraw.hpp src/Backend/TriangleBootstrapDraw.cpp tests/BackendUnitTests/TriangleBootstrapDrawTests.cpp tests/VulkanUnitTests/DrawTests.cpp
git commit -m "Backend: gate triangle bootstrap write-back on attachment target state"
```

### Task 4: Cover dynamic rendering positive path and run focused verification

**Files:**
- Modify: `tests/VulkanUnitTests/DrawTests.cpp`
- Modify: `docs/BackendBringup.md`
- Test: `tests/VulkanUnitTests/DrawTests.cpp`

**Step 1: Write the failing test**

Add:

- `DrawTest.StrictGpuTriangleBootstrapWritesDynamicRenderingColorStoreStore`

This should verify that dynamic rendering with `storeOp = eStore` still produces the expected strict GPU write-back result.

**Step 2: Run test to verify it fails or is incomplete**

Run:

```bash
./build-cuda-bootstrap/draw-unittests --gtest_filter='DrawTest.StrictGpuTriangleBootstrapWritesDynamicRenderingColorStoreStore'
```

Expected:
- RED until dynamic-rendering extraction path is fully wired through the new target contract

**Step 3: Write minimal implementation**

Finish any missing dynamic-rendering target extraction / validation and update `docs/BackendBringup.md` focused test guidance to include the new coverage.

**Step 4: Run focused verification**

Run:

```bash
cmake --build build-cuda-bootstrap --target vk-unittests backend-unittests draw-unittests --parallel 1
./build-cuda-bootstrap/backend-unittests --gtest_filter='TriangleBootstrapDraw.*'
./build-cuda-bootstrap/vk-unittests --gtest_filter='GraphicsBackendPipeline.*'
SWIFTSHADER_GPU_ALLOW_CPU_FALLBACK=1 SWIFTSHADER_CUDA_DUMP_SOURCE=0 ./build-cuda-bootstrap/draw-unittests --gtest_filter='DrawTest.DynamicRenderingSolidColorTriangle:DrawTest.StrictGpuTriangleBootstrapRejectsRenderPassColorStoreDontCare:DrawTest.StrictGpuTriangleBootstrapRejectsDynamicRenderingColorStoreDontCare:DrawTest.StrictGpuTriangleBootstrapWritesRenderPassColorStoreStore:DrawTest.StrictGpuTriangleBootstrapWritesDynamicRenderingColorStoreStore:DrawTest.TexturedTriangleNearest'
```

Expected:
- all commands PASS

**Step 5: Commit**

```bash
git add tests/VulkanUnitTests/DrawTests.cpp docs/BackendBringup.md
git commit -m "Tests: cover attachment target contract in strict GPU draw"
```
