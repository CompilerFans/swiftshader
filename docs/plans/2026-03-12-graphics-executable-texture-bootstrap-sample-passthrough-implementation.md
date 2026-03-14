# Graphics Executable Texture Bootstrap Sample Passthrough Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Accept trivial sample pass-through shaders in `GraphicsExecutable` texture-bootstrap metadata without re-accepting true post-processing.

**Architecture:** Extend the existing narrow direct-sample analysis in `GraphicsExecutable.cpp` into a tiny recursive/iterative resolver for the final `location 0` value. The resolver follows only pass-through instructions until it either reaches a supported image-sample instruction or proves the shader is outside the narrow path.

**Tech Stack:** C++, GoogleTest, Vulkan unit tests, SwiftShader SPIR-V reflection

---

### Task 1: Add the failing Vulkan pipeline test

**Files:**
- Modify: `tests/VulkanUnitTests/GraphicsBackendPipelineTests.cpp`

**Step 1: Write the failing test**
- Add a textured pipeline whose fragment shader stores `texture(texSampler, inTexCoord)` into a local `vec4` and then writes that local to `outColor`.
- Expect `graphicsPipelineBootstrapTextureState(...).hasBinding == true`.

**Step 2: Run test to verify it fails**
Run: `cmake --build build-cuda-bootstrap --target vk-unittests --parallel 1 && ./build-cuda-bootstrap/vk-unittests --gtest_filter='GraphicsBackendPipeline.ExtractsTextureBootstrapBindingWhenFragmentUsesSamplePassthrough'`
Expected: FAIL because the current extractor only accepts a direct sample result at the final store.

### Task 2: Implement minimal pass-through resolution

**Files:**
- Modify: `src/Backend/GraphicsExecutable.cpp`

**Step 1: Write minimal implementation**
- Add a tiny resolver that starts from the final stored value for `location 0` and follows only trivial pass-through instructions.
- Accept only if the chain terminates at a supported image-sample instruction.
- Continue rejecting arithmetic/post-processing and multiple distinct final stored values.

**Step 2: Run focused test to verify it passes**
Run: `cmake --build build-cuda-bootstrap --target vk-unittests --parallel 1 && ./build-cuda-bootstrap/vk-unittests --gtest_filter='GraphicsBackendPipeline.ExtractsTextureBootstrapBindingWhenFragmentUsesSamplePassthrough'`
Expected: PASS

### Task 3: Re-run focused/broader validation and update docs

**Files:**
- Modify: `task_plan.md`
- Modify: `progress.md`
- Modify: `findings.md`
- Modify: `docs/BackendBringup.md`

**Step 1: Run verification**
Run:
- `cmake --build build-cuda-bootstrap --target vk-unittests backend-unittests draw-unittests --parallel 1`
- `./build-cuda-bootstrap/vk-unittests --gtest_filter='GraphicsBackendPipeline.*:BackendSmoke.*'`
- `./build-cuda-bootstrap/backend-unittests --gtest_filter=GraphicsExecutable.*`
- `./build-cuda-bootstrap/draw-unittests --gtest_filter=DrawTest.SolidColorTriangle`
- `./build-cuda-bootstrap/vk-unittests --gtest_filter='GraphicsBackendPipeline.*:BackendSmoke.*:GraphicsBackendSelection.*'`
- `./build-cuda-bootstrap/backend-unittests`
- `git diff --check`

**Step 2: Update tracking docs**
- Record the new narrow rule: trivial pass-through from a supported sample is accepted, but true post-processing is still rejected.
