# Graphics Executable Texture Bootstrap Design

**Date:** 2026-03-12

**Goal**

把 triangle bootstrap 中剩余的 texture-shader 元数据分析也迁进 `backend::GraphicsExecutable`，让 `TriangleBootstrapDraw` 不再直接读取 fragment `SpirvShader`。

## Problem Statement

上一刀已经把以下 shader-only 分析迁进了 `GraphicsExecutable`：

- vertex constant `gl_PointSize`
- 非 texture 的 fragment bootstrap 模板

但 texture bootstrap 仍然在 draw 时扫描 fragment shader：

- 是否存在 texture sampling
- 是否满足 location 0 的 `vec2` texcoord 输入
- descriptor set / binding 在哪

这意味着 `TriangleBootstrapDraw` 仍然直接依赖 fragment `SpirvShader`，还没有真正只消费 executable metadata。

## Constraints

- 不改变当前 GPU/CPU fallback 语义。
- 不把 descriptor/image/sampler 的实际物化迁进 `GraphicsExecutable`。
- 不改变现有 narrow texture bootstrap 的 runtime 行为。

## Approaches

### Option A: 保持 texture metadata 分析留在 draw helper

**优点**
- 代码改动最少。

**缺点**
- `TriangleBootstrapDraw` 继续依赖 `SpirvShader`
- `GraphicsExecutable` 仍然不能完整承接 fragment bootstrap 的 pipeline-time metadata

**结论**
- 不采用。

### Option B: `GraphicsExecutable` 记录 texture bootstrap binding metadata

**做法**
- pipeline 创建期识别 narrow texture bootstrap shader
- 记录 descriptor set / binding
- draw 时只用 executable metadata 去 descriptor sets 里取出纹理和 sampler 状态

**优点**
- `TriangleBootstrapDraw` 不再直接读取 fragment shader
- descriptor 物化仍保留在 draw-time，职责边界清晰
- 是向正式 graphics executable path 演进的自然一步

**缺点**
- 需要新增一小段 texture-specific metadata API

**结论**
- 采用。

### Option C: 直接把 texture data 和 sampler state 塞进 `GraphicsExecutable`

**优点**
- 更接近最终执行对象

**缺点**
- descriptor/image/sampler 是绑定态，不适合在 pipeline object 上固化

**结论**
- 暂不采用。

## Chosen Design

### 1. Extend GraphicsExecutable texture metadata

`GraphicsExecutable` 新增只读 texture bootstrap metadata：

- `hasBootstrapTextureBinding()`
- `bootstrapTextureBinding()`

该 metadata 只表示：

- shader 使用了 narrow texture bootstrap 可支持的采样形态
- 对应 descriptor set / binding

它**不**持有实际 texture bytes 或 sampler state。

### 2. Keep draw-time materialization in TriangleBootstrapDraw

`TriangleBootstrapDraw` 继续负责：

- descriptor set / pipeline layout 验证
- `SampledImageDescriptor` 解包
- sampler state 验证
- `FragmentBootstrapConfig::Texture2DColor` 的实际填充

但它不再自己读 fragment `SpirvShader`。

### 3. Testing

- backend unit test 只锁住新的默认 metadata API
- Vulkan integration test 用真实 graphics pipeline 验证：
  - combined image sampler shader 会产出 texture bootstrap binding metadata
  - metadata 中的 descriptor set / binding 符合 shader 声明

## Success Criteria

- `TriangleBootstrapDraw` 不再直接包含或读取 fragment `SpirvShader`
- texture bootstrap binding metadata 在 pipeline-time 建立
- focused backend / Vulkan / draw 验证继续通过
