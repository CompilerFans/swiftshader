# GPU Backend Bring-up / 自研 GPU 后端 Bring-up

## Scope / 范围
This document describes the current bootstrap flow for the GPU backend scaffolding. It is intentionally narrower than the public `README.md`, and `README.md` exposure remains deferred until the backend is mature enough for project-level documentation.

本文档描述当前自研 GPU 后端骨架的 bring-up 流程。它刻意比公开的 `README.md` 更窄；在后端成熟到适合项目级公开说明之前，`README.md` 的对外暴露继续延后。

## Configure / 配置
- CMake: `cmake -S . -B build-gpu -DSWIFTSHADER_ENABLE_GPU_BACKEND=ON`
- GN arg: `swiftshader_enable_gpu_backend=true`

## CUDA Build / CUDA 构建
- Prerequisites:
  - Install NVIDIA driver so `libcuda.so.1` is available at runtime.
  - Install the CUDA toolkit so CMake can satisfy `find_package(CUDAToolkit REQUIRED)`.
  - Prefer `Subzero` for the current bootstrap flow.
- Recommended configure command:
  - `cmake -S . -B build-cuda-bootstrap -DREACTOR_BACKEND=Subzero -DSWIFTSHADER_ENABLE_GPU_BACKEND=ON -DSWIFTSHADER_GPU_USE_CUDA=ON -DSWIFTSHADER_BUILD_TESTS=ON -DSWIFTSHADER_BUILD_BENCHMARKS=ON`
- Recommended build command:
  - `cmake --build build-cuda-bootstrap --parallel`

- 前置条件：
  - 运行时需要可用的 NVIDIA 驱动，使 `libcuda.so.1` 可被加载。
  - 构建时需要安装 CUDA toolkit，供 CMake 的 `find_package(CUDAToolkit REQUIRED)` 使用。
  - 当前 bootstrap 流程建议搭配 `Subzero`。
- 推荐配置命令：
  - `cmake -S . -B build-cuda-bootstrap -DREACTOR_BACKEND=Subzero -DSWIFTSHADER_ENABLE_GPU_BACKEND=ON -DSWIFTSHADER_GPU_USE_CUDA=ON -DSWIFTSHADER_BUILD_TESTS=ON -DSWIFTSHADER_BUILD_BENCHMARKS=ON`
- 推荐构建命令：
  - `cmake --build build-cuda-bootstrap --parallel`

## CUDA Smoke Tests / CUDA 烟测
- Backend runtime smoke:
  - `./build-cuda-bootstrap/backend-unittests`
- Draw smoke:
  - `SWIFTSHADER_GPU_ALLOW_CPU_FALLBACK=1 SWIFTSHADER_CUDA_DUMP_SOURCE=0 ./build-cuda-bootstrap/draw-unittests --gtest_filter=DrawTest.SolidColorTriangle`
- Focused draw coverage:
  - `SWIFTSHADER_GPU_ALLOW_CPU_FALLBACK=1 SWIFTSHADER_CUDA_DUMP_SOURCE=0 ./build-cuda-bootstrap/draw-unittests --gtest_filter=DrawTest.TexturedTriangleNearest:DrawTest.TexturedTriangleDescriptorArrayIndexOneBootstrapNearest:DrawTest.TexturedTriangleSeparateImageSamplerNearest:DrawTest.TexturedTriangleSeparateImageSamplerDescriptorArrayIndexOneBootstrapNearest:DrawTest.DynamicUniformBufferOffsetsSelectPerDrawColor:DrawTest.DynamicRenderingSolidColorTriangle`
- Vulkan app smoke (`vkcube`):
  - `VK_ICD_FILENAMES=$PWD/build-cuda-bootstrap/Linux/vk_swiftshader_icd.json SWIFTSHADER_CUDA_DUMP_SOURCE=0 SWIFTSHADER_CUDA_DISABLE_WARMUP=1 SWIFTSHADER_CUDA_LAUNCH_STAMP=/tmp/vkcube_cuda_stamps.txt vkcube --c 60`
  - Expect `/tmp/vkcube_cuda_stamps.txt` to be non-empty.
