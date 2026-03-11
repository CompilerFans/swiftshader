# Custom GPU Backend Bring-up / 自研 GPU 后端 Bring-up

## Scope / 范围
This document describes the current bootstrap flow for the custom GPU backend scaffolding. It is intentionally narrower than the public `README.md`, and `README.md` exposure remains deferred until the backend is mature enough for project-level documentation.

本文档描述当前自研 GPU 后端骨架的 bring-up 流程。它刻意比公开的 `README.md` 更窄；在后端成熟到适合项目级公开说明之前，`README.md` 的对外暴露继续延后。

## Configure / 配置
- CMake: `cmake -S . -B build-custom -DSWIFTSHADER_ENABLE_CUSTOM_GPU_BACKEND=ON`
- GN arg: `swiftshader_enable_custom_gpu_backend=true`

## CUDA Build / CUDA 构建
- Prerequisites:
  - Install NVIDIA driver so `libcuda.so.1` is available at runtime.
  - Install the CUDA toolkit so CMake can satisfy `find_package(CUDAToolkit REQUIRED)`.
  - Prefer `Subzero` for the current bootstrap flow.
- Recommended configure command:
  - `cmake -S . -B build-cuda-bootstrap -DREACTOR_BACKEND=Subzero -DSWIFTSHADER_ENABLE_CUSTOM_GPU_BACKEND=ON -DSWIFTSHADER_CUSTOM_GPU_USE_CUDA=ON -DSWIFTSHADER_BUILD_TESTS=ON -DSWIFTSHADER_BUILD_BENCHMARKS=ON`
- Recommended build command:
  - `cmake --build build-cuda-bootstrap --parallel`

- 前置条件：
  - 运行时需要可用的 NVIDIA 驱动，使 `libcuda.so.1` 可被加载。
  - 构建时需要安装 CUDA toolkit，供 CMake 的 `find_package(CUDAToolkit REQUIRED)` 使用。
  - 当前 bootstrap 流程建议搭配 `Subzero`。
- 推荐配置命令：
  - `cmake -S . -B build-cuda-bootstrap -DREACTOR_BACKEND=Subzero -DSWIFTSHADER_ENABLE_CUSTOM_GPU_BACKEND=ON -DSWIFTSHADER_CUSTOM_GPU_USE_CUDA=ON -DSWIFTSHADER_BUILD_TESTS=ON -DSWIFTSHADER_BUILD_BENCHMARKS=ON`
- 推荐构建命令：
  - `cmake --build build-cuda-bootstrap --parallel`

## CUDA Smoke Tests / CUDA 烟测
- Backend runtime smoke:
  - `./build-cuda-bootstrap/backend-unittests`
- Draw smoke:
  - `SWIFTSHADER_CUDA_DUMP_SOURCE=0 ./build-cuda-bootstrap/draw-unittests --gtest_filter=DrawTest.SolidColorTriangle`
- Focused draw coverage:
  - `SWIFTSHADER_CUDA_DUMP_SOURCE=0 ./build-cuda-bootstrap/draw-unittests --gtest_filter=DrawTest.TexturedTriangleNearest:DrawTest.TexturedTriangleSeparateImageSamplerNearest:DrawTest.DynamicUniformBufferOffsetsSelectPerDrawColor:DrawTest.DynamicRenderingSolidColorTriangle`
- Vulkan app smoke (`vkcube`):
  - `VK_ICD_FILENAMES=$PWD/build-cuda-bootstrap/Linux/vk_swiftshader_icd.json SWIFTSHADER_CUDA_DUMP_SOURCE=0 SWIFTSHADER_CUDA_DISABLE_WARMUP=1 SWIFTSHADER_CUDA_LAUNCH_STAMP=/tmp/vkcube_cuda_stamps.txt vkcube --c 60`
  - Expect `/tmp/vkcube_cuda_stamps.txt` to be non-empty.
