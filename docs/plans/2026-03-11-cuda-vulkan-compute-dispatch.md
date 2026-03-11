# CUDA Vulkan Compute Dispatch Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 打通 `SWIFTSHADER_CUSTOM_GPU_USE_CUDA` 构建下真实的 Vulkan compute dispatch 到 CUDA runtime 的端到端执行，并让 `ComputeTests` 的第一批窄 case 开始真实通过。

**Architecture:** 先沿现有 custom backend queue execution seam，把 compute dispatch 降成一个最小 POD ABI，仅支持 `storage buffer + vkCmdDispatch + gl_GlobalInvocationId`。先让 `Memcpy` 和 `GlobalInvocationId` 跑通，再逐步放开控制流与更复杂资源模型。

**Tech Stack:** SwiftShader Vulkan frontend, custom backend execution seam, `CudaCompilerDriver`, `CudaRuntimeAPI`, GoogleTest, Vulkan unit tests

---

### Task 1: 收紧首批 compute 验收面

**Files:**
- Modify: `tests/VulkanUnitTests/ComputeTests.cpp`
- Modify: `tests/VulkanUnitTests/ComputeBackendPipelineTests.cpp`

**Step 1: 写出首批期望通过的窄验收集**

- 保留 CUDA build 下 `ComputeTests` 的总入口，但把计划中的首批 case 标记为真实执行目标：
  - `Memcpy`
  - `GlobalInvocationId`
- 其余 case 暂时允许细粒度 skip，而不是整个 fixture skip。

**Step 2: 先写失败测试/断言调整**

- 让 `ComputeBackendPipelineTests` 的 dispatch case 在 CUDA build 下不再 `GTEST_SKIP()`，而是要求看到真实 CUDA launch。
- 先运行：
  - `cmake --build build-cuda-bootstrap --target vk-unittests --parallel 4`
  - `(cd build-cuda-bootstrap && SWIFTSHADER_CUDA_DUMP_SOURCE=0 ./vk-unittests --gtest_filter='ComputeBackendPipelineTest.DispatchUsesFakeRuntimeWhenCustomBackendEnabled')`
- 预期：FAIL，因为真实 Vulkan compute dispatch 还没接通。

**Step 3: 提交测试准备检查点**

```bash
git add tests/VulkanUnitTests/ComputeTests.cpp tests/VulkanUnitTests/ComputeBackendPipelineTests.cpp
git commit -m "VulkanUnitTests: target real CUDA compute dispatch"
```

### Task 2: 找到 Vulkan compute dispatch 的真实执行接线点

**Files:**
- Inspect/Modify: `src/Vulkan/VkCommandBuffer.cpp`
- Inspect/Modify: `src/Vulkan/VkQueue.cpp`
- Inspect/Modify: `src/Vulkan/VkPipeline.hpp`
- Inspect/Modify: `src/Vulkan/VkPipeline.cpp`
- Inspect/Modify: `src/Backend/ExecutionBackend.hpp`
- Inspect/Modify: `src/Backend/ExecutionBackendFactory.cpp`

**Step 1: 明确 `vkCmdDispatch` 记录和 submit 的现有路径**

- 追清：
  - dispatch command 记录在哪里
  - submit 时谁解释 compute command
  - 现在哪条路径仍然落回 CPU 或空实现

**Step 2: 写最小失败回归记录**

- 在 `progress.md` / `findings.md` 记录当前 compute dispatch 路径、回退点和首个缺失环节。

**Step 3: 定义 custom backend compute entry seam**

- 若还没有独立 entry，新增一个最小 compute-dispatch backend hook。
- 该 hook 参数先只包含：
  - backend executable
  - groupCountX/Y/Z
  - localSizeX/Y/Z
  - descriptor/resource table

**Step 4: 运行窄测试确认还在 fail**

- 只跑：
  - `ComputeBackendPipelineTest.DispatchUsesFakeRuntimeWhenCustomBackendEnabled`

**Step 5: 提交 seam 检查点**

```bash
git add src/Vulkan/VkCommandBuffer.cpp src/Vulkan/VkQueue.cpp src/Backend/ExecutionBackend.hpp src/Backend/ExecutionBackendFactory.cpp
git commit -m "Backend: add compute dispatch execution seam"
```

### Task 3: 定义最小 compute launch ABI

**Files:**
- Create or Modify: `src/Backend/ComputeDispatch.hpp`
- Modify: `src/Backend/CudaRuntimeAPI.*`
- Modify: `src/Backend/RuntimeAPI.*`
- Modify: `src/Vulkan/VkDescriptorSet*.cpp`
- Modify: `src/Vulkan/VkBuffer*.cpp`

**Step 1: 写 ABI 结构**

- 定义 host-side `ComputeDispatchParams` / `ComputeBufferBinding`：
  - buffer device pointer
  - binding number
  - descriptor offset/range
  - group counts
  - local sizes

**Step 2: 先只支持 storage buffer**

- 明确只解析：
  - `VK_DESCRIPTOR_TYPE_STORAGE_BUFFER`
- 遇到其它 descriptor type：
  - 在 CUDA compute path 明确报 unsupported / skip，而不是 silently 错误执行。

**Step 3: 写窄验证**

- 跑：
  - `ComputeParams/SwiftShaderVulkanBufferToBufferComputeTest.Memcpy/0`
- 预期：仍 fail，但能确认 buffer table / params 已形成。

**Step 4: 提交 ABI 检查点**

