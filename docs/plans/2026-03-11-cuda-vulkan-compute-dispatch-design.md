# CUDA Vulkan Compute Dispatch Design

**Goal:** 在 `SWIFTSHADER_CUSTOM_GPU_USE_CUDA` 构建下打通真实的 `Vulkan compute dispatch -> CUDA runtime` 端到端路径，让 `tests/VulkanUnitTests/ComputeTests.cpp` 中的第一批参数化 compute case 不再依赖跳过逻辑，而是开始真实执行并通过。

## Scope

本轮只覆盖最窄但完整的一条 compute 路径：

- 单个 compute pipeline
- 单个 descriptor set
- 仅 `VK_DESCRIPTOR_TYPE_STORAGE_BUFFER`
- `vkCmdDispatch()`
- workgroup 维度映射到 CUDA grid / block
- shader 可见 builtin 先只要求 `gl_GlobalInvocationId`
- 目标优先跑通 `Memcpy` 与 `GlobalInvocationId` 两组 `ComputeTests`

明确不在本轮范围内：

- image / sampler / texel buffer / uniform buffer
- push constants / specialization constants 的完整接线
- 多 descriptor set / dynamic offsets / descriptor indexing
- barriers、async overlap、timeline semaphore 优化
- 更复杂控制流 case 全量放开

## Current State

仓库当前已经具备以下 compute 相关基础：

- custom backend build flag 与 CUDA runtime / compiler driver
- backend executable 生成与 fake runtime dispatch 验证
- Vulkan compute pipeline 创建可生成 backend executable
- `ComputeBackendPipelineTests` 已明确记录：真实 CUDA-backed Vulkan compute dispatch 还未端到端接线
- `ComputeTests` 在 CUDA 构建下目前整体 skip，因为真实 Vulkan compute dispatch 仍返回全零

这意味着当前缺口不在“能否编译 compute shader”，而在：

1. Vulkan command recording / submit 路径没有把 compute dispatch 真实导向 custom CUDA backend；
2. descriptor / buffer 绑定信息没有形成可执行的 CUDA launch ABI；
3. compute builtin 和 dispatch dimensions 没有进入真实 kernel 执行。

## Recommended Approach

推荐采用“先窄后宽”的端到端接线方案：

1. 先只支持 `storage buffer + dispatch + gl_GlobalInvocationId`
2. 把 Vulkan compute submit 降到一个最小 `ComputeDispatchParams` POD ABI
3. 由 custom backend 在 queue submit / command execution 中识别 compute dispatch，构造 CUDA launch 参数并执行真实 kernel
4. 用 `ComputeTests.Memcpy` 与 `ComputeTests.GlobalInvocationId` 做第一批验收

推荐理由：

- 这是最短闭环，能最快给出真实端到端信号；
- 一旦 `Memcpy` / `GlobalInvocationId` 打通，后续控制流 / 分支类 case 主要是 shader lowering 问题，而不是执行框架问题；
- 风险局限在 compute path，不需要重新扰动已稳定的 draw / present 路径。

## Architecture

### 1. Vulkan Frontend Responsibilities

Vulkan 前端继续负责：

- pipeline 创建、descriptor set 更新、command buffer 录制
- queue submit 生命周期与现有同步模型
- 资源对象与内存对象管理

新增职责是把 compute dispatch 所需的最小执行信息从现有 Vulkan 对象中提取出来，传给 custom backend。该信息至少包含：

- backend executable / entrypoint
- dispatch group counts
- shader-declared local size
- 已绑定 storage buffer 的 device pointer / offset / size
- descriptor binding 到 shader 参数槽位的映射

### 2. Compute Dispatch ABI

需要定义一个最小、稳定、POD 风格的 compute launch ABI。建议分成两层：

- **Host-side dispatch params**
  - pipeline executable handle
  - groupCountX/Y/Z
  - localSizeX/Y/Z
  - descriptor metadata
  - resource table / buffer bindings

- **Kernel-visible params**
  - storage buffer base pointers
  - storage buffer ranges / offsets
  - dispatch dimensions
  - builtin support payload

kernel-visible params 必须避免携带宿主 C++ 对象或 Vulkan 句柄，只保留 plain-old-data 与可直接传入 CUDA launch 的指针/数值。