- Vulkan app GPU-render bring-up (`vkcube`, triangle bootstrap writes to swapchain):
  - `VK_ICD_FILENAMES=$PWD/build-cuda-bootstrap/Linux/vk_swiftshader_icd.json SWIFTSHADER_CUDA_DUMP_SOURCE=0 SWIFTSHADER_CUDA_DISABLE_WARMUP=1 SWIFTSHADER_CUDA_LAUNCH_STAMP=/tmp/vkcube_cuda_stamps.txt SWIFTSHADER_CUSTOM_GPU_RENDER_TRIANGLE_BOOTSTRAP=1 SWIFTSHADER_CUSTOM_GPU_TRACE_TRIANGLE_BOOTSTRAP_RENDER=1 SWIFTSHADER_CUSTOM_GPU_TRACE_CPU_DRAW=1 vkcube --c 3`
  - Expect one `[custom-gpu] triangle bootstrap render: rendered=1 wrote=1` line per frame, and no `[custom-gpu] cpu DrawCall::run` lines.
- Benchmark helper:
  - `tests/VulkanBenchmarks/run-animated-triangle-benchmark.sh --backend=cuda --scene=color --seconds=10`

- 后端 runtime 烟测：
  - `./build-cuda-bootstrap/backend-unittests`
- 绘制烟测：
  - `SWIFTSHADER_CUDA_DUMP_SOURCE=0 ./build-cuda-bootstrap/draw-unittests --gtest_filter=DrawTest.SolidColorTriangle`
- 聚焦绘制覆盖：
  - `SWIFTSHADER_CUDA_DUMP_SOURCE=0 ./build-cuda-bootstrap/draw-unittests --gtest_filter=DrawTest.TexturedTriangleNearest:DrawTest.TexturedTriangleSeparateImageSamplerNearest:DrawTest.DynamicUniformBufferOffsetsSelectPerDrawColor:DrawTest.DynamicRenderingSolidColorTriangle`
- Vulkan 应用烟测（`vkcube`）：
  - `VK_ICD_FILENAMES=$PWD/build-cuda-bootstrap/Linux/vk_swiftshader_icd.json SWIFTSHADER_CUDA_DUMP_SOURCE=0 SWIFTSHADER_CUDA_DISABLE_WARMUP=1 SWIFTSHADER_CUDA_LAUNCH_STAMP=/tmp/vkcube_cuda_stamps.txt vkcube --c 60`
  - 期望 `/tmp/vkcube_cuda_stamps.txt` 非空。
- Vulkan 应用 GPU-render bring-up（`vkcube`，triangle bootstrap 写回 swapchain）：
  - `VK_ICD_FILENAMES=$PWD/build-cuda-bootstrap/Linux/vk_swiftshader_icd.json SWIFTSHADER_CUDA_DUMP_SOURCE=0 SWIFTSHADER_CUDA_DISABLE_WARMUP=1 SWIFTSHADER_CUDA_LAUNCH_STAMP=/tmp/vkcube_cuda_stamps.txt SWIFTSHADER_CUSTOM_GPU_RENDER_TRIANGLE_BOOTSTRAP=1 SWIFTSHADER_CUSTOM_GPU_TRACE_TRIANGLE_BOOTSTRAP_RENDER=1 SWIFTSHADER_CUSTOM_GPU_TRACE_CPU_DRAW=1 vkcube --c 3`
  - 期望每帧输出一行 `[custom-gpu] triangle bootstrap render: rendered=1 wrote=1`，且不出现 `[custom-gpu] cpu DrawCall::run`。
- Benchmark 辅助脚本：
  - `tests/VulkanBenchmarks/run-animated-triangle-benchmark.sh --backend=cuda --scene=color --seconds=10`

## Vulkan ICD Usage / 作为 Vulkan ICD 使用
- Create an ICD manifest that points to the CUDA-enabled `libvk_swiftshader.so`, for example:
  - `{"file_format_version":"1.0.0","ICD":{"library_path":"/absolute/path/to/build-cuda-bootstrap/libvk_swiftshader.so","api_version":"1.0.5"}}`
- Run Vulkan applications with `VK_ICD_FILENAMES=/path/to/swiftshader-icd.json`.

