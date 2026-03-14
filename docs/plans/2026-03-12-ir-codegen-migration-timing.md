# IR-Based Codegen Migration Timing & Feasibility

**Date:** 2026-03-12

## Problem Statement

当前 GPU bring-up 的 module 构建仍主要依赖“生成 CUDA-like 文本源码 -> 交给 `nvcc` 解析/编译 -> 加载 fatbin”的路径：
- `backend::CudaCompilerDriver` 会把源码写到 `/tmp/.../module.cu` 并 `posix_spawnp("nvcc", ...)` 生成 `module.fatbin`。
- `backend::CudaRuntimeAPI::createModule()` 依赖上述编译产物并通过 Driver API `cuModuleLoad()` 加载。

这条路径对 bring-up 友好（可读/可 dump），但随着 shader 覆盖扩大，会在以下方面变成瓶颈或风险：
- **性能**：spawn 外部编译器 + C++ 前端解析成本高，且目前调用点容易落到 draw-time。
- **可移植性**：要求系统存在 `nvcc`；对非 NVIDIA/非 CUDA 环境不可用。
- **工程可控性**：编译缓存、错误定位、ABI 稳定性、以及多后端一致性更难系统化。

因此需要评估：何时、以及是否，把“文本解析和生成”的路径迁移成“基于 IR 的编译/生成”。

## What “IR-Based” Means Here

这件事有两个维度，必须拆开看：

1. **前端内部表示**（SwiftShader 内部 IR）：  
   `SpirvShader/SpirvBinary -> SemanticIR -> KernelIR -> codegen`  
   目标是把资源模型、quad/helper-lane 语义、以及 ABI 变成可验证的结构化数据，避免 backend 直接扫 SPIR-V 做 ad-hoc 决策。

2. **后端编译器输入**（外部/运行时 toolchain 的输入形态）：  
   由 CUDA-like 高级语言文本，迁到更低层的 IR（例如 LLVM IR/bitcode，或更贴近目标的中间形式）。

“只把文本从 CUDA C++ 换成 textual LLVM IR”仍然属于“文本解析”，收益有限；真正的核心是：前端拥有结构化 IR，并能直接把该 IR lower 到后端接受的中间产物（尽量减少高层语言解析与不确定性）。

## Current Code State (as of 2026-03-12)

仓库已经具备 IR 迁移的若干脚手架：
- `src/Pipeline/SemanticIR.*` / `KernelIR.*` / `KernelABI.*`：存在最小骨架，但语义覆盖仍很窄。
- `src/Pipeline/CodegenTarget.hpp`：枚举 `CudaLikeSource` 与 `LlvmIR`。
- `src/Pipeline/CudaLikeSourceEmitter.*`：从 `KernelIRModule` 生成 CUDA-like 源码（当前主要覆盖 VS lowering 的极小子集）。
- `src/Pipeline/LlvmIREmitter.*`：目前是 placeholder 文本。
- `backend::RuntimeAPI::createModule(const std::string &sourceOrIR, ...)` 已预留 “sourceOrIR” 入口，但运行时仍按源码走 `nvcc`。

这说明“内部 IR”方向已经开了口，但“后端真正消费 IR”仍未落地。

## Feasibility Options

### Option A: Keep nvcc + CUDA-like source (status quo), but fix caching/placement

**做法**
- 继续生成 CUDA-like 源码，交给 `nvcc --fatbin`。
- 把编译从 draw-time 收敛到 pipeline-time，加入 module cache（key 可为 source hash + arch + entryPoint）。
- 允许在内存中加载（后续可考虑 `cuModuleLoadDataEx`），减少临时文件开销。

**当前进展（2026-03-12）**
- 已在 `backend::CudaRuntimeAPI::createModule()` 引入进程内 module cache，避免同一份 source/entryPoint 被重复触发 `nvcc` 编译。
  - 默认开启；设置 `SWIFTSHADER_CUDA_DISABLE_MODULE_CACHE=1` 可关闭（用于调试 toolchain，但会非常慢）。
- fragment bootstrap 的 `ConstantColor` 生成已改为通过参数传递颜色值，避免把颜色常量 baked 到源码导致 cache 过度碎片化。

**优点**
- bring-up 最稳，调试最直接（源码可读、nvcc 错误信息成熟）。
- 不引入大型依赖或新 toolchain。

