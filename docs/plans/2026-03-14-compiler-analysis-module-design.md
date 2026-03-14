# Compiler Analysis Module Extraction Design

**Date:** 2026-03-14

## Context

当前仓库里已经有一条逐步成形的 graphics compiler 路线，但“编译器分析功能”还没有独立模块边界：

- `SemanticIR` 目前仍然很薄，只保存 `stage`、`entryPoint` 和最小 `vertexLowering` 信息。
- 与当前 graphics 路线真正相关的 shader/compiler 分析，大多散落在 `src/Backend/GraphicsExecutable.cpp` 中：
  - fragment feature mask
  - texture plan
  - image resource plan
  - general resource plan
  - triangle-bootstrap unsupported reasons

这带来三个问题：

1. **错层**：通用编译器分析被绑定在 backend executable object 上。
2. **难测**：当前测试大多经由 `GraphicsExecutable` 或 Vulkan pipeline introspection 间接验证，无法作为独立 compiler unit tests 分层扩展。
3. **难演进**：若后续需要扩 `KernelIR`、emitter 或 IR-based codegen，现有分析逻辑没有稳定的独立输入/输出契约。

## Goal

把当前 graphics 路线中通用的 shader/compiler analysis 从 `GraphicsExecutable` 中提取成 `src/Pipeline/` 下的独立模块，并为其建立分层单测基础：

- 第一层：`SpirvShader -> CompilerAnalysis`
- 第二层：`CompilerAnalysis -> KernelIR`
- 第三层：`Emitter / ABI parity`

## Non-Goals

本轮**不**做以下内容：

- 完整重写 `SemanticIR`
- 把所有 bootstrap-specific 逻辑一次性并入 compiler 层
- 完整实现复杂 fragment lowering
- 在同一刀里做完所有 `KernelIR` / emitter 扩展

本轮重点是“模块独立化 + 首批分层测试契约”，而不是一次性做完全部 compiler pipeline 演进。

## Approaches Considered

### Option A: Extract only general compiler analysis first

把以下内容抽成独立模块：

- fragment feature mask
- texture/resource/image plan
- unsupported reason mask

保留 bootstrap-specific 内容在现有 graphics/backend adapter 中：

- constant point size bootstrap inference
- static bootstrap fragment template inference

**Pros**
- 边界清晰，通用 compiler functionality 与 bring-up strategy 解耦
- 改动面可控
- 更适合先建立单元测试矩阵

**Cons**
- 仍然会有一部分 analysis 留在 `GraphicsExecutable`
- 后续还要做第二轮边界收缩

### Option B: Extract both general analysis and bootstrap-specific analysis together

把 `GraphicsExecutable.cpp` 里的所有 shader analysis 都一起抽到 compiler 层。

**Pros**
- 一次性收口
- `GraphicsExecutable` 会变得很薄

**Cons**
- 模块边界容易被当前 bootstrap 过渡逻辑污染
- 会把 bring-up-specific policy 和 compiler core semantics 混在一起
- 第一刀风险过大

### Option C: Leave analysis in backend and only add tests

不做模块提取，只补测试。

**Pros**
- 改动最小

**Cons**
- 继续维持错层
- 之后做 IR/codegen 演进时还得再拆一次

## Decision

采用 **Option A**。

先把**通用 compiler analysis**独立成新模块，再让 `GraphicsExecutable` 消费其结果。bootstrap-specific 分析先保留在 backend/graphics adapter 层，等 compiler 核心模块稳定后再决定哪些值得继续下沉。

## Design

### 1) New module: `ShaderCompilerAnalysis`

新增：

- `src/Pipeline/ShaderCompilerAnalysis.hpp`
- `src/Pipeline/ShaderCompilerAnalysis.cpp`

模块职责：

- 接收 `sw::SpirvShader`
- 接收最小 layout/query 上下文
- 产出稳定的 analysis result object

模块**不得**依赖：

- `GraphicsExecutable`
- `TriangleBootstrapDraw`
- `Renderer`
- Vulkan command-buffer state

这样它才能作为真正的 compiler module 被独立测试和重用。

### 2) Context API

建议新增上下文类型：

- `ShaderCompilerAnalysisContext`

包含：

- descriptor binding query callback
- callback userdata
- `descriptorSetCount`
- `dynamicOffsetCount`
- `pushConstantSize`

这样模块只依赖“编译器分析所需的最小外部信息”，而不是直接依赖 `GraphicsExecutableCreateInfo`。