### 3. Builtin Mapping

第一批 case 只需要正确支持：

- `gl_GlobalInvocationId.x/y/z`

映射规则应显式固定为：

- `globalX = blockIdx.x * blockDim.x + threadIdx.x`
- `globalY = blockIdx.y * blockDim.y + threadIdx.y`
- `globalZ = blockIdx.z * blockDim.z + threadIdx.z`

其中：

- `blockDim` 取 shader-declared local size
- `gridDim` 取 Vulkan dispatch group count

这要求 compute codegen 与 runtime launch 在 local size / group count 语义上完全一致。

### 4. Descriptor / Buffer Binding Strategy

第一阶段只接 `VK_DESCRIPTOR_TYPE_STORAGE_BUFFER`：

- host 端从 bound descriptor set 中解析每个 storage buffer 的 `VkBuffer`、memory offset、descriptor offset/range
- backend 将其转换成 kernel 可见的 buffer table
- shader lowering 通过 binding 编号索引对应 buffer

为了降低风险，首版可以只支持：

- set = 0
- bindings 连续或显式映射
- 每个 binding 一个 storage buffer

后续再推广到多 set / dynamic offset。

### 5. Execution Path

推荐接线点是现有 custom backend 的 command execution / queue submit 路径，而不是重新造一条旁路：

1. command buffer 记录 `vkCmdBindPipeline` / `vkCmdBindDescriptorSets` / `vkCmdDispatch`
2. submit 时进入 backend-neutral queue execution seam
3. 若当前为 custom CUDA build 且 command 为 compute dispatch：
   - 提取 pipeline / descriptors / group counts
   - 构造 `ComputeDispatchParams`
   - 通过 `CudaRuntimeAPI` 启动真实 kernel
4. 保留现有 CPU 路径作为非-CUDA build 或未支持场景回退

### 6. Testing Strategy

按 TDD 逐层推进：

- backend / Vulkan smoke：确认 dispatch 真正走到 CUDA runtime，而不是 fake runtime / CPU 路径
- 窄 Vulkan tests：
  - `Memcpy`
  - `GlobalInvocationId`
- 通过后再放开更多 `ComputeTests` case

跳过策略也要同步收缩：

- 不再整体 skip `ComputeTests`
- 只对暂未支持的特性或 case 细粒度 skip

## Risks

### Risk 1: compute executable 与 Vulkan dispatch ABI 不一致

症状会表现为：

- 所有输出为零
- 只有 index 0 正确
- local size 改变后行为错乱

缓解方式：

- 先用 `Memcpy` 和 `GlobalInvocationId` 两个最简单的 case 定死 ABI
- 在测试中覆盖多个 `localSizeX`

### Risk 2: descriptor buffer 地址/offset 解释错误

症状会表现为：

- 读写落在错误偏移
- guard magic 被破坏
- 输入正确但输出全零

缓解方式：

- 优先复用 `ComputeTests` 里现成的 guard layout
- 先只支持 storage buffer，避免同时引入 descriptor 复杂性

### Risk 3: queue submit 路径错误地走回 CPU 或 fake runtime

缓解方式：

- 在 `ComputeBackendPipelineTests` 增加“真实 CUDA launch occurred”的断言
- 在 Vulkan compute 路径保留 stamp/source dump 辅助观测，但不把脆弱源码子串当成核心验收

## Success Criteria

本轮完成标准定义为：

1. `build-cuda-bootstrap/vk-unittests` 中不再整体 skip `ComputeTests`
2. 至少以下 case 在 CUDA build 下真实执行并通过：
   - `ComputeParams/SwiftShaderVulkanBufferToBufferComputeTest.Memcpy/*`
   - `ComputeParams/SwiftShaderVulkanBufferToBufferComputeTest.GlobalInvocationId/*`
3. `ComputeBackendPipelineTests` 中 dispatch 相关测试从 skip 转为真实 launch 验证
4. 非 CUDA build 行为保持不变

## Out of Scope Follow-ups

如果第一批 compute case 打通，下一轮自然扩展顺序应为：

1. `BranchSimple` / `BranchDeclareSSA`
2. 其他控制流 case
3. descriptor / push constant 扩展
4. 更完整的 Vulkan compute synchronization / barrier 语义