```bash
git add src/Backend/ComputeDispatch.hpp src/Backend/CudaRuntimeAPI.* src/Backend/RuntimeAPI.* src/Vulkan/VkDescriptorSet*.cpp src/Vulkan/VkBuffer*.cpp
git commit -m "Backend: add minimal compute dispatch ABI"
```

### Task 4: 让 compute codegen 支持最小 builtin 和 storage buffer 访问

**Files:**
- Modify: `src/Backend/*Compute*`
- Modify: `src/Pipeline/SpirvShader*`
- Modify: `src/Vulkan/VkPipeline.cpp`
- Modify: backend compute codegen files identified in Task 2

**Step 1: 写 `Memcpy` 的失败测试执行**

- 跑：
  - `(cd build-cuda-bootstrap && SWIFTSHADER_CUDA_DUMP_SOURCE=0 ./vk-unittests --gtest_filter='ComputeParams/SwiftShaderVulkanBufferToBufferComputeTest.Memcpy/0')`
- 预期：FAIL，输出仍不正确。

**Step 2: 实现最小 codegen**

- 只支持：
  - 从 binding 读取 storage buffer
  - 按 `gl_GlobalInvocationId.x` 索引元素
  - 写回 storage buffer

**Step 3: 实现 builtin 映射**

- kernel 中正确导出：
  - `gl_GlobalInvocationId.x/y/z`
- local size 取自 shader declared local size。

**Step 4: 验证 `Memcpy` 通过**

- 跑：
  - `Memcpy/0`
  - `Memcpy/1`

**Step 5: 提交 codegen 检查点**

```bash
git add src/Backend src/Pipeline src/Vulkan/VkPipeline.cpp
git commit -m "Backend: execute narrow Vulkan compute memcpy on CUDA"
```

### Task 5: 打通 `GlobalInvocationId`

**Files:**
- Modify: same compute codegen/runtime files from Task 4
- Test: `tests/VulkanUnitTests/ComputeTests.cpp`

**Step 1: 写 `GlobalInvocationId` 的失败运行**

- 跑：
  - `GlobalInvocationId/0`
  - `GlobalInvocationId/1`

**Step 2: 校正 grid/block 与 builtin 语义**

- 确保：
  - `gridDim = dispatch group count`
  - `blockDim = local size`
  - `global = group * local + localInvocation`

**Step 3: 验证第一批 case**

- 跑：
  - `Memcpy/*`
  - `GlobalInvocationId/*`

**Step 4: 提交 builtin 检查点**

```bash
git add src/Backend src/Pipeline tests/VulkanUnitTests/ComputeTests.cpp
git commit -m "Backend: support global invocation id in CUDA compute"
```

### Task 6: 放开 Vulkan-side dispatch smoke 测试

**Files:**
- Modify: `tests/VulkanUnitTests/ComputeBackendPipelineTests.cpp`

**Step 1: 去掉 CUDA build 下 dispatch smoke 的 skip**

- 让测试断言真实 CUDA launch stamp 出现。

**Step 2: 跑窄 smoke**

- `ComputeBackendPipelineTest.DispatchUsesFakeRuntimeWhenCustomBackendEnabled`

**Step 3: 提交 smoke 检查点**

```bash
git add tests/VulkanUnitTests/ComputeBackendPipelineTests.cpp
git commit -m "VulkanUnitTests: verify real CUDA compute dispatch"
```

### Task 7: 细粒度管理剩余 compute suite

**Files:**
- Modify: `tests/VulkanUnitTests/ComputeTests.cpp`

**Step 1: 仅保留未支持 case 的细粒度 skip**

- 已支持：
  - `Memcpy`
  - `GlobalInvocationId`
- 未支持的控制流 case 可以先局部 skip，并写清楚原因。

**Step 2: 运行全部 compute 参数化 suite**

- `./build-cuda-bootstrap/vk-unittests --gtest_filter='ComputeParams/SwiftShaderVulkanBufferToBufferComputeTest.*'`

**Step 3: 记录通过/跳过矩阵**

- 把已支持 / 未支持 case 记入 `progress.md` 和 `findings.md`。

**Step 4: 提交 suite 管理检查点**

```bash
git add tests/VulkanUnitTests/ComputeTests.cpp progress.md findings.md
git commit -m "VulkanUnitTests: narrow supported CUDA compute cases"
```

### Task 8: 全量验证与文档更新

**Files:**
- Modify: `docs/BackendBringup.md`
- Modify: `progress.md`
- Modify: `findings.md`
- Modify: `task_plan.md`

**Step 1: 跑回归**

- `cmake --build build-cuda-bootstrap --target vk-unittests --parallel 4`
- `(cd build-cuda-bootstrap && SWIFTSHADER_CUDA_DUMP_SOURCE=0 ./vk-unittests --gtest_fail_fast)`
- `(cd build-cuda-bootstrap && SWIFTSHADER_CUDA_DUMP_SOURCE=0 ./backend-unittests)`

**Step 2: 更新文档**

- 在 `docs/BackendBringup.md` 说明：
  - CUDA build 下 Vulkan compute dispatch 的当前支持范围
  - 已打通哪些 `ComputeTests`
  - 仍未支持哪些 case

**Step 3: 提交最终检查点**

```bash
git add docs/BackendBringup.md progress.md findings.md task_plan.md
git commit -m "Docs: record CUDA Vulkan compute dispatch status"
```