- Vulkan app GPU-render bring-up (`vkcube`, triangle bootstrap writes to swapchain):
  - `VK_ICD_FILENAMES=$PWD/build-cuda-bootstrap/Linux/vk_swiftshader_icd.json SWIFTSHADER_CUDA_DUMP_SOURCE=0 SWIFTSHADER_CUDA_DISABLE_WARMUP=1 SWIFTSHADER_CUDA_LAUNCH_STAMP=/tmp/vkcube_cuda_stamps.txt SWIFTSHADER_GPU_RENDER_TRIANGLE_BOOTSTRAP=1 SWIFTSHADER_GPU_TRACE_TRIANGLE_BOOTSTRAP_RENDER=1 SWIFTSHADER_GPU_TRACE_CPU_DRAW=1 vkcube --c 3`
  - Expect one `[gpu] triangle bootstrap render: rendered=1 wrote=1` line per frame, and no `[gpu] cpu DrawCall::run` lines.
- Benchmark helper:
  - `tests/VulkanBenchmarks/run-animated-triangle-benchmark.sh --backend=cuda --scene=color --seconds=10`

- 后端 runtime 烟测：
  - `./build-cuda-bootstrap/backend-unittests`
- 绘制烟测：
  - `SWIFTSHADER_GPU_ALLOW_CPU_FALLBACK=1 SWIFTSHADER_CUDA_DUMP_SOURCE=0 ./build-cuda-bootstrap/draw-unittests --gtest_filter=DrawTest.SolidColorTriangle`
- 聚焦绘制覆盖：
  - `SWIFTSHADER_GPU_ALLOW_CPU_FALLBACK=1 SWIFTSHADER_CUDA_DUMP_SOURCE=0 ./build-cuda-bootstrap/draw-unittests --gtest_filter=DrawTest.TexturedTriangleNearest:DrawTest.TexturedTriangleDescriptorArrayIndexOneBootstrapNearest:DrawTest.TexturedTriangleSeparateImageSamplerNearest:DrawTest.TexturedTriangleSeparateImageSamplerDescriptorArrayIndexOneBootstrapNearest:DrawTest.DynamicUniformBufferOffsetsSelectPerDrawColor:DrawTest.DynamicRenderingSolidColorTriangle`
- Vulkan 应用烟测（`vkcube`）：
  - `VK_ICD_FILENAMES=$PWD/build-cuda-bootstrap/Linux/vk_swiftshader_icd.json SWIFTSHADER_CUDA_DUMP_SOURCE=0 SWIFTSHADER_CUDA_DISABLE_WARMUP=1 SWIFTSHADER_CUDA_LAUNCH_STAMP=/tmp/vkcube_cuda_stamps.txt vkcube --c 60`
  - 期望 `/tmp/vkcube_cuda_stamps.txt` 非空。
- Vulkan 应用 GPU-render bring-up（`vkcube`，triangle bootstrap 写回 swapchain）：
  - `VK_ICD_FILENAMES=$PWD/build-cuda-bootstrap/Linux/vk_swiftshader_icd.json SWIFTSHADER_CUDA_DUMP_SOURCE=0 SWIFTSHADER_CUDA_DISABLE_WARMUP=1 SWIFTSHADER_CUDA_LAUNCH_STAMP=/tmp/vkcube_cuda_stamps.txt SWIFTSHADER_GPU_RENDER_TRIANGLE_BOOTSTRAP=1 SWIFTSHADER_GPU_TRACE_TRIANGLE_BOOTSTRAP_RENDER=1 SWIFTSHADER_GPU_TRACE_CPU_DRAW=1 vkcube --c 3`
  - 期望每帧输出一行 `[gpu] triangle bootstrap render: rendered=1 wrote=1`，且不出现 `[gpu] cpu DrawCall::run`。
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
- `SWIFTSHADER_CUDA_TRACE_CALLS`
  - When set, print CUDA runtime bring-up and key CUDA driver calls to `stderr` (for example: `cuInit`, context creation, `cuModuleLoad`, `cuLaunchKernel`, memory copies).
