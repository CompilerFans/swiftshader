# Graphics Attachment Lifecycle Tracking Design

**Date:** 2026-03-14

## Context

当前 graphics GPU bring-up 的 strict draw 仍由 `TriangleBootstrapDraw` 驱动。render pass / dynamic rendering 在 begin 阶段已经正确执行了 `loadOp == CLEAR`，但 draw-time 的 color write-back 仍是 ad-hoc 路径：

- `CmdDrawBase::draw()` 只把 pipeline、dynamic state、`renderArea`、`layer` 和 push constants 交给 backend。
- `TriangleBootstrapDraw.cpp` 直接从 `pipeline->getAttachments().colorBuffer[0]` 取 `vk::ImageView *`，再自行检查格式 / sample count 并把 RGBA buffer 写回。

这意味着 backend draw 并没有显式理解“当前活跃 color attachment 0 的生命周期状态”，尤其缺少：

- 当前 attachment 0 是否真的存在
- 当前 draw 使用的 attachment layout
- 当前 draw 是否允许对该 attachment 做 store/write-back
- render pass 与 dynamic rendering 两条路径的统一 draw-time contract

## Goal

为 strict GPU triangle bootstrap 引入一个最小、显式的 draw-time color attachment target contract，让 backend draw 在不重新实现 clear/load 语义的前提下，能够统一消费 render pass 和 dynamic rendering 的 active color attachment 0，并据此做正确的 write-back gate。

## Non-Goals

本设计**不**尝试在这一刀完成以下内容：

- depth / stencil attachment lifecycle
- resolve / MSAA attachment lifecycle
- 多 color attachments、input attachments、subpass local read
- 完整 resource state tracker / barrier / ownership 系统
- 把 attachment lifecycle 提升为完整 backend execution graph

本阶段只覆盖：

- `color attachment 0`
- single-sampled offscreen/write-back path
- render pass 与 dynamic rendering 两条 draw 入口

## Approaches Considered

### Option 1: Explicit draw-time color attachment target contract

在 `backend::GraphicsDrawCall` 中新增最小的 `GraphicsColorAttachmentTarget`，由 `CmdDrawBase::draw()` 在 draw 前从当前 render pass 或 dynamic rendering 状态提取：

- `vk::ImageView *imageView`
- `VkImageLayout layout`
- `VkAttachmentStoreOp storeOp`
- `bool present`

`TriangleBootstrapDraw` 只消费这份 contract，不再自己猜当前 attachment lifecycle 状态。

**Pros**
- 最小切口，层次正确
- render pass / dynamic rendering 可以共用同一 draw-time 输入
- 不把 command-buffer state 错塞到 pipeline-time metadata

**Cons**
- 这一刀仍然只覆盖一个 color attachment
- 更完整的 attachment tracker 还要后续扩展

### Option 2: Build a broader `ExecutionState` attachment tracker first

在 `VkCommandBuffer::ExecutionState` 里先建立完整 attachment lifecycle tracker，再让 backend draw 读取 tracker。

**Pros**
- 更接近长期 attachment/sync 模型

**Cons**
- 切口过大，容易把本轮工作膨胀成 command-buffer state 重构
- 当前 strict GPU bootstrap 只需要 attachment 0 的最小 contract，不需要完整 tracker

### Option 3: Push attachment metadata into `GraphicsExecutable`

把 attachment 状态作为 pipeline/executable metadata 暴露给 backend。

**Pros**
- 看起来统一

**Cons**
- 错层。`imageView` / `layout` / `storeOp` 属于 command-buffer state，不是 pipeline-time metadata
- dynamic rendering attachment state与 render pass attachment state都不是 `GraphicsExecutable` 应该承载的内容

## Decision

采用 **Option 1**。

先把 active color attachment 0 建模成 draw-time contract，再让 `TriangleBootstrapDraw` 基于这份 contract 做 write-back gate。clear 仍保留在 render pass / dynamic rendering begin 阶段执行；本轮只把“谁是当前 attachment target，以及当前 draw 是否允许 write-back”显式化。

## Design

### 1) New draw-time contract