- 新建一个指向 CUDA 构建产物 `libvk_swiftshader.so` 的 ICD manifest，例如：
  - `{"file_format_version":"1.0.0","ICD":{"library_path":"/absolute/path/to/build-cuda-bootstrap/libvk_swiftshader.so","api_version":"1.0.5"}}`
- 运行 Vulkan 应用时设置 `VK_ICD_FILENAMES=/path/to/swiftshader-icd.json`。

## CUDA Environment Variables / CUDA 环境变量
- `SWIFTSHADER_CUDA_DUMP_SOURCE`
  - Empty or unset means dump generated CUDA source to `stderr`.
  - Set to `0`, `false`, `off`, or `no` to suppress the `stderr` dump.
- `SWIFTSHADER_CUDA_SOURCE_DUMP_PATH`
  - Append generated CUDA source to the specified file.
- `SWIFTSHADER_CUDA_LAUNCH_STAMP`
  - Append one line per kernel launch to the specified file.
- `SWIFTSHADER_CUDA_DISABLE_WARMUP`
  - Disable the startup warmup launch done by the CUDA runtime bootstrap.

- `SWIFTSHADER_CUDA_DUMP_SOURCE`
  - 为空或未设置时，会把生成的 CUDA 源码打印到 `stderr`。
  - 设为 `0`、`false`、`off` 或 `no` 可关闭 `stderr` dump。
- `SWIFTSHADER_CUDA_SOURCE_DUMP_PATH`
  - 把生成的 CUDA 源码追加写入指定文件。
- `SWIFTSHADER_CUDA_LAUNCH_STAMP`
  - 每次 kernel launch 向指定文件追加一行标记。
- `SWIFTSHADER_CUDA_DISABLE_WARMUP`
  - 关闭 CUDA runtime bootstrap 默认的启动预热 launch。

## Custom GPU Bring-up Environment Variables / 自研 GPU Bring-up 环境变量
- `SWIFTSHADER_CUSTOM_GPU_RENDER_TRIANGLE_BOOTSTRAP`
  - Attempt a minimal CUDA triangle-pipeline bootstrap per draw and write the RGBA output back into the first color attachment (location 0), skipping the CPU `DrawCall::run()` path when successful.
  - Intended for bring-up only; it does **not** implement full Vulkan shader/pipeline semantics yet.
- `SWIFTSHADER_CUSTOM_GPU_REQUIRE_TRIANGLE_BOOTSTRAP`
  - Abort if `SWIFTSHADER_CUSTOM_GPU_RENDER_TRIANGLE_BOOTSTRAP` cannot render and write back successfully.
- `SWIFTSHADER_CUSTOM_GPU_TRACE_TRIANGLE_BOOTSTRAP_RENDER`
  - Print a per-draw line: `[custom-gpu] triangle bootstrap render: rendered=... wrote=...`.
- `SWIFTSHADER_CUSTOM_GPU_TRACE_CPU_DRAW`
  - Print when the CPU graphics path is used: `[custom-gpu] cpu DrawCall::run`.
- `SWIFTSHADER_CUSTOM_GPU_TRACE_CPU_SUBMIT`
  - Print when the custom execution backend still falls back to CPU submit: `[custom-gpu] submit falling back to CPU backend`.
- `SWIFTSHADER_CUSTOM_GPU_REQUIRE_CUSTOM_SUBMIT`
  - Abort if the custom execution backend would fall back to CPU submit.

- `SWIFTSHADER_CUSTOM_GPU_RENDER_TRIANGLE_BOOTSTRAP`
  - 每次 draw 尝试执行最小的 CUDA triangle-pipeline bootstrap，并把 RGBA 输出写回到第一个 color attachment（location 0）；成功时跳过 CPU 的 `DrawCall::run()`。
  - 仅用于 bring-up；目前**不**具备完整 Vulkan shader/pipeline 语义。
