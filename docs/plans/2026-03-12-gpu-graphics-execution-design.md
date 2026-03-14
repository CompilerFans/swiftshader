# GPU Graphics Execution Refactor Design

**Date:** 2026-03-12

**Goal**

把 Vulkan graphics draw 的执行入口从 CPU `Renderer::draw()` 内部的 GPU 旁路，重构为 backend-owned draw dispatch。第一阶段目标不是完成完整 graphics backend，而是先把 draw 路由层次改对，让 `ExecutionBackend` 成为 draw 的直接执行者。

## Problem Statement

当前 draw 链路是：

`CmdDrawBase::draw()` → `sw::Renderer::draw()` → 在 `Renderer::draw()` 内部判断 runtime / env → 走 triangle bootstrap 或落回 CPU `DrawCall::run()`

这个结构有三个主要问题：

1. **插层过晚**：draw 已经深度进入 CPU renderer 后，才尝试 GPU 化。
2. **职责混杂**：`Renderer::draw()` 同时承担 CPU renderer、GPU bootstrap、strict abort、fallback 判断。
3. **后续难扩展**：真正的 graphics backend、resource model、queue 语义都没法自然挂接到 `ExecutionBackend`。

## Constraints

- 不能一次跳到完整 `GraphicsExecutable`，风险过高、改动面过大。
- 当前 triangle bootstrap 仍然是唯一真正能触发 CUDA graphics launch 的窄通路，短期内必须保留。
- CPU draw 语义不能被这次重构破坏。
- 现有环境变量和 strict/fallback bring-up 行为要继续可用。

## Approaches

### Option A: 继续把 GPU draw 保留在 `Renderer::draw()`

**做法**
- 只整理 `Renderer.cpp` 内部代码，继续由 renderer 自己判断 GPU/bootstrap/fallback。

**优点**
- 改动最小。

**缺点**
- 错层问题不解决。
- 后面接 `GraphicsExecutable` 仍然会撞到同样的问题。

**结论**
- 不采用。

### Option B: 给 `ExecutionBackend` 增加 draw seam，由 backend 决策 draw 路由

**做法**
- `CmdDrawBase` 把准备好的 draw 参数交给 `ExecutionBackend::draw(...)`
- CPU backend 直接转发到 CPU renderer
- GPU backend 先尝试 triangle bootstrap helper，未处理时再按 fallback/strict 规则决定是否回 CPU
- `Renderer::draw()` 收敛为 CPU-only

**优点**
- 第一时间把 draw 调度层次改对
- 仍然可以复用现有 triangle bootstrap
- 为后续 `GraphicsExecutable` 留出自然演进位点

**缺点**
- 需要抽出一组 draw 参数结构与 helper
- 仍然不是最终形态，后续还要继续拆 graphics state / resource model

**结论**
- 采用。

### Option C: 直接上完整 `GraphicsExecutable` / backend graphics program

**做法**
- 在 pipeline 创建期生成 graphics executable，draw 时直接 lower Vulkan state 并 launch。

**优点**
- 最终形态更正确。

**缺点**
- 牵涉 pipeline ABI、resource handle、attachments、descriptor lowering、barrier/present 等，超出当前可控首刀范围。

**结论**
- 作为后续阶段，不作为本轮切片。

## Chosen Design

### 1. 新的 draw dispatch seam

给 `ExecutionBackend` 增加显式 draw 入口。`CmdDrawBase` 不再直接依赖 `sw::Renderer::draw()`，而是把 draw 参数传给 backend。

这意味着：

- `VkCommandBuffer` 仍负责准备 descriptor、vertex/index 输入和 render area
- backend 负责决定 draw 究竟由谁执行

### 2. CPU/GPU backend 各自承担执行决策

- **CPU backend**
  - 只做一件事：确保 `sw::Renderer` 存在，并调用 CPU draw

- **GPU backend**
  - 如果有可用 runtime，先尝试 GPU triangle bootstrap helper
  - 如果 helper 成功写回附件，则本次 draw 已完成
  - 如果 helper 未处理/失败：
    - 允许 fallback 时走 CPU renderer
    - strict GPU 模式下直接 abort

### 3. 把 triangle bootstrap 从 `Renderer.cpp` 抽成 backend helper

新 helper 负责：

- 读取 pipeline / dynamic state / 输入流
- 推导 fragment bootstrap config / texture config / point size
- 调用 `runTrianglePipelineBootstrap(...)`
- 把颜色结果写回 color attachment

`Renderer.cpp` 不再感知 runtime、triangle bootstrap、strict/fallback env 决策。

### 4. 本轮刻意不做的事

- 不实现完整 `GraphicsExecutable`
- 不重做 transfer/copy/blit/resolve/present
- 不改变 queue submit 的全同步语义
- 不改变当前宏开关策略

这些都保留到下一阶段。

## Data Flow After Refactor

`CmdDrawBase`
→ 绑定/更新 inputs
→ 构造 draw params
→ `executionBackend->draw(params)`

CPU backend:
→ `Renderer::draw(...)`
→ `DrawCall::run(...)`

GPU backend:
→ `tryTriangleBootstrapDraw(...)`
→ success: return
→ else fallback allowed? yes → `Renderer::draw(...)`
→ else abort

## Testing Strategy

### Backend unit tests
- 覆盖 draw routing policy 的最小语义
- 覆盖 GPU strict 模式与 CPU fallback 模式的选择结果

### Draw/Vulkan focused tests
- 现有 `DrawTest.SolidColorTriangle`
- 现有 narrow graphics smoke / backend selection tests

### Success Criteria
- `CmdDrawBase` 不再直接调用 `renderer->draw()`
- `Renderer::draw()` 不再包含 runtime/triangle-bootstrap/fallback 决策
- CUDA build 下现有窄 draw 验证仍可触发 GPU launch
- CPU build 行为保持不变
