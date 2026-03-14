# Triangle Bootstrap Separate-Image-Sampler Materialization Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Extend strict GPU triangle bootstrap so direct-sample separate image/sampler pipelines can materialize `Texture2DColor` fragment bootstrap state.

**Architecture:** Keep `GraphicsExecutable` as the owner of pipeline-time sampled-image plan metadata. Promote narrow separate image/sampler plans to `bootstrapSupported`, and change `TriangleBootstrapDraw` to consume the full texture plan instead of only the combined-binding compatibility accessor.

**Tech Stack:** C++, GoogleTest, Vulkan draw/unit tests, SwiftShader Vulkan pipeline/bootstrap code

---

### Task 1: Reproduce the strict GPU draw failure

**Files:**
- Existing test: `tests/VulkanUnitTests/DrawTests.cpp`

**Step 1: Run the existing focused draw test**
Run: `SWIFTSHADER_CUDA_DUMP_SOURCE=0 ./build-cuda-bootstrap/draw-unittests --gtest_filter='DrawTest.TexturedTriangleSeparateImageSamplerNearest'`
Expected: FAIL because strict GPU bootstrap currently falls back to the default fragment/bootstrap behavior for separate image/sampler.

### Task 2: Promote narrow separate image/sampler plans to bootstrap-supported

**Files:**
- Modify: `src/Backend/GraphicsExecutable.cpp`

**Step 1: Keep classification rules unchanged**
- Continue classifying one sampled image + one sampler as `SeparateImageSampler`.

**Step 2: Update support flag**
- Mark that plan as `bootstrapSupported` when the fragment output still satisfies the existing direct-sample passthrough rule.

### Task 3: Materialize separate image/sampler at draw time

**Files:**
- Modify: `src/Backend/TriangleBootstrapDraw.cpp`

**Step 1: Replace combined-only config building**
- Add descriptor reads for:
  - one sampled-image binding
  - one sampler binding
- Reuse the existing texture-format and sampler-state validation rules.

**Step 2: Consume the richer texture plan**
- Stop gating texture bootstrap materialization on `hasBootstrapTextureBinding()`.
- Gate on `hasTexturePlan()` plus `texturePlan.bootstrapSupported` instead.

### Task 4: Verify draw and metadata regressions

**Files:**
- Modify: `task_plan.md`
- Modify: `progress.md`
- Modify: `findings.md`
- Modify: `docs/BackendBringup.md`

**Step 1: Run focused verification**
Run:
- `SWIFTSHADER_CUDA_DUMP_SOURCE=0 ./build-cuda-bootstrap/draw-unittests --gtest_filter='DrawTest.TexturedTriangleSeparateImageSamplerNearest'`
- `./build-cuda-bootstrap/vk-unittests --gtest_filter='GraphicsBackendPipeline.RejectsTextureBootstrapBindingForSeparateImageSampler'`

**Step 2: Run broader verification**
Run:
- `cmake --build build-cuda-bootstrap --target vk-unittests backend-unittests draw-unittests --parallel 1`
- `./build-cuda-bootstrap/vk-unittests --gtest_filter='GraphicsBackendPipeline.*:BackendSmoke.*:GraphicsBackendSelection.*'`
- `./build-cuda-bootstrap/backend-unittests --gtest_filter=GraphicsExecutable.*`
- `SWIFTSHADER_CUDA_DUMP_SOURCE=0 ./build-cuda-bootstrap/draw-unittests --gtest_filter='DrawTest.SolidColorTriangle:DrawTest.TexturedTriangleNearest:DrawTest.TexturedTriangleSeparateImageSamplerNearest'`
- `./build-cuda-bootstrap/backend-unittests`
- `git diff --check`