- `SWIFTSHADER_CUDA_DISABLE_MODULE_CACHE`
  - Disable the in-process module cache and force `nvcc` compilation for every `createModule(...)` request. Useful for debugging toolchain issues, but extremely slow.

- `SWIFTSHADER_CUDA_DUMP_SOURCE`
  - 为空或未设置时，会把生成的 CUDA 源码打印到 `stderr`。
  - 设为 `0`、`false`、`off` 或 `no` 可关闭 `stderr` dump。
- `SWIFTSHADER_CUDA_SOURCE_DUMP_PATH`
  - 把生成的 CUDA 源码追加写入指定文件。
- `SWIFTSHADER_CUDA_LAUNCH_STAMP`
  - 每次 kernel launch 向指定文件追加一行标记。
- `SWIFTSHADER_CUDA_DISABLE_WARMUP`
  - 关闭 CUDA runtime bootstrap 默认的启动预热 launch。
- `SWIFTSHADER_CUDA_TRACE_CALLS`
  - 设置后会把 CUDA runtime bring-up 和关键 CUDA driver 调用打印到 `stderr`（例如：`cuInit`、context 创建、`cuModuleLoad`、`cuLaunchKernel`、内存拷贝）。
- `SWIFTSHADER_CUDA_DISABLE_MODULE_CACHE`
  - 关闭进程内 module cache，使每次 `createModule(...)` 都强制触发 `nvcc` 编译。用于排查 toolchain 问题，但会极度变慢。

## GPU Bring-up Environment Variables / 自研 GPU Bring-up 环境变量
- `SWIFTSHADER_GPU_ALLOW_CPU_FALLBACK`
  - Allow CPU fallback paths even when running a CUDA-backed build (disables the default CUDA-mode aborts).
  - Useful for debugging or running CPU-based tests with a CUDA-enabled build.
  - When fallback is allowed and `SWIFTSHADER_GPU_RENDER_TRIANGLE_BOOTSTRAP` is unset, the GPU backend still keeps a one-time warmup-only triangle bootstrap attempt before falling back to the CPU renderer.
- `SWIFTSHADER_GPU_TRACE_CPU_COMPUTE`
  - Print when a compute dispatch falls back to the CPU `ComputeProgram::run()` path: `[gpu] cpu ComputeProgram::run`.
- `SWIFTSHADER_GPU_RENDER_TRIANGLE_BOOTSTRAP`
  - Let the GPU execution backend attempt the triangle bootstrap in render-to-attachment mode per draw and write the RGBA output back into the first color attachment (location 0), skipping the CPU `DrawCall::run()` path when successful.
  - Intended for bring-up only; it does **not** implement full Vulkan shader/pipeline semantics yet.
- `SWIFTSHADER_GPU_REQUIRE_TRIANGLE_BOOTSTRAP`
  - Abort if `SWIFTSHADER_GPU_RENDER_TRIANGLE_BOOTSTRAP` cannot render and write back successfully.
- `SWIFTSHADER_GPU_TRACE_TRIANGLE_BOOTSTRAP_RENDER`
  - Print a per-draw line: `[gpu] triangle bootstrap render: rendered=... wrote=...`.
- `SWIFTSHADER_GPU_TRACE_CPU_DRAW`
  - Print when the CPU graphics path is used: `[gpu] cpu DrawCall::run`.
- `SWIFTSHADER_GPU_TRACE_GPU_SUBMIT`
  - Print when the GPU execution backend handles a queue submit: `[gpu] submit via gpu execution backend`.
