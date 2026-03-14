# Graphics Executable Texture Bootstrap Layout Validation Design

**Date:** 2026-03-12

**Goal**

把当前 texture-bootstrap metadata 的 narrow path 从“只看 shader”再收紧一刀：除了 shader 形状匹配，还要验证 descriptor layout 形状，避免 `GraphicsExecutable` 误接收 descriptor array 这类当前 draw helper 无法正确物化的资源绑定。

## Problem Statement

当前 `GraphicsExecutable` 的 texture-bootstrap metadata 提取条件是：

- fragment input `location 0 == vec2`
- shader 含有 texture sampling
- shader 的 descriptor decorations 收敛到单个 set / binding

这个判定仍然只基于 shader metadata，而没有看 pipeline layout。于是会留下一个 false-positive 类别：

- fragment shader 使用 `sampler2D texSampler[2]`
- SPIR-V decorations 仍然只会指向一个 set / binding
- 但对应的 descriptor layout `descriptorCount > 1`

当前 `TriangleBootstrapDraw` 的 texture 物化代码只会按单个 `SampledImageDescriptor` 读取 binding 起始位置，并没有表达 “descriptor array 中具体用的是哪个元素”。这意味着 descriptor array 不应被视为当前 narrow texture-bootstrap path 的已支持形态。

## Options Considered

### Option 1: 只加 negative tests，不改实现

- 优点：改动最小
- 缺点：如果 descriptor array 真的被误接收，测试会直接暴露真实 bug，但没有解决路径

### Option 2: 在 `GraphicsExecutable` create path 增加 pipeline-layout 校验

- 优点：
  - 改动小
  - 边界清晰
  - 和现有 pipeline-time metadata ownership 一致
- 缺点：
  - `GraphicsExecutableCreateInfo` 需要额外接一条 `PipelineLayout` 依赖

### Option 3: 把 texture resource plan 整体下沉进 executable

- 优点：长期方向正确
- 缺点：这一步范围过大，会把 draw-time binding state、descriptor materialization 和 executable design 一起耦合

## Chosen Design

采用 Option 2。

- `vk::GraphicsPipeline::compileShaders()` 在构建 `GraphicsExecutableCreateInfo` 时，把当前 `PipelineLayout` 传入
- `GraphicsExecutable::create(...)` 在提取 texture-bootstrap binding metadata 时，同时验证：
  - shader 形状满足当前 narrow path
  - layout 中该 set / binding 的 descriptor type 是 `COMBINED_IMAGE_SAMPLER`
  - layout 中该 binding 的 `descriptorCount == 1`
- 若 `descriptorCount != 1`，则不产出 texture-bootstrap binding metadata

## Why This Slice

- 可以直接消灭一个真实的 false-positive 类别
- 不改变 draw-time launch / write-back 逻辑
- 仍然保持 `GraphicsExecutable` 只持有 pipeline-time 可稳定确定的 metadata

## Testing

- 新增 Vulkan integration negative test：
  - fragment shader 使用 `sampler2D texSampler[2]`
  - descriptor set layout 的对应 binding `descriptorCount = 2`
  - 期望 `GraphicsExecutable` 不暴露 texture-bootstrap binding metadata
- 保留现有 positive combined-image-sampler test
- 保留已有 `location 0 != vec2` negative test

## Success Criteria

- descriptor array 不再被误判为当前 narrow texture-bootstrap path
- 单 descriptor combined-image-sampler positive case 继续通过
- 现有 draw/backend focused verification 保持绿色