**缺点**
- 仍依赖高层语言解析 + 外部进程，性能与可移植性问题本质没解决。

**结论**
- 作为短期（仍在 bootstrap/窄路径阶段）最可行的方案；但必须先做缓存与编译时机迁移，否则后续扩面会被编译成本拖死。

### Option B: Still text, but in-process compilation (NVRTC / driver JIT)

**做法**
- 仍生成 CUDA-like 源码，但用 NVRTC 或类似机制在进程内编译到 PTX/cubin，并用 `cuModuleLoadDataEx` 加载。

**优点**
- 去掉 spawn `nvcc`，减少 `/tmp` I/O 和进程开销，便于缓存与并发控制。

**缺点**
- 仍依赖 CUDA C++ 前端解析；并且 NVRTC 的可用性、版本与特性覆盖需要额外评估。

**结论**
- 如果短期性能/稳定性被 `nvcc` spawn 明显拖累，这是低风险的中间过渡；但它不是“IR-based”的终局方案。

### Option C: True IR-based backend input (LLVM IR / bitcode / custom IR)

**做法**
- 扩充 `SemanticIR/KernelIR` 直到能表达所需 shader 子集，并为其定义稳定 ABI + 资源模型。
- 让 runtime 或外部 toolchain 直接消费 IR（例如 LLVM bitcode），产出目标可加载的二进制。

**优点**
- 避免高层语言解析，编译链更可控、更适合多后端。
- 更利于在 IR 层做分析（capability gate、资源 plan、quad/helper-lane 约束）与一致性验证。

**缺点**
- 工程量大：需要完善 IR 表达、SSA/控制流、内存模型、图像/采样语义等。
- 依赖风险：引入 LLVM/NVVM 或自研 compiler pipeline，会带来版本耦合与维护成本。

**结论**
- 长期方向可行且必要，但不应在“资源模型/attachment/sync 语义仍未稳定”时过早切换默认后端输入形态，否则会把两条不稳定轴（语义 + toolchain）叠加在一起。

## When To Switch: Timing Triggers

建议以“触发条件”而不是日期来决定迁移时机：

1. **KernelABI + ResourcePlan 稳定**：descriptor/push constants/dynamic offsets/索引形态已经能在 IR/ABI 中表达，并被测试锁住。
2. **编译成本成为瓶颈**：module 编译频率已从 draw-time 收敛到 pipeline-time 仍然不可接受，或缓存命中率不足。
3. **shader 覆盖进入非模板阶段**：开始支持复杂控制流/大量 op 时，字符串拼接源码会快速失控。
4. **目标 runtime/toolchain 明确偏好 IR**：例如自研 GPU 编译器天然以 LLVM IR 为输入，此时继续扩 CUDA-like 源码会造成重复劳动。

满足 (1) 之后，再择机引入 IR-consuming runtime；否则优先把 (1)(2) 做稳。

## Recommendation (Staged)

1. **短期（继续 bootstrap 扩面）**
   - 维持 CUDA-like 源码路径，但把“编译时机 + 缓存”作为硬门槛：不得每 draw 编译。
   - 继续扩 `GraphicsExecutable` 的资源 plan 与 capability gate，让“可走 GPU”决策前置且可解释。

2. **中期（IR 真实承载语义）**
   - 扩 `SemanticIR/KernelIR` 覆盖资源与关键 fragment 语义，保持与 Vulkan 规则对齐。
   - 让 `CodegenTarget::{CudaLikeSource, LlvmIR}` 对同一份 IR 生成输出，并用 `NormalizedAbiDescription`/tests 防止两条路径漂移。

3. **长期（切换默认并收敛）**
   - 引入 IR-consuming runtime（自研 GPU 或替代后端），将 LLVM IR/bitcode 作为主要输入。
   - CUDA-like 源码保留为 bring-up/debug fallback（或仅用于最小 smoke）。

## References (Current Implementation)

- `src/Backend/CudaCompilerDriver.*` (nvcc spawn + fatbin artifacts)
- `src/Backend/CudaRuntimeAPI.*` (createModule -> compileToFatbin -> cuModuleLoad)
- `src/Pipeline/{SemanticIR,KernelIR,CodegenTarget,CudaLikeSourceEmitter,LlvmIREmitter}.*`