- `SWIFTSHADER_GPU_TRACE_CPU_SUBMIT`
  - Print when the CPU execution backend handles a queue submit: `[cpu-backend] submit`.
- `SWIFTSHADER_GPU_REQUIRE_GPU_SUBMIT`
  - Abort if the CPU execution backend is selected for queue submit.

- `SWIFTSHADER_GPU_ALLOW_CPU_FALLBACK`
  - 即使在 CUDA build 下也允许走 CPU 回退路径（关闭 CUDA 模式下默认的 abort）。
  - 适用于调试或在 CUDA build 下跑 CPU 相关测试。
  - 当允许 fallback 且未设置 `SWIFTSHADER_GPU_RENDER_TRIANGLE_BOOTSTRAP` 时，GPU backend 仍会先保留一次 warmup-only 的 triangle bootstrap 尝试，然后才回退到 CPU renderer。
- `SWIFTSHADER_GPU_TRACE_CPU_COMPUTE`
  - 当 compute dispatch 回退到 CPU `ComputeProgram::run()` 路径时打印：`[gpu] cpu ComputeProgram::run`。
- `SWIFTSHADER_GPU_RENDER_TRIANGLE_BOOTSTRAP`
  - 让 GPU 执行后端在每次 draw 上尝试 render-to-attachment 形态的最小 CUDA triangle-pipeline bootstrap，并把 RGBA 输出写回到第一个 color attachment（location 0）；成功时跳过 CPU 的 `DrawCall::run()`。
  - 仅用于 bring-up；目前**不**具备完整 Vulkan shader/pipeline 语义。
- `SWIFTSHADER_GPU_REQUIRE_TRIANGLE_BOOTSTRAP`
  - 当 `SWIFTSHADER_GPU_RENDER_TRIANGLE_BOOTSTRAP` 无法成功渲染并写回时直接 abort。
- `SWIFTSHADER_GPU_TRACE_TRIANGLE_BOOTSTRAP_RENDER`
  - 每次 draw 打印一行：`[gpu] triangle bootstrap render: rendered=... wrote=...`。
- `SWIFTSHADER_GPU_TRACE_CPU_DRAW`
  - 当 CPU 图形路径被使用时打印：`[gpu] cpu DrawCall::run`。
- `SWIFTSHADER_GPU_TRACE_GPU_SUBMIT`
  - 当 GPU 执行后端处理一次 queue submit 时打印：`[gpu] submit via gpu execution backend`。
- `SWIFTSHADER_GPU_TRACE_CPU_SUBMIT`
  - 当 CPU 执行后端处理一次 queue submit 时打印：`[cpu-backend] submit`。
- `SWIFTSHADER_GPU_REQUIRE_GPU_SUBMIT`
  - 当 queue submit 选择到 CPU 执行后端时直接 abort。

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
- The GPU backend flag enables backend scaffolding code paths and compile definitions.
- Graphics draw now enters the backend through `ExecutionBackend::draw(...)`; `CmdDrawBase` no longer calls `Renderer::draw()` directly.
- `GpuExecutionBackend` now owns triangle-bootstrap routing:
  - strict GPU mode renders/write-backs in the backend helper and aborts on failure
  - fallback-enabled mode can still perform a one-time warmup-only bootstrap before falling back to the CPU renderer
  - explicit `SWIFTSHADER_GPU_RENDER_TRIANGLE_BOOTSTRAP=1` keeps the backend on the render-to-attachment path