在 `src/Backend/GraphicsDraw.hpp` 中新增一个最小的 color attachment target 结构，并把它按值挂到 `GraphicsDrawCall` 上：

- `vk::ImageView *imageView`
- `VkImageLayout layout`
- `VkAttachmentStoreOp storeOp`
- `bool present`

不在这个结构里重复存 format / sample count / row pitch，这些仍由 `vk::ImageView` 查询，避免 contract 重复缓存可派生状态。

### 2) Extraction in `CmdDrawBase::draw()`

`CmdDrawBase::draw()` 在发起 backend draw 之前，统一提取当前 active color attachment 0：

- **Render pass path**
  - 从 `executionState.renderPass->getSubpass(subpassIndex)` 读取 subpass color attachment 0 的 `VkAttachmentReference`
  - 从 `executionState.renderPassFramebuffer->getAttachment(attachmentIndex)` 读取 `vk::ImageView *`
  - 从 `executionState.renderPass->getAttachment(attachmentIndex)` 读取 `storeOp`
  - 从 `VkAttachmentReference.layout` 读取 draw-time layout

- **Dynamic rendering path**
  - 从 `executionState.dynamicRendering->getColorAttachment(0)` 读取 `imageView` / `storeOp` / `imageLayout`

如果 attachment 0 不存在、为 `VK_ATTACHMENT_UNUSED` 或 imageView 为空，则 `present = false`。

### 3) `TriangleBootstrapDraw` consumes the contract

`TriangleBootstrapDraw` 不再自己“从绑定结果里猜当前 attachment target”，而是只消费 `GraphicsDrawCall::colorAttachment0`。

在 render-to-attachment strict GPU 路径中，write-back gate 收敛为：

- `present == true`
- `imageView != nullptr`
- `storeOp == VK_ATTACHMENT_STORE_OP_STORE`
- `layout` 属于当前允许的 color-attachment write-back 布局
  - 第一刀先接受 `VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL`
  - 同时接受 `VK_IMAGE_LAYOUT_GENERAL`
- 现有的 image format / sample count / byte width 检查继续保留

如果 gate 不满足：

- fallback-allowed 路径：放弃 bootstrap write-back，继续走 CPU renderer
- strict/require 路径：给出明确的 unsupported / abort 原因

### 4) Clear/store/layout semantics in this phase

本设计刻意不重新实现 begin 阶段 clear：

- `loadOp == CLEAR` 继续由 `CmdBeginRenderPass` / `CmdBeginRendering` 执行
- `storeOp` 和 draw-time layout 则成为 backend write-back 的输入 gate

这样可以避免在 Phase 21 把“attachment lifecycle”误做成“重写 Vulkan render pass semantics”。本轮只把 draw-time target contract 明确化，并把 store/layout 约束从隐式假设变成显式条件。

### 5) Testing strategy

测试分两层：

1. **Backend/unit-level gate tests**
   - 给 `TriangleBootstrapDraw` 的 attachment target gate 增加纯逻辑覆盖
   - 锁住 `STORE` / `DONT_CARE`、允许布局 / 非允许布局这类边界

2. **Draw end-to-end tests**
   - 扩 `DrawTester`，允许配置 `colorStoreOp`
   - 新增两条 strict GPU death tests：
     - render pass + `storeOp = DONT_CARE`
     - dynamic rendering + `storeOp = DONT_CARE`
   - 这些 tests 在现状应为 RED，因为当前 ad-hoc write-back 忽略了 `storeOp`
   - 再补两条正向覆盖：
     - render pass + `storeOp = STORE`
     - dynamic rendering + `storeOp = STORE`

所有 draw-related cases 继续保存图片到 `draw-test-artifacts/`。

## Expected Outcome

完成后，strict GPU triangle bootstrap 仍然是最小 bring-up path，但 draw-time color write-back 将不再依赖 ad-hoc attachment probing。render pass 与 dynamic rendering 会首次拥有统一的 backend-owned color attachment target contract，这为后续更完整的 attachment lifecycle / resolve / depth-stencil 路径打下最小稳定接口。
