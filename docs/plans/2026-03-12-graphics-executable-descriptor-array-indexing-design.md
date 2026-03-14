# Design: Descriptor-Array Indexing In Texture Bootstrap Plan

## Goal
Support a wider sampled-image resource surface for the current narrow `Texture2DColor` bootstrap by handling descriptor arrays when the array element index is a constant.

## Non-Goals
- Supporting non-constant (dynamic / non-uniform) descriptor indexing in bootstrap.
- Supporting multiple sampled-image resources or post-processed sampled color in bootstrap.
- Replacing the full CPU renderer draw path.

## Problem
SwiftShader's SPIR-V `descriptorDecorations` propagate `DescriptorSet`/`Binding`, but do not encode descriptor-array element selection. When a shader samples `sampler2D tex[2]; texture(tex[1], ...)`, the decorations still point to the same set/binding, but draw-time materialization must pick element `1`.

Before this slice, the bootstrap path treated `descriptorCount > 1` as unsupported to avoid silently materializing element `0`.

## Approach
1. Extend the sampled-image plan to explicitly carry the selected descriptor-array element:
   - `GraphicsExecutableTexturePlan::imageArrayElement`
   - `GraphicsExecutableTexturePlan::samplerArrayElement`
2. Extract constant array indices from the SPIR-V value chain feeding texture sampling:
   - Walk backwards from the sampled-image operand through trivial forwarding (`OpLoad`, `OpCopyObject`, `OpCopyLogical`).
   - Detect `OpAccessChain` / `OpInBoundsAccessChain` / `OpPtrAccessChain` and read the constant index operand.
   - If a non-constant index is observed, mark bootstrap as unsupported for that plan.
3. Materialize descriptor-array elements at draw time:
   - Resolve the descriptor pointer using `bindingOffset + arrayElement * descriptorSize`.
   - Keep the existing layout checks (descriptor type) and add bounds checks against `descriptorCount`.

## Bootstrap Eligibility
For `CombinedImageSampler`:
- fragment input `location 0` is exactly `vec2`
- fragment output `location 0` resolves to a supported texture sample via trivial pass-through
- binding type is `COMBINED_IMAGE_SAMPLER`
- descriptor-array element index is constant (or absent, treated as `0`) and `arrayElement < descriptorCount`

For `SeparateImageSampler`:
- same shader constraints as above
- binding types are one `SAMPLED_IMAGE` plus one `SAMPLER`
- each element index is constant (or absent, treated as `0`) and in-bounds for its `descriptorCount`

## Testing
- Vulkan pipeline introspection exposes `imageArrayElement` / `samplerArrayElement`.
- `GraphicsBackendPipeline` adds:
  - descriptor array index `0` positive case
  - descriptor array index `1` positive case
- `DrawTest` adds a strict bootstrap render regression that forces the triangle bootstrap path to be the final writeback and validates the selected descriptor-array element.

---

# 设计：Texture Bootstrap Plan 的 Descriptor-Array Index 支持

## 目标
在当前窄 `Texture2DColor` bootstrap 语义下，支持 descriptor array 的常量 index，从而扩大 sampled-image 资源支持面。

## 非目标
- bootstrap 下支持 non-constant / non-uniform descriptor indexing。
- bootstrap 下支持多纹理组合或 sample 后的额外片元运算。
- 替代 CPU renderer 的真实 draw 执行路径。

## 问题
SPIR-V 的 `descriptorDecorations` 只携带 `DescriptorSet`/`Binding`，不携带 descriptor-array element。shader 写 `sampler2D tex[2]; texture(tex[1], ...)` 时，如果 draw-time 仍按 element `0` 物化，就会产生静默错误。

因此在没有 element metadata 之前，`descriptorCount > 1` 被保守视为 unsupported。

## 方案
1. 扩展 sampled-image plan，显式携带 element：
   - `GraphicsExecutableTexturePlan::imageArrayElement`
   - `GraphicsExecutableTexturePlan::samplerArrayElement`
2. 从 texture sample operand 的 provenance 回溯常量 index：
   - 允许 `OpLoad`/`OpCopyObject`/`OpCopyLogical` 的 trivial 转发
   - 命中 `OpAccessChain` / `OpInBoundsAccessChain` / `OpPtrAccessChain` 时读取常量 index
   - 若观察到 non-constant index，则该 plan 的 bootstrap 标记为 unsupported
3. draw-time 物化 descriptor element：
   - 按 `bindingOffset + arrayElement * descriptorSize` 取出 descriptor
   - 增加对 `descriptorCount` 的 bounds check

## Bootstrap 资格
`CombinedImageSampler`：
- fragment `location 0 == vec2`
- `location 0` 输出可通过 trivial passthrough 解析到支持的 texture sample
- layout binding type 为 `COMBINED_IMAGE_SAMPLER`
- array element 为常量（缺失视为 `0`），且 `arrayElement < descriptorCount`

`SeparateImageSampler`：
- 同上 shader 约束
- layout 形态为 `SAMPLED_IMAGE` + `SAMPLER`
- 两个 element index 都为常量（缺失视为 `0`），且都在各自 `descriptorCount` 范围内

## 测试
- pipeline introspection 暴露 `imageArrayElement` / `samplerArrayElement`
- `GraphicsBackendPipeline` 覆盖 index `0` / index `1` 两个正例
- `DrawTest` 新增 strict bootstrap render 回归，强制 triangle bootstrap 成为最终 writeback，并验证 descriptor-array element 选择正确
