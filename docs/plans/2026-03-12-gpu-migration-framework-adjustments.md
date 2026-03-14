# GPU Migration Framework Adjustments

**Date:** 2026-03-12

## Context

当前 graphics GPU bring-up 仍以 triangle bootstrap 为主：pipeline-time 侧的 `GraphicsExecutable` 已逐步吸收 shader-only metadata，并新增 image resource plan（sampled + storage），用于后续更一般资源建模；draw-time 仍主要由 `TriangleBootstrapDraw` 物化绑定并驱动 CUDA kernel。此阶段的核心目标是：在不牺牲 correctness 的前提下，把 Vulkan graphics 执行从 CPU renderer 逐步迁入 backend-owned execution。

截至 2026-03-12，本阶段也已经补齐了两块关键脚手架：
- `GraphicsExecutableResourcePlan`：把 layout 轮廓（set 数、dynamic offset 数、push constant size 占位）和 descriptor refs（含 array element / dynamic offset index）前置到 pipeline-time，使后续资源建模不必再回到 draw-time 扫 SPIR-V。
- fragment feature mask：把 discard/storage-image/image-query/derivatives/atomics/subgroup 等 fragment 能力点提取成可测试的 bitmask，作为后续统一 capability/side-effect gate 的输入。

本文件把“继续走向整体 GPU 方案”在框架实现上还需要迁移/调整的面向更大支持面的结论落盘，避免只沿单个 bootstrap happy-path 单点收敛。

## Conclusions: What Still Needs Migration/Adjustment

### 1) Resource model: converge on a full descriptor + constants plan

当前已存在最小可用的 `GraphicsExecutableResourcePlan`（先建模/暴露，不强行执行），但要面向“整体 GPU 执行”仍需继续补齐：
- Descriptor families: `UBO/SSBO`、`(uniform|storage) texel buffer`、`storage image`、`input attachment`、`sampler`、`combined image sampler`、acceleration structure（若未来需要）都需要在 plan 中有一致表示，并能被资源物化与 ABI 消费。
- Constants: push constants（实际使用范围而不是 max size 占位）、specialization constants、dynamic offsets（已建模计数与索引，但仍需贯穿到 draw/dispatch ABI）、robustness sizing（buffer/image）。
- Binding rules: descriptorCount、partially bound、immutable sampler、descriptor indexing features（尤其 non-uniform/dynamic index 与 Vulkan feature 约束）。

关键点是把“资源形态”和“是否可走某条执行路径（bootstrap/GPU/CPU fallback）”解耦：plan 负责描述资源使用与索引形态，执行路径只根据 plan + capability gate 作出可解释决策。

### 2) Indexing semantics as first-class metadata (constant/dynamic/non-uniform)

整体 GPU 方案必须把索引形态纳入 plan 并贯穿 ABI：
- 常量索引：可在 pipeline-time 固化 array element / offset。
- 动态索引：需要运行时参数、bounds 策略与 non-uniform 处理（以及对应的 Vulkan feature requirements）。
- `NonUniform`：会影响 backend 选择、lane packing、以及可能的 divergence 策略。

没有这层抽象，后续扩 descriptor indexing 时会反复出现“单点加 patch + 大量 false-positive/false-negative”。

### 3) Shader capability + side-effect gate: correctness-first routing

当前已对 fragment `OpImageRead/OpImageWrite` 做了 bootstrap 禁止，整体需要把 capability gate 系统化：
- Side effects: image/buffer writes、atomics、discard/demote、sample mask writes、memory barriers。
- Quad invariants: derivatives/LOD/helper lane 语义要求 2x2 quad 同步调度；helper lane 必须参与导数/采样但不得提交 side-effect。
- 采样相关扩展：`texelFetch`/`textureSize`/lod/query 等是否能被当前路径正确表达，需要显式 gate + 回退理由（避免 silent wrong）。

输出应包含可测试/可日志化的 “unsupported reason list”，而不是仅一个布尔值。

### 4) RenderPass/attachments semantics: make attachment lifecycle explicit

