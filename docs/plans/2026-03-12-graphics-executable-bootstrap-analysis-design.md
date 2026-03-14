# Graphics Executable Bootstrap Analysis Design

**Date:** 2026-03-12

**Goal**

把 triangle bootstrap 中“只依赖 shader 本身”的静态分析从 draw 时搬到 `backend::GraphicsExecutable`。本轮只迁移 pipeline-time 可确定的信息，例如 vertex constant point size 和非 descriptor 依赖的 fragment bootstrap 模板；texture descriptor 物化仍留在 draw helper。

## Problem Statement

虽然上一刀已经给 graphics pipeline 接上了 `GraphicsExecutable`，但它当前只保存 stage entry point 和 vertex lowering。真实的 triangle bootstrap 仍然在 draw 时直接挖 `SpirvShader`：

1. vertex point size 常量仍在 draw 时扫描 SPIR-V；
2. fragment bootstrap mode（constant color、FragCoord、FrontFacing、PointCoord、flat/smooth color）仍在 draw 时分类；
3. `GraphicsExecutable` 还没有开始承担真正可复用的 pipeline-time graphics 分析结果。

这使它仍然像一个挂点，而不像后续正式 graphics execution 的宿主。

## Constraints

- 不改 draw route，不改变 CPU/GPU fallback 语义。
- 不改变当前 triangle bootstrap 实际渲染结果。
- 不在本轮把 texture descriptor 数据塞进 `GraphicsExecutable`，因为那部分依赖 draw-time descriptor state。

## Approaches

### Option A: 保持分析都在 `TriangleBootstrapDraw.cpp`

**优点**
- 改动最少。

**缺点**
- `GraphicsExecutable` 继续空心化；
- pipeline-time 与 draw-time 职责边界不清；
- 后续正式 graphics executable 仍要再拆一次。

**结论**
- 不采用。

### Option B: 把静态 shader bootstrap 分析迁到 `GraphicsExecutable`

**做法**
- `GraphicsExecutable` 在创建时接受可选的 `SpirvShader` 指针
- 在 pipeline 创建期完成：
  - vertex constant point size 提取
  - 非 texture 的 fragment bootstrap 模板分类
- `TriangleBootstrapDraw` 只消费 executable 提供的模板；texture descriptor 物化继续留在 helper

**优点**
- 把真正稳定的 shader 分析固定在 pipeline-time
- draw helper 变轻，减少直接依赖 `SpirvShader`
- 是向“graphics executable 驱动 draw”演进的自然一步

**缺点**
- `GraphicsExecutable` 需要开始感知 bootstrap-specific metadata

**结论**
- 采用。

### Option C: 直接让 `GraphicsExecutable` 承担完整 texture bootstrap 物化

**优点**
- 更接近最终 graphics execution object

**缺点**
- descriptor/image/sampler 是 draw-time 绑定态，不适合在本轮强行塞进 pipeline object

**结论**
- 暂不采用。

## Chosen Design

### 1. Extend GraphicsExecutable metadata

`backend::GraphicsExecutable` 新增 bootstrap 相关只读 metadata：

- `bootstrapPointSize()`
- `hasBootstrapFragmentConfig()`
- `bootstrapFragmentConfig()`

其中：

- point size 默认仍是 `64.0f`，只有在 vertex shader 明确写常量 `gl_PointSize` 时才覆盖
- fragment bootstrap config 只覆盖 shader-only 可确定的模式：
  - `ConstantColor`
  - `FragCoordQuadrants`
  - `FragCoordDiscardLeftConstantColor`
  - `PointCoordGradient`
  - `FrontFacingBinaryColors`
  - `FlatInterpolatedColor`
  - `InterpolatedColor`
  - `InterpolatedColorBlueNearFragDepth`

### 2. Creation API

保留现有 `create(vertexModule, fragmentModule)` 用法，并新增更完整的 create-info 入口，使 pipeline 创建期可以把 `SpirvShader` 指针一起传入做 bootstrap 分析。

### 3. Triangle bootstrap helper cleanup

`TriangleBootstrapDraw` 不再自己分析：

- vertex constant point size
- 非 texture fragment bootstrap kind

它只做：

- 从 executable 读取 bootstrap metadata
- 在 texture shader 场景下，根据 draw-time descriptor/sampler state 物化 `Texture2DColor` 配置
- 启动现有 bootstrap path

## Testing Strategy

### Backend unit tests

- `GraphicsExecutable` 能从真实 `SpirvShader` 提取 constant point size
- `GraphicsExecutable` 能从真实 fragment shader 提取 constant color bootstrap 模板
- 无 fragment shader 时不会伪造 bootstrap fragment config

### Focused regression

- `DrawTest.SolidColorTriangle` 继续通过
- graphics backend Vulkan smoke 继续通过

## Success Criteria

- `GraphicsExecutable` 不再只是 entry-point container
- `TriangleBootstrapDraw` 不再直接分析非 texture 的 fragment bootstrap mode 或 vertex point size
- focused backend/draw/Vulkan tests 通过
