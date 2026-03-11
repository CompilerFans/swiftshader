# GPU Backend Refactor Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 统一 GPU 后端命名，去除 `CUSTOM_GPU` / `FakeRuntime` 等开发痕迹，并保持当前 CUDA bring-up 与严格模式行为不变。

**Architecture:** 先让测试和验证脚本切换到新的 `GPU` / `StubRuntime` 语义，制造明确的编译红灯；再以最小代码改动同步核心 backend、Vulkan 路径、文档和构建脚本。保留 CPU fallback 作为显式调试开关，但从命名和文档上降级其中心性。

**Tech Stack:** C++, CMake, GN, GoogleTest, Bash

---

### Task 1: 切换测试到新语义

**Files:**
- Modify: `tests/BackendUnitTests/BackendFactoryTests.cpp`
- Modify: `tests/VulkanUnitTests/BackendSelectionTests.cpp`
- Modify: `tests/VulkanUnitTests/GraphicsBackendSelectionTests.cpp`
- Modify: `tests/VulkanUnitTests/BackendSmokeTests.cpp`
- Modify: `tests/VulkanUnitTests/ComputeBackendPipelineTests.cpp`
- Modify: `tests/BackendUnitTests/RuntimeAPITests.cpp`
- Modify: `tests/BackendUnitTests/ComputeDispatchValidationTests.cpp`
- Modify: `tests/BackendUnitTests/TrianglePipelineBootstrapTests.cpp`

**Step 1: Write the failing test**

- 把 `BackendKind::CUSTOM_GPU` 改为 `BackendKind::GPU`
- 把 `FakeRuntimeAPI` / `FakeRuntime...` 测试名改为 `StubRuntimeAPI` / `StubRuntime...`
- 把 `FallsBack` 命名改为默认 backend 选择语义

**Step 2: Run test to verify it fails**

Run: `cmake --build build-cuda-bootstrap --target backend-unittests vk-unittests --parallel 1`

Expected: 编译失败，报旧符号不存在或旧头文件不存在。

**Step 3: Commit**

暂不提交，继续 Task 2。

### Task 2: 重命名 backend 核心符号

**Files:**
- Modify: `src/Backend/ExecutionBackend.hpp`
- Modify: `src/Backend/ExecutionBackendFactory.cpp`
- Modify: `src/Backend/GraphicsBackend.hpp`
- Modify: `src/Backend/GraphicsBackend.cpp`
- Modify: `src/Backend/BackendFactory.cpp`
- Modify: `src/Backend/BackendConfig.hpp`
- Modify: `src/Backend/PresentAdapter.hpp`
- Modify: `src/Backend/PresentAdapter.cpp`
- Modify: `src/Backend/CpuExecutionBackend.cpp`
- Modify: `src/Backend/CustomExecutionBackend.cpp`
- Modify: `src/Device/Renderer.cpp`
- Modify: `src/Vulkan/VkPipeline.cpp`

**Step 1: Write minimal implementation**

- 新语义替换旧符号和环境变量
- 统一日志前缀为 `[gpu]`
- 重命名 bootstrap / adapter / execution backend 内部类型

**Step 2: Run targeted build**

Run: `cmake --build build-cuda-bootstrap --target backend-unittests vk-unittests --parallel 1`

Expected: 编译通过。

### Task 3: 重命名 `StubRuntime` 文件与引用

**Files:**
- Move: `src/Backend/FakeRuntimeAPI.hpp` → `src/Backend/StubRuntimeAPI.hpp`
- Move: `src/Backend/FakeRuntimeAPI.cpp` → `src/Backend/StubRuntimeAPI.cpp`
- Modify: `src/Backend/CMakeLists.txt`
- Modify: `src/Backend/BUILD.gn`
- Modify: 所有包含 `FakeRuntimeAPI.hpp` 的测试与源码

**Step 1: Rename files and includes**

- 文件名、类名、静态方法名、测试引用全部同步

**Step 2: Run targeted build**

Run: `cmake --build build-cuda-bootstrap --target backend-unittests vk-unittests --parallel 1`

Expected: 编译通过。

### Task 4: 同步文档与脚本

**Files:**
- Modify: `docs/BackendBringup.md`
- Modify: `tests/presubmit.sh`
- Modify: `tests/VulkanBenchmarks/CMakeLists.txt`
- Modify: `tests/VulkanBenchmarks/run-animated-triangle-benchmark.sh`
- Modify: 其他使用旧宏/旧环境变量的测试源码

**Step 1: Update docs and scripts**

- 文档与命令行示例全部切换到新命名
- 明确 fallback 是显式调试开关

**Step 2: Verify no old names remain**

Run: `rg -n "SWIFTSHADER_(ENABLE_)?CUSTOM_GPU|CUSTOM_GPU|FakeRuntime|custom-gpu" src tests docs CMakeLists.txt BUILD.gn -g '!docs/plans/**'`

Expected: 无匹配。

### Task 5: 验证、打 tag、提交推送

**Files:**
- Modify: `docs/plans/2026-03-12-gpu-backend-refactor-design.md`
- Modify: `docs/plans/2026-03-12-gpu-backend-refactor.md`

**Step 1: Run verification**

Run:
- `./build-cuda-bootstrap/backend-unittests`
- `./build-cuda-bootstrap/vk-unittests --gtest_filter=BackendSmoke.*:BackendSelection.*:GraphicsBackendSelection.*:ComputeBackendPipelineTest.DispatchUsesStubRuntimeWhenGpuBackendEnabled`

Expected: 通过。

**Step 2: Add tag**

Run: `git tag gpu_init 313545f85af72f954820e54f4110cda591a6cf7b`

**Step 3: Commit**

Run:
- `git add ...`
- `git commit -m "Backend: rename custom GPU scaffolding"`

**Step 4: Push**

Run: `git push origin gpu_init`
- `git push origin HEAD:refs/for/master`