### 3) Result API

建议新增：

- `ShaderCompilerAnalysisResult`

包含：

- `ShaderTexturePlan texturePlan`
- `ShaderImageResourcePlan imageResourcePlan`
- `ShaderResourcePlan resourcePlan`
- `uint32_t fragmentFeatureMask`
- `uint32_t unsupportedReasonMask`

类型命名从 backend-specific 名字转成中性 compiler 名字：

- `GraphicsExecutableDescriptorRef` -> `ShaderDescriptorRef`
- `GraphicsExecutableImageResourcePlan` -> `ShaderImageResourcePlan`
- `GraphicsExecutableResourcePlan` -> `ShaderResourcePlan`
- `GraphicsExecutableTexturePlan` -> `ShaderTexturePlan`
- `GraphicsExecutableFragmentFeature` -> `ShaderFragmentFeature`
- `GraphicsExecutableTriangleBootstrapUnsupportedReason` -> `ShaderUnsupportedReason`

### 4) Analysis entry points

建议第一刀只做一个主入口：

- `analyzeGraphicsFragmentShader(const sw::SpirvShader &, const ShaderCompilerAnalysisContext &)`

返回完整 `ShaderCompilerAnalysisResult`。

可选再加：

- `analyzeGraphicsVertexShader(...)`

但第一刀不要求它承载 point-size/bootstrap-specialized 分析。当前 vertex 相关 bootstrap 推断仍保留在 backend。

### 5) What moves in this phase

从 `GraphicsExecutable.cpp` 提取到新模块：

- `fragmentFeatureMaskForShader(...)`
- sampled descriptor collection
- storage image descriptor collection
- texture plan construction
- image resource plan construction
- general resource plan construction
- unsupported reason mask construction

### 6) What stays in backend for now

继续保留在 `GraphicsExecutable.cpp` / graphics adapter：

- `tryGetBootstrapVertexPointSizeConstant(...)`
- `tryBuildStaticBootstrapFragmentConfig(...)`
- 任何只服务于当前 triangle bootstrap template 的专用推断

原因：这些逻辑当前更像 bring-up strategy，而不是通用 compiler capability analysis。

### 7) `GraphicsExecutable` integration

`GraphicsExecutable` 不再自己拼 analysis 逻辑，而是：

1. 组装 `ShaderCompilerAnalysisContext`
2. 调用新模块
3. 保存 analysis result 中仍需要对外暴露的字段
4. 保留 bootstrap-specific 字段的组装

最终形态是：

- `GraphicsExecutable` 变成 consumer
- `ShaderCompilerAnalysis` 变成 producer

### 8) Layered test strategy

#### Layer 1: `SpirvToCompilerAnalysis`

新增独立单测文件，例如：

- `tests/BackendUnitTests/SpirvToCompilerAnalysisTests.cpp`

这是首批主覆盖层，直接断言 compiler analysis result。

推荐覆盖矩阵：

- **Supported**
  - constant color fragment
  - interpolated color
  - combined image sampler
  - separate image/sampler
  - descriptor array constant index
- **Gated / unsupported**
  - storage image read/write
  - discard
  - image query/fetch
  - derivatives
  - atomics
  - subgroup
  - buffer descriptors present
  - non-constant descriptor array element

#### Layer 2: `CompilerAnalysis -> KernelIR`

扩 `KernelIR` tests，验证分析结果的关键 metadata 不会在 lowering 中丢失。

第一刀不要求做完整 lowering；至少要保证：

- feature mask 可透传
- unsupported reason mask 可透传
- resource/texture presence metadata 可继续消费

#### Layer 3: `Emitter / ABI parity`

扩 `CodegenEmitterTests.cpp` / `AbiParityTests.cpp`。

这里不重复验证语义，而验证：

- 同一份 compiler-derived IR 输入
- CUDA-like source emitter 与 LLVM IR emitter
- 其 normalized ABI / capability header 不发生漂移

## Expected Outcome

完成后，graphics 路线的通用 shader/compiler analysis 将首次拥有独立模块边界。`GraphicsExecutable` 会从“analysis 宿主”降级为“analysis 消费者”，测试也会从 Vulkan-side 间接验证扩展为真正的 compiler-layer 分层验证。这为后续扩 `SemanticIR`、`KernelIR` 和 IR-based codegen 提供更稳的中间落脚点。