- `Renderer::draw()` is now CPU-only; it no longer reads GPU runtime state or GPU bring-up environment variables.
- The current triangle-bootstrap render path forwards the same fragment/bootstrap parameters used during dry-run probing, so strict GPU draw no longer silently falls back to the bootstrap default fragment color.
- In CUDA-backed builds, CPU fallback for queue submit, graphics draw, and compute dispatch is disabled by default (CPU submit/draw/compute aborts). Set `SWIFTSHADER_GPU_ALLOW_CPU_FALLBACK=1` to re-enable CPU fallback.
- Compute backend bootstrap produces backend executables and stub-runtime dispatch validation, but does not yet replace the full CPU compute execution path.
- `GraphicsPipeline` now also builds a metadata-only `backend::GraphicsExecutable` when a vertex stage is present.
  - It stores stage metadata plus shader-only triangle-bootstrap metadata such as constant `gl_PointSize` and non-texture fragment bootstrap templates.
  - It now stores a sampled-image texture plan, separating resource shape from current bootstrap support:
    - `CombinedImageSampler`: one sampled-image resource expressed as a single `COMBINED_IMAGE_SAMPLER` binding
    - `SeparateImageSampler`: one sampled image plus one sampler binding
    - `Other`: more complex sampled-image layouts such as multiple combined image samplers
  - The plan is built from the descriptors that actually feed texture sample instructions, not from every fragment-shader descriptor decoration; unrelated UBOs or other non-sampled descriptors do not change sampled-image classification.
  - It also stores an image resource plan that lists the fragment shader's sampled-image-related descriptors and storage image descriptors (including descriptor-array element when constant), so future backend resource modeling can consume stable metadata without re-scanning SPIR-V.
  - The legacy bootstrap-compatible binding metadata is still derived only from the `CombinedImageSampler` plan when the fragment shader also fits the narrow bootstrap path: `location 0` is exactly `vec2`, the shader samples a texture, the final `location 0` value resolves to that sample through only trivial pass-through instructions, and the layout binding is a `COMBINED_IMAGE_SAMPLER` descriptor with a constant descriptor-array element (default `0`) that is in-bounds for `descriptorCount`.
  - Bootstrap-compatible texture bindings are rejected when the fragment shader contains storage-image read/write operations (`OpImageRead`/`OpImageWrite`), because triangle bootstrap cannot emulate those side effects.
  - Texture bootstrap materialization still happens at draw time in `TriangleBootstrapDraw`, because it depends on bound descriptor/image/sampler state; that draw-time path now consumes the richer texture plan and can materialize both `CombinedImageSampler` and narrow `SeparateImageSampler` shapes.
  - Draw execution still goes through the existing backend helper / CPU renderer path.
- Remaining CPU-owned areas after this slice: the CPU renderer implementation itself plus broader transfer/copy/blit/resolve/present ownership and the future full graphics executable execution/resource model.

- GPU backend 开关会启用 backend scaffolding 代码路径和相关编译定义。
- 图形 draw 现在先经由 `ExecutionBackend::draw(...)` 进入 backend；`CmdDrawBase` 不再直接调用 `Renderer::draw()`。
- `GpuExecutionBackend` 现在直接持有 triangle-bootstrap 路由：
  - strict GPU 模式在 backend helper 中执行 render/write-back，失败即 abort
  - 允许 fallback 时，仍可先执行一次 warmup-only bootstrap，再落回 CPU renderer
  - 显式设置 `SWIFTSHADER_GPU_RENDER_TRIANGLE_BOOTSTRAP=1` 时，会保持在 render-to-attachment 路径
