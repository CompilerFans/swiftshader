# Graphics Executable Texture Bootstrap Narrowing Design

**Date:** 2026-03-12

**Goal**

把当前 texture-bootstrap metadata 的适用边界收紧成一个明确、可验证的窄子集，避免 `GraphicsExecutable` 误判一些当前并不打算支持的纹理片元形态。

## Problem Statement

上一刀已经把 texture-bootstrap 的 descriptor set / binding metadata 迁进了 `GraphicsExecutable`，但当前判定还偏宽：

- 只要 location 0 输入分量数 `>= 2`
- 有 texture sampling
- descriptor decorations 收敛到单个 set / binding

这会把一些当前 narrow path 不打算承担的 shader 也纳入，例如：

- location 0 是 `vec3 color`
- location 1 才是 `vec2 texCoord`
- fragment shader 实际用的是 location 1 去 sample texture

现有 bootstrap 并没有真正做 varyings 级别的语义映射，因此这类 shader 不应该被视为“已支持的 texture bootstrap shader”。

## Chosen Design

- 当前 narrow texture-bootstrap metadata 进一步收紧为：
  - fragment input location 0 必须是 **exactly `vec2`**
  - 存在 texture sampling 指令
  - descriptor decorations 收敛到单个 set / binding
- 对不满足该条件的 texture fragment shader，`GraphicsExecutable` 不产出 texture bootstrap binding metadata。

## Why This Slice

- 改动非常小
- 行为边界更清楚
- 直接避免 future false-positive metadata
- 不引入新的 draw-time 或 Vulkan runtime 依赖

## Testing

- 新增 Vulkan integration negative test：
  - location 0 是 color
  - location 1 才是 texcoord
  - fragment shader sample texture using location 1
  - 期望 `GraphicsExecutable` **不**暴露 texture bootstrap binding metadata

## Success Criteria

- `GraphicsExecutable` 不再把“非 location0-vec2 texcoord” shader 认成当前 narrow texture-bootstrap path
- 现有 combined-image-sampler positive case 继续通过
