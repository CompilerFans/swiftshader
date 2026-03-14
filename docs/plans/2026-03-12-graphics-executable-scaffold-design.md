# Graphics Executable Scaffold Design

**Date:** 2026-03-12

**Goal**

为 graphics pipeline 引入一个 backend-owned `GraphicsExecutable` 骨架，先承载最小 stage 元数据和 vertex lowering 信息，并把它挂到 `vk::GraphicsPipeline`。本轮不改变 draw 执行行为，也不引入新的 graphics launch 路径。

## Problem Statement

上一刀已经把 draw routing 从 `Renderer::draw()` 收到了 backend，但 graphics pipeline 创建期仍然没有 backend-owned executable/object model。结果是：

1. graphics backend 没有稳定的 pipeline-time 挂点；
2. vertex lowering 信息虽然能从 semantic IR 提取，但没有地方长期附着在 graphics pipeline 上；
3. 后续如果要把 triangle bootstrap helper 演进成正式 graphics execution 入口，只能继续把状态塞进临时 helper 或 runtime 分支。

## Constraints

- 不改现有 draw 语义，不影响 CPU draw 和当前 triangle bootstrap。
- 不在本轮实现 graphics launch / resource lowering / attachment lowering。
- 兼容 graphics pipeline library 的部分 pipeline：
  - 有 vertex 的 pipeline 可以创建 backend executable；
  - 只有 fragment 的 pipeline 不强行伪造 backend executable。

## Approaches

### Option A: 继续只在 draw 时临时读取 shader/state

**优点**
- 当前改动最小。

**缺点**
- pipeline-time backend ownership 继续缺失；
- vertex lowering 等信息仍然没有稳定宿主；
- 后续正式 graphics execution 还得再拆一轮。

**结论**
- 不采用。

### Option B: 引入 metadata-only `GraphicsExecutable`

**做法**
- 新增 backend 对象，持有：
  - vertex stage entry point
  - optional fragment stage entry point
  - vertex lowering info
- `GraphicsPipeline::compileShaders()` 在 shader 编译完成后构建 semantic IR，并在有 vertex stage 时创建 backend executable。

**优点**
- 给后续 graphics backend 留出自然的 pipeline-time 接入点；
- 风险低，不碰 draw 行为；
- 可以立即用测试锁住 stage 组合和 metadata 保真。

**缺点**
- 只是脚手架，不直接提供新功能。

**结论**
- 采用。

### Option C: 直接把 triangle bootstrap helper 替换成正式 graphics executable

**优点**
- 更接近最终形态。

**缺点**
- 会同时卷入 shader lowering、descriptor/resource ABI、attachments、present/resolve 等多个未收敛问题。

**结论**
- 作为下一阶段，不在本轮做。

## Chosen Design

### 1. Backend object shape

`backend::GraphicsExecutable` 只负责描述 graphics pipeline 的最小 backend-facing metadata：

- `valid()`
- `vertexEntryPoint()`
- `hasFragmentStage()`
- `fragmentEntryPoint()`
- `vertexLowering()`

创建规则：

- 必须有 vertex semantic module，且 stage 必须是 `VK_SHADER_STAGE_VERTEX_BIT`
- fragment semantic module 可缺省；如果存在，stage 必须是 `VK_SHADER_STAGE_FRAGMENT_BIT`

### 2. Graphics pipeline hookup

`vk::GraphicsPipeline` 增加：

- `std::shared_ptr<backend::GraphicsExecutable> backendExecutable`
- `bool hasBackendExecutable() const`

在 `compileShaders()` 中：

- 沿用现有 shader 编译流程
- 编译结束后，用 `SemanticIRBuilder` 分别为 vertex/fragment shader 构建 semantic modules
- 在有 vertex module 时创建 backend executable 并保存

在 `destroyPipeline()` 中 reset。

### 3. Pipeline library semantics

- pre-raster library 或完整 graphics pipeline：只要有 vertex stage，就创建 backend executable
- fragment-only library：不创建 backend executable

这样既不伪造缺失的 vertex lowering，也允许后续把 fragment-only library 作为单独问题处理。

## Testing Strategy

### Backend unit tests

- `GraphicsExecutable` 能保存 vertex lowering 和 stage entry point
- `GraphicsExecutable` 允许 vertex-only
- `GraphicsExecutable` 拒绝缺失 vertex 或错误 stage 组合

### Vulkan integration test

- 构造一个最小 graphics pipeline
- 断言 `vk::GraphicsPipeline::hasBackendExecutable()` 为 true

## Success Criteria

- backend 侧存在最小 `GraphicsExecutable`
- `vk::GraphicsPipeline` 在有 vertex stage 时拥有 backend executable
- 现有 draw 行为不变
- focused backend/vulkan tests 通过