- `Renderer::draw()` 已收敛为 CPU-only，不再读取 GPU runtime 状态或 GPU bring-up 环境变量。
- 当前 triangle-bootstrap 的实际 render 分支已经会携带与 dry-run probe 相同的 fragment/bootstrap 参数，因此 strict GPU draw 不再静默掉回 bootstrap 默认片元颜色。
- 在 CUDA build 下，queue submit / graphics draw / compute dispatch 默认关闭 CPU fallback（命中 CPU submit/draw/compute 会 abort）。设置 `SWIFTSHADER_GPU_ALLOW_CPU_FALLBACK=1` 可重新允许 CPU fallback。
- 当前 compute backend bootstrap 已经具备 backend executable 与 stub-runtime dispatch 验证，但还没有完全替代 CPU compute 执行路径。
- `GraphicsPipeline` 现在也会在存在 vertex stage 时构建 metadata-only `backend::GraphicsExecutable`。
  - 它除了 stage metadata 之外，还会持有 shader-only 的 triangle-bootstrap metadata，例如 constant `gl_PointSize` 和非 texture 的 fragment bootstrap 模板。
  - 它现在还会持有 sampled-image texture plan，把资源形态和当前 bootstrap 支持显式拆开：
    - `CombinedImageSampler`：单个 `COMBINED_IMAGE_SAMPLER` binding 表达的一路 sampled-image 资源
    - `SeparateImageSampler`：单个 sampled image binding 加单个 sampler binding
    - `Other`：更复杂的 sampled-image 资源布局，例如多个 combined image samplers
  - 这个 plan 现在只根据真实喂给 texture sample 指令的 descriptor 来构建，而不是简单枚举 fragment shader 的全部 descriptor decoration；因此 unrelated UBO / 其他 non-sampled descriptor 不会再把 sampled-image classification 污染成 `Other`。
  - 它现在还会持有 image resource plan：显式列出 fragment shader 的 sampled-image 相关 descriptors 与 storage image descriptors（含常量 descriptor-array element），为后续更一般的 backend 资源建模提供稳定元数据入口。
  - 它现在还会持有更通用的 resource plan：包含 layout 轮廓（descriptor set 数、dynamic offset 数、push constant size 占位）与 descriptor refs（含 descriptor type/count、常量 array element、以及 dynamic offset index），供后续统一的资源物化/ABI 与 capability gate 使用。
  - 它现在还会持有 fragment feature mask（discard/storage-image/image-query/derivatives/atomics/subgroup），作为 correctness-first capability/side-effect gate 的输入。
  - 兼容旧调用方的 bootstrap binding metadata 仍然只会从 `CombinedImageSampler` plan 中派生，而且 fragment shader 还必须落在当前 narrow bootstrap 路径上：`location 0` 必须恰好是 `vec2`、shader 需要执行 texture sample、最终写入 `location 0` 的值只允许通过极小的 trivial passthrough 链解析到该 sample 结果，且 layout 上该 binding 必须是 `COMBINED_IMAGE_SAMPLER` descriptor；若该 binding 是 descriptor array，则 array element 必须是常量（默认 `0`）并落在 `descriptorCount` 范围内。
  - 一旦检测到 fragment shader 含 storage image read/write（`OpImageRead`/`OpImageWrite`），必须拒绝 bootstrap-compatible texture binding，因为 triangle bootstrap 无法模拟 storage image side-effect。
  - texture bootstrap 的 descriptor/image/sampler 物化仍留在 `TriangleBootstrapDraw` 的 draw-time 路径，因为那部分依赖绑定态；但这条路径现在已经能直接消费 richer texture plan，并支持 narrow `SeparateImageSampler` 的 strict GPU draw 物化。
  - draw 执行本身仍沿用现有 backend helper / CPU renderer 路径。
- 本切片之后仍然保留 CPU ownership 的范围：CPU renderer 本体，以及更大范围内尚未 backend-owned 的 transfer/copy/blit/resolve/present 与未来完整 graphics executable execution/resource model。

- 自研后端开关会启用后端骨架相关代码路径和编译定义。
- CUDA build 下默认切断 queue submit、图形 draw、compute dispatch 的 CPU fallback（CPU submit/draw/compute 会 abort）；可通过 `SWIFTSHADER_GPU_ALLOW_CPU_FALLBACK=1` 重新允许 CPU fallback。
- compute 后端 bootstrap 已能生成 backend executable 并完成 stub runtime dispatch 验证，但尚未完整替代 CPU compute 执行路径。

