# GPU Backend Refactor Design

## Goal

清理当前 GPU 后端代码中的开发痕迹，统一命名，去除误导性表述，同时保留当前 CUDA bring-up 所需的最小调试/回退抓手。

## Baseline

- 参考提交 `313545f85af72f954820e54f4110cda591a6cf7b` 仅作为“GPU 后端开发前的干净基线”。
- 该提交本身不包含 GPU 后端实现，因此本次重构不以该提交中的接口命名为直接来源，而以“去实验痕迹、统一语义”为目标。

## Decisions

### 1. `CUSTOM_GPU` 全量改为 `GPU`

- 编译开关：
  - `SWIFTSHADER_ENABLE_CUSTOM_GPU_BACKEND` → `SWIFTSHADER_ENABLE_GPU_BACKEND`
  - `SWIFTSHADER_CUSTOM_GPU_USE_CUDA` → `SWIFTSHADER_GPU_USE_CUDA`
- 运行时环境变量：
  - `SWIFTSHADER_CUSTOM_GPU_*` → `SWIFTSHADER_GPU_*`
- 代码符号：
  - `BackendKind::CUSTOM_GPU` → `BackendKind::GPU`
  - `CustomExecutionBackend` / `createCustomExecutionBackend()` → `GpuExecutionBackend` / `createGpuExecutionBackend()`
  - `createCustomPresentAdapter()` → `createGpuPresentAdapter()`

不保留兼容别名，直接一次性完成迁移。

### 2. `FakeRuntime` 规范为 `StubRuntime`

- `FakeRuntimeAPI` 实际承担的是非 CUDA GPU build 下的占位 runtime，而非纯测试 double。
- 因此统一改为：
  - `FakeRuntimeAPI` → `StubRuntimeAPI`
  - 相关测试名中 `FakeRuntime` → `StubRuntime`

### 3. 保留 fallback 机制，但降级为显式 bring-up 开关

- `ALLOW_CPU_FALLBACK` 仍保留。
- 原因：当前图形 / compute / submit 路径仍需要该开关做定位、回归和局部 bring-up。
- 但要移除误导性的默认语义：
  - 测试名中不再出现 `FallsBack`
  - 文档中明确它是“显式调试开关”，不是默认行为目标

### 4. 宏只保留必要的两层

- 保留：
  - `SWIFTSHADER_ENABLE_GPU_BACKEND`
  - `SWIFTSHADER_GPU_USE_CUDA`
- 不做更大规模的宏消除改造。
- 原因：CUDA 头文件、构建依赖和部分测试路径仍依赖编译期开关；当前阶段强行全部动态化，收益低于改动成本。

### 5. 其他必要清理

- 日志前缀 `[custom-gpu]` → `[gpu]`
- `GraphicsBootstrapMode::CustomWithCpuGraphicsFallback` 改为更准确的 GPU 语义命名
- Present adapter 内部占位类型 `FakePresentAdapter` 改为 `GpuPresentAdapter`
- 文档、基准脚本、presubmit 配置、单元测试过滤名全部同步

## Validation

- 先把相关测试和命名切换到新语义，确认旧实现下出现编译/测试失败。
- 再完成最小实现改造。
- 重点验证：
  - `backend-unittests`
  - 聚焦 `vk-unittests` 中 backend / compute backend 相关 case
  - `git grep` / `rg` 确认源码和文档中无旧 `CUSTOM_GPU` / `FakeRuntime` 遗留

## Non-goals

- 不引入 CPU/GPU 动态热切换。
- 不在本次重构里推进新的 compute/graphics 功能。
- 不大规模重写 backend 架构，仅做语义统一和开发痕迹清理。