- `SWIFTSHADER_CUSTOM_GPU_REQUIRE_TRIANGLE_BOOTSTRAP`
  - 当 `SWIFTSHADER_CUSTOM_GPU_RENDER_TRIANGLE_BOOTSTRAP` 无法成功渲染并写回时直接 abort。
- `SWIFTSHADER_CUSTOM_GPU_TRACE_TRIANGLE_BOOTSTRAP_RENDER`
  - 每次 draw 打印一行：`[custom-gpu] triangle bootstrap render: rendered=... wrote=...`。
- `SWIFTSHADER_CUSTOM_GPU_TRACE_CPU_DRAW`
  - 当 CPU 图形路径被使用时打印：`[custom-gpu] cpu DrawCall::run`。
- `SWIFTSHADER_CUSTOM_GPU_TRACE_CPU_SUBMIT`
  - 当自研执行后端仍回退到 CPU submit 时打印：`[custom-gpu] submit falling back to CPU backend`。
- `SWIFTSHADER_CUSTOM_GPU_REQUIRE_CUSTOM_SUBMIT`
  - 当自研执行后端要回退到 CPU submit 时直接 abort。

## Compute Builtin Mapping / Compute Builtin 映射
- Vulkan compute dispatch dimensions:
  - `vkCmdDispatch(groupCountX, groupCountY, groupCountZ)` sets `gl_NumWorkGroups`.
  - `layout(local_size_x=..., local_size_y=..., local_size_z=...) in;` sets `gl_WorkGroupSize`.
  - CUDA mapping: `gridDim == gl_NumWorkGroups`, `blockDim == gl_WorkGroupSize`.
- Workgroup and invocation IDs:
  - `gl_WorkGroupID` ↔ `blockIdx`
  - `gl_LocalInvocationID` ↔ `threadIdx`
  - `gl_GlobalInvocationID = gl_WorkGroupID * gl_WorkGroupSize + gl_LocalInvocationID` (component-wise) ↔ `blockIdx * blockDim + threadIdx`
  - `gl_LocalInvocationIndex = ((threadIdx.z * blockDim.y) + threadIdx.y) * blockDim.x + threadIdx.x`
- `vkCmdDispatchBase(baseGroupX, baseGroupY, baseGroupZ, groupCountX, groupCountY, groupCountZ)`:
  - `gl_WorkGroupID = baseGroup + blockIdx`, so `gl_GlobalInvocationID = (baseGroup + blockIdx) * blockDim + threadIdx`.
- SwiftShader CPU reference: `src/Pipeline/ComputeProgram.cpp` computes `gl_GlobalInvocationId` the same way (see `ComputeProgram::setSubgroupBuiltins()`).

- Vulkan compute dispatch 的维度语义：
  - `vkCmdDispatch(groupCountX, groupCountY, groupCountZ)` 对应 `gl_NumWorkGroups`。
  - `layout(local_size_x=..., local_size_y=..., local_size_z=...) in;` 对应 `gl_WorkGroupSize`。
  - CUDA 映射：`gridDim == gl_NumWorkGroups`，`blockDim == gl_WorkGroupSize`。
- Workgroup / invocation ID：
  - `gl_WorkGroupID` ↔ `blockIdx`
  - `gl_LocalInvocationID` ↔ `threadIdx`
  - `gl_GlobalInvocationID = gl_WorkGroupID * gl_WorkGroupSize + gl_LocalInvocationID`（逐分量）↔ `blockIdx * blockDim + threadIdx`
  - `gl_LocalInvocationIndex = ((threadIdx.z * blockDim.y) + threadIdx.y) * blockDim.x + threadIdx.x`
- `vkCmdDispatchBase(baseGroupX, baseGroupY, baseGroupZ, groupCountX, groupCountY, groupCountZ)`：
  - `gl_WorkGroupID = baseGroup + blockIdx`，因此 `gl_GlobalInvocationID = (baseGroup + blockIdx) * blockDim + threadIdx`。
- SwiftShader CPU 参考实现：`src/Pipeline/ComputeProgram.cpp` 里的 builtin 计算与上述公式一致（见 `ComputeProgram::setSubgroupBuiltins()`）。

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