## Focused Validation / 聚焦验证
- Build backend tests: `cmake --build build --target backend-unittests --parallel 1`
- Run backend tests: `./build/backend-unittests`
- Focused filters:
  - `./build/backend-unittests --gtest_filter=SemanticIR.*:KernelABI.*:CodegenEmitter.*:AbiParity.*`
  - `./build/backend-unittests --gtest_filter=RuntimeAPI.*:ComputeDispatchValidation.*:ResourceStateTracker.*`
  - `./build/backend-unittests --gtest_filter=GraphicsExecutable.*`
  - `./build/vk-unittests --gtest_filter='GraphicsBackendPipeline.*:BackendSmoke.*:GraphicsBackendSelection.*'`

## CPU Fallback / CPU 回退
- Without `SWIFTSHADER_ENABLE_GPU_BACKEND`, backend selection defaults to CPU.
- With `SWIFTSHADER_ENABLE_GPU_BACKEND=ON`, the GPU backend path is enabled. In CUDA-backed builds, CPU fallback for submit/draw/compute is disabled by default; set `SWIFTSHADER_GPU_ALLOW_CPU_FALLBACK=1` to override.

- 不开启 `SWIFTSHADER_ENABLE_GPU_BACKEND` 时，后端选择默认走 CPU。
- 开启 `SWIFTSHADER_ENABLE_GPU_BACKEND=ON` 后，会启用自研后端路径；CUDA build 下默认不允许 submit/draw/compute 回退到 CPU，可通过 `SWIFTSHADER_GPU_ALLOW_CPU_FALLBACK=1` 覆盖该行为。

## Codegen Dumps / 代码生成导出
- CUDA-like source can be dumped via `SWIFTSHADER_CUDA_DUMP_SOURCE` (stderr) and `SWIFTSHADER_CUDA_SOURCE_DUMP_PATH` (append to file).
- The current CUDA bring-up path still relies on generating CUDA-like source and compiling it into a loadable module; see the IR migration notes for timing and feasibility.
- LLVM IR dumps are not yet a stable, supported surface.

- CUDA 风格源码可通过 `SWIFTSHADER_CUDA_DUMP_SOURCE`（stderr）与 `SWIFTSHADER_CUDA_SOURCE_DUMP_PATH`（追加写文件）导出。
- 当前 CUDA bring-up 路径仍依赖“生成 CUDA 风格源码并编译成可加载 module”的链路；其 IR 迁移时机与可行性见下方链接。
- LLVM IR 的 dump 目前尚未形成稳定的对外能力面。

## Design Notes / 设计笔记
- GPU 框架迁移的总体结论（面向更大支持面的迁移点）：`docs/plans/2026-03-12-gpu-migration-framework-adjustments.md`
- 从 “nvcc 文本源码路径” 迁到 “IR-based codegen” 的时机与可行性：`docs/plans/2026-03-12-ir-codegen-migration-timing.md`

## Bring-up Checklist / Bring-up 清单
- Configure with the backend flag: `cmake -S . -B build-gpu -DSWIFTSHADER_ENABLE_GPU_BACKEND=ON`
- Build backend tests: `cmake --build build --target backend-unittests --parallel 1`
- Run backend filters: `./build/backend-unittests --gtest_filter=SemanticIR.*:KernelABI.*:CodegenEmitter.*:AbiParity.*:RuntimeAPI.*:ComputeDispatchValidation.*:ResourceStateTracker.*`
- Build or compile-check Vulkan smoke tests for backend selection, present adapter, and compute pipeline integration.
- Compare behavior with the default CPU path if a regression appears.

## Smoke Tests / 烟测
- `BackendSmoke.ComputePathCanCompile` checks that compute backend bootstrap can produce a backend executable from parsed shader information.
- `BackendSmoke.DefaultBackendSelectionMatchesBuildFlags` checks that the default non-flagged build still selects the CPU backend path.
