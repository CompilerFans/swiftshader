# Custom GPU Backend Bring-up / 自研 GPU 后端 Bring-up

## Scope / 范围
This document describes the current bootstrap flow for the custom GPU backend scaffolding. It is intentionally narrower than the public `README.md`, and `README.md` exposure remains deferred until the backend is mature enough for project-level documentation.

本文档描述当前自研 GPU 后端骨架的 bring-up 流程。它刻意比公开的 `README.md` 更窄；在后端成熟到适合项目级公开说明之前，`README.md` 的对外暴露继续延后。

## Configure / 配置
- CMake: `cmake -S . -B build-custom -DSWIFTSHADER_ENABLE_CUSTOM_GPU_BACKEND=ON`
- GN arg: `swiftshader_enable_custom_gpu_backend=true`

## Current Behavior / 当前行为
- The custom backend flag enables backend scaffolding code paths and compile definitions.
- CPU execution remains available as the fallback path for graphics and presentation.
- Compute backend bootstrap produces backend executables and fake-runtime dispatch validation, but does not yet replace the full CPU compute execution path.

- 自研后端开关会启用后端骨架相关代码路径和编译定义。
- 图形与 present 仍保留 CPU fallback 路径。
- compute 后端 bootstrap 已能生成 backend executable 并完成 fake runtime dispatch 验证，但尚未完整替代 CPU compute 执行路径。

## Focused Validation / 聚焦验证
- Build backend tests: `cmake --build build --target backend-unittests --parallel 1`
- Run backend tests: `./build/backend-unittests`
- Focused filters:
  - `./build/backend-unittests --gtest_filter=SemanticIR.*:KernelABI.*:CodegenEmitter.*:AbiParity.*`
  - `./build/backend-unittests --gtest_filter=RuntimeAPI.*:ComputeDispatchValidation.*:ResourceStateTracker.*`

## CPU Fallback / CPU 回退
- Without `SWIFTSHADER_ENABLE_CUSTOM_GPU_BACKEND`, backend selection defaults to CPU.
- With `SWIFTSHADER_ENABLE_CUSTOM_GPU_BACKEND=ON`, the custom backend path is enabled, but unsupported pieces still fall back to CPU-owned behavior where the current bootstrap provides that path.

- 不开启 `SWIFTSHADER_ENABLE_CUSTOM_GPU_BACKEND` 时，后端选择默认走 CPU。
- 开启 `SWIFTSHADER_ENABLE_CUSTOM_GPU_BACKEND=ON` 后，会启用自研后端路径；但当前 bootstrap 尚未覆盖的部分，仍可能回退到 CPU 持有的行为。

## Codegen Dumps / 代码生成导出
- At this stage, generated CUDA-like source and LLVM IR are produced in memory by the emitters.
- There is currently no automatic on-disk dump path. When dumps are added later, they should be documented here before being exposed in broader project documentation.

- 当前阶段，生成的 CUDA 风格源码和 LLVM IR 仍主要以内存字符串形式存在。
- 目前还没有自动写盘的 dump 路径；后续若增加导出能力，应先在本文档记录，再考虑扩展到更公开的项目文档。

## Bring-up Checklist / Bring-up 清单
- Configure with the backend flag: `cmake -S . -B build-custom -DSWIFTSHADER_ENABLE_CUSTOM_GPU_BACKEND=ON`
- Build backend tests: `cmake --build build --target backend-unittests --parallel 1`
- Run backend filters: `./build/backend-unittests --gtest_filter=SemanticIR.*:KernelABI.*:CodegenEmitter.*:AbiParity.*:RuntimeAPI.*:ComputeDispatchValidation.*:ResourceStateTracker.*`
- Build or compile-check Vulkan smoke tests for backend selection, present adapter, and compute pipeline integration.
- Compare behavior with the default CPU path if a regression appears.

## Smoke Tests / 烟测
- `BackendSmoke.ComputePathCanCompile` checks that compute backend bootstrap can produce a backend executable from parsed shader information.
- `BackendSmoke.GraphicsPathStillFallsBackToCpu` checks that the default non-flagged build still selects the CPU backend path.