从“只写一个 color attachment buffer”迁到完整 Vulkan attachments 语义，需要统一 attachment 生命周期与状态跟踪：
- load/store/clear、layout transitions、subpass dependencies、resolve（MSAA）、depth/stencil。
- input attachment 的读取语义（与 sampled/storage 区分清晰）。
- dynamic rendering 与传统 render pass 两条路径的统一 lowering。

否则 barrier/layout/ownership 无法自然落到 backend。

### 5) Fixed-function and dynamic state plumbing: variant strategy

dynamic state / fixed-function state 必须以一致方式进入 backend owned execution：
- viewport/scissor、blend/depth-stencil、cull/frontface、depth bias、sample count/mask、rasterization state 等。
- 明确哪些状态作为 kernel 参数，哪些触发 pipeline variant（并定义 cache key）。

若不先定策略，后续会出现“CPU/Vulkan 状态变化但 GPU kernel 仍按旧状态执行”的隐性 bug。

### 6) Geometry/VS/raster/FS pipeline: graduate from single-triangle bootstrap

triangle bootstrap 是 bring-up 工具，不是最终形态。整体 GPU graphics 需要按阶段定义 ABI 和数据流：
- vertex fetch（含 index/instance/format/stride）、primitive setup、clip/cull、raster、varying interpolation、fragment、output merge（depth/stencil/blend）。
- 明确每阶段输入输出布局、tile/quad 调度与缓存策略。

### 7) Memory + synchronization: backend-owned hazards and queue semantics

整体 GPU 方案最终必须把 Vulkan 的可见性/顺序在 backend 落地：
- resource state tracking：buffer/image read/write hazard、barrier、layout、queue family ownership。
- submit：semaphore/fence（含 timeline）等待/信号，host sync（flush/invalidate）与映射内存一致性。
- upload/download：staging path、draw-test 读回（截图/验证）路径。

这部分一旦延后太久，会成为 WSI/present、readback、以及 conformance 的硬阻塞。

### 8) Command buffer ownership and execution scheduling

需要明确 `vk::CommandBuffer` 到 backend 的边界与“可走 GPU / 必须回退”的决策点：
- 决策尽量前置到 pipeline/executable 构建期（plan + gate）。
- draw-time 仅做少量资源物化与参数填充，不应承担大量 shader 解析或编译。
- 回退边界：按 draw、按 pass、按 command buffer 需要一致规则，并可通过测试锁住。

### 9) Observability and tests: broaden coverage, keep artifacts

建议把 pipeline introspection 从 image 扩到 buffers/push constants/dynamic offsets/索引形态，形成“可验证的后端契约”。

Draw-related regression 新增或扩展时默认 dump 到 `draw-test-artifacts/`，并在 strict GPU 模式下对“回退/拒绝”给出明确原因与图像/日志证据。

已知风险：在某些 image query/fetch 管线（如 `texelFetch`/`textureSize`）方向上，vk-unittests 曾出现初始化阶段不稳定（需单独定位，避免成为扩大支持面的阻塞点）。

## Practical Next Milestones (Suggested)

1. 把 `GraphicsExecutableImageResourcePlan` 上升为更一般的 `GraphicsExecutableResourcePlan`（先做“提取 + 暴露 + gate”，不急着全支持）。
2. 建立统一的 capability/side-effect gate（输出可测试的 reason list），并把 gate 决策前置到 pipeline/executable 创建期。
3. 引入 attachment lifecycle/state tracking 的最小闭环（先覆盖 offscreen color + layout + clear/store），为后续 depth/stencil/resolve 打基础。
4. 把 “编译/模块生成” 从 draw-time 迁到 pipeline-time，并引入缓存（避免每 draw spawn compiler）。

## References

- `src/Backend/GraphicsExecutable.*` (image resource plan, bootstrap gating)
- `src/Backend/TriangleBootstrapDraw.*` (draw-time materialization)
- `tests/VulkanUnitTests/PipelineIntrospection.*` (pipeline plan visibility in tests)
