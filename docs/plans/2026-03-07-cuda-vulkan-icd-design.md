# 自研 GPU Vulkan ICD 设计 / Custom-GPU Vulkan ICD Design

## 状态 / Status
草稿。本文档在 brainstorming 过程中逐步更新；只有标记为“已确认”的部分代表已与用户达成一致。  
Draft. This document is updated incrementally during brainstorming. Only sections marked as “confirmed” reflect approved conclusions.

## 已确认目标 / Confirmed Goals
- 在自研 GPU 之上构建尽可能完整的 Vulkan 图形 ICD。  
  Build a Vulkan graphics ICD that is as complete as practical on top of a custom in-house GPU.
- 优先做面向自研 GPU 的原生实现，而不是 CPU fallback 或对现有图形驱动的薄封装。  
  Prefer a native implementation for the custom GPU over a CPU fallback or a thin wrapper around an existing graphics driver.
- 第一阶段必须支持真实窗口显示、`Swapchain` 和 `Present`，而不仅是离屏渲染。  
  Phase 1 must support real window display, `Swapchain`, and `Present`, not only offscreen rendering.

## 已确认平台约束 / Confirmed Platform Constraints
- 目标并非 NVIDIA GPU，后端不能依赖 PTX。  
  The target is not an NVIDIA GPU, and the backend must not depend on PTX.
- 自研 GPU 的编译工具链可接受两类输入：CUDA 风格高级语言，或 LLVM IR。  
  The custom GPU compiler toolchain can accept either CUDA-like high-level source or LLVM IR.
- SwiftShader 侧只负责 lowering 到上述输入形式；后端编译器或中端由外部工具链负责。  
  SwiftShader only needs to lower into those accepted forms; the backend compiler or middle-end is owned outside SwiftShader.
- 初期驱动适配先采用 CUDA 兼容 runtime 接口，以降低 bring-up 成本。  
  Initial driver integration should use a CUDA-compatible runtime interface to reduce bring-up cost.
- 运行时层后续必须可替换为更底层的 native interface，而不影响 Vulkan 前端语义。  
  The runtime layer must later be replaceable by a lower-level native interface without changing Vulkan frontend semantics.
- Shader 编译从一开始就要支持 JIT 和离线编译两种模式。  
  Shader compilation must support both JIT and offline compilation from the beginning.

## 已确认总体方向 / Confirmed Direction
### 选定方案 / Chosen Approach
保留 SwiftShader 作为 Vulkan 前端与语义基础，但将当前 CPU 渲染后端替换为自研 GPU 图形后端。  
Use SwiftShader as the Vulkan frontend and semantic foundation, while replacing the current CPU rendering backend with a custom-GPU graphics backend.

### 对比过的备选方案 / Alternatives Considered
1. 保留 Vulkan 前端，重建自研 GPU 后端。**已选定。**  
   Keep the Vulkan frontend and rebuild the backend for the custom GPU. **Chosen.**
2. 只保留 ICD 外壳，底层整体重写。  
   Keep only the ICD shell and redesign almost everything underneath.
3. 先做受限的 compute 风格兼容层，再逐步扩展。  
   Build a restricted compute-style compatibility layer first and grow it over time.

### 选择原因 / Why This Approach
- 复用 `src/Vulkan/` 中成熟的 ICD、对象模型、命令流、描述符和同步语义。  
  Reuse the mature ICD, object model, command flow, descriptor handling, and synchronization semantics in `src/Vulkan/`.
- 复用 `src/Pipeline/` 中已有的 SPIR-V 解析与 shader/pipeline 语义分析。  
  Reuse the SPIR-V parsing and shader/pipeline semantic analysis in `src/Pipeline/`.
- 避免落入长期停留在“不完整兼容层”的陷阱。  
  Avoid getting trapped in a permanently incomplete compatibility layer.
- 在最大限度复用现有 Vulkan 语义资产的同时，允许彻底替换执行后端。  
  Maximize reuse of Vulkan semantic assets while still allowing a full backend replacement.

## 已确认目标架构 / Confirmed Target Architecture
### 第 1 层：Vulkan ICD / API 前端 / Layer 1: Vulkan ICD / API Frontend
保留 `src/Vulkan/` 大部分实现，继续负责对象生命周期、命令录制、描述符、pipeline 创建、同步对象和 swapchain 对外语义。  
Keep most of `src/Vulkan/` and continue to own object lifetime, command recording, descriptors, pipeline creation, synchronization objects, and swapchain-facing behavior.

### 第 2 层：IR / Pipeline 语义层 / Layer 2: IR / Pipeline Semantic Layer
保留 `src/Pipeline/` 中 SPIR-V 解析和 pipeline 语义建模，但要与 Reactor 和 CPU JIT 执行路径解耦，演化为后端无关的 lowering 层。  
Keep SPIR-V parsing and pipeline semantic modeling in `src/Pipeline/`, but decouple them from Reactor and CPU JIT execution so this layer evolves into a backend-neutral lowering layer.

### 第 3 层：自研 GPU 图形后端 / Layer 3: Custom GPU Graphics Backend
新增后端层，负责 device/queue、内存对象、shader executable 生成与缓存、图形阶段执行、compute 执行、copy/clear/resolve 以及 attachment 相关操作。  
Add a new backend layer to own device/queue management, memory objects, shader executable generation and cache, graphics-stage execution, compute execution, copy/clear/resolve operations, and attachment handling.

### 第 4 层：WSI / Present 集成 / Layer 4: WSI / Present Integration
保留 `src/WSI/` 的抽象边界，但以自研 GPU 的显示/内存共享路径重做 swapchain image 和 present 流程。  
Keep the abstraction boundaries in `src/WSI/`, but reimplement swapchain images and present flow around the custom GPU display and memory-sharing path.

## 已确认模块策略 / Confirmed Module Strategy
### 基本保留 / Keep Largely Intact
- `src/Vulkan/` 前端与 ICD 对象模型。  
  `src/Vulkan/` frontend and ICD object model.
- `src/Pipeline/` 中 SPIR-V 解析与 shader 语义分析。  
  SPIR-V parsing and shader semantic analysis in `src/Pipeline/`.
- `src/WSI/` 中 surface / swapchain 抽象边界。  
  Surface / swapchain abstraction boundaries in `src/WSI/`.
- `tests/` 中现有 Vulkan 单测与 regres 体系。  
  Existing Vulkan unit tests and regres infrastructure in `tests/`.

### 需要大幅重写 / Rewrite Substantially
- `src/Device/` 的大部分执行路径，包括 CPU renderer、setup、raster、fragment 和 blending 流程。  
  Most of `src/Device/`, including CPU renderer execution, setup, raster, fragment, and blending flow.
- 目前用于生成 CPU 代码的 Reactor / LLVM / Subzero 执行路径。  
  Reactor / LLVM / Subzero execution paths currently used to generate CPU code.
- 假设 host-first 或 CPU-rendered image 的资源和内存内部实现。  
  Resource and memory internals that assume host-first or CPU-rendered images.

### 新增核心区域 / New Core Area
建议新增 `src/Backend/` 或 `src/CustomGPU/`，负责：  
A new core area, likely `src/Backend/` or `src/CustomGPU/`, should own:
- device / queue / 调度管理 / device, queue, and scheduling management
- memory / image / buffer backing / memory, image, and buffer backing
- shader executable 生成与缓存 / shader executable generation and cache
- raster / fragment / copy / clear 路径 / rasterization, fragment, copy, and clear paths
- WSI / present 集成 / WSI and present integration

## 已确认图形执行模型 / Confirmed Graphics Execution Model
### 阶段映射 / Stage Mapping
- Vertex 与 compute shader 以 kernel 形式在顶点、实例或 workgroup 维度执行。  
  Vertex and compute shaders execute as kernels over vertices, instances, or workgroups.
- Primitive assembly 与 setup 使用独立 kernel，产出轻量 primitive descriptor。  
  Primitive assembly and setup execute in separate kernels that generate lightweight primitive descriptors.
- Rasterization 采用 tile-based 管线，并显式进行 primitive binning。  
  Rasterization uses a tile-based pipeline with explicit primitive binning.
- Fragment 处理采用 quad-aware 执行模型，以保留导数语义。  
  Fragment processing uses quad-aware execution to preserve derivative semantics.
- Depth、stencil、blending 在 tile 内按局部顺序执行，再提交回 backing memory。  
  Depth, stencil, and blending execute in tile-local order before committing results to backing memory.

### 选定的光栅模型 / Chosen Raster Model
采用 **tile-based + quad-aware** 的图形执行模型。  
Use a **tile-based + quad-aware** graphics execution model.

### 近期落地策略 / Near-Term Raster Bring-up Strategy
在长期目标保持 **tile-based + quad-aware** 不变的前提下，近期 bring-up 不直接移植 `src/Device/SetupProcessor.cpp`、`src/Device/QuadRasterizer.cpp`、`src/Device/PixelProcessor.cpp` 的 CPU 实现，也不采用 `nvdiffrast` 一类外部 raster 作为主路径。  
While the long-term target remains **tile-based + quad-aware**, the near-term bring-up should not directly transplant the CPU implementation in `src/Device/SetupProcessor.cpp`, `src/Device/QuadRasterizer.cpp`, and `src/Device/PixelProcessor.cpp`, nor should it adopt an external raster such as `nvdiffrast` as the main path.

近期应采用 **简单自研 CUDA raster**：先以单三角形、单 render target、`bbox + edge function` 覆盖测试打通最小链路，再在这个基础上逐步演进到 tile/binning、quad-aware fragment feed 和更完整的 attachment 语义。  
The near-term implementation should use a **simple in-house CUDA raster**: first bring up the minimal path with a single triangle, one render target, and `bbox + edge function` coverage testing, then evolve it toward tile/binning, quad-aware fragment feed, and richer attachment semantics.

CPU 路径应继续作为 correctness oracle，通过专门的 CPU 参考测试与 GPU 专有测试逐步对齐功能与精度；早期允许为未来字段预留 `stub` / `dummy` 接口，但每步都必须通过失败测试和对齐验证推进。  
The CPU path should remain the correctness oracle, with functionality and precision aligned incrementally through dedicated CPU reference tests and GPU-specific tests; early bring-up may leave `stub` / `dummy` interfaces for future fields, but every step must advance through failing tests and alignment checks.

### 选择原因 / Why This Model
- 比当前 CPU renderer 更符合 GPU 风格并行。  
  It matches GPU-style parallelism better than the current CPU renderer.
- 更容易控制 Vulkan 相关的顺序、导数与 attachment 语义。  
  It provides a controllable place to preserve Vulkan-relevant ordering, derivative, and attachment semantics.
- 比高度乱序的 immediate-mode kernel 方案更适合作为 correctness-first 的 bring-up 基础。  
  It is a better correctness-first bring-up foundation than a highly unordered immediate-mode kernel design.

## 已确认 Shader 编译策略 / Confirmed Shader Compilation Strategy
### 总体形态 / Required High-Level Shape
采用类似 TVM 的多 codegen 架构：  
Adopt a TVM-like multi-codegen architecture:
- `SPIR-V -> 后端无关内部 IR / backend-neutral internal IR`
- `内部 IR -> CUDA 风格源码 / internal IR -> CUDA-like source`
- `内部 IR -> LLVM IR / internal IR -> LLVM IR`
- 两条 codegen 路径必须共享同一套后端 ABI 与资源模型。  
  Both codegen paths must target the same backend ABI and resource model.

### 近期主路径 / Near-Term Path
先采用源码生成路径：  
Use source generation first:
- `SPIR-V -> 内部 IR -> CUDA 风格源码 / SPIR-V -> internal IR -> CUDA-like source`
- 将生成结果交给外部编译器/中端。  
  Hand the generated source to the external compiler or middle-end.

### 需要保留的并行路径 / Secondary Path To Preserve
同时保留 LLVM 路径：  
Keep a parallel LLVM path:
- `SPIR-V -> 内部 IR -> LLVM IR / SPIR-V -> internal IR -> LLVM IR`
- 将 LLVM IR 交给外部编译器/中端。  
  Hand LLVM IR to the external compiler or middle-end.

### 关键架构约束 / Required Architectural Constraint
两条路径必须共享同一个 lowering contract：  
Both paths must coexist behind the same lowering contract:
- 一个 SPIR-V 语义前端 / one SPIR-V semantic frontend
- 一份共享内部 IR / one shared internal IR
- 多个 codegen 目标 / multiple codegen targets
- 相同的 resource binding、stage ABI 与 launch contract / identical resource binding, stage ABI, and launch contract

### 选择原因 / Why This Direction
- 源码生成最利于早期 bring-up、调试和新特性试验。  
  Source generation is best for early bring-up, debugging, and feature experimentation.
- LLVM IR 路径保留长期优化空间和更强的编译器控制能力。  
  LLVM IR keeps room for long-term optimization and tighter compiler control.
- 共享 IR 可以避免演化出两套不兼容的 shader compiler。  
  A shared IR prevents the project from growing two incompatible shader compilers.
- 同一结构未来还能支持调试导出、文本检查和更多后端实验。  
  The same structure can later support debug dumps, textual inspection, and backend experimentation.

### 明确非目标 / Explicit Non-Goal
不把 Reactor 继续演化为自研 GPU 的主 codegen 基座；应改为新的后端无关 lowering 层。  
Do not evolve Reactor into the main codegen substrate for the custom GPU; replace it with a new backend-neutral lowering layer.

## 已确认运行时适配策略 / Confirmed Runtime Adaptation Strategy
### 初始运行时契约 / Initial Runtime Contract
第一阶段后端集成先对接 CUDA 兼容 runtime 接口，用于 kernel launch、内存分配、数据拷贝、同步和 module 管理。它只是 bring-up 契约，不是最终硬件抽象边界。  
The first backend integration should target a CUDA-compatible runtime interface for kernel launch, memory allocation, copies, synchronization, and module management. This is a bring-up contract, not the final hardware abstraction boundary.

### 可替换性要求 / Replacement Requirement
该 runtime 契约必须被封装在 backend adapter 之后，以便后续替换成更底层的 native driver interface，而不影响 Vulkan 命令处理、描述符语义和共享的 shader lowering 流程。  
The runtime contract must be wrapped behind a backend adapter so a future lower-level native driver interface can replace it without changing Vulkan command processing, descriptor semantics, or the shared shader lowering pipeline.

### Shader 构建模式 / Shader Build Modes
从第一天起支持两种模式：  
Support both modes from the beginning:
- JIT 编译，用于快速迭代和 pipeline bring-up。  
  JIT compilation for fast iteration and pipeline bring-up.
- 离线编译，用于部署、降低启动时延和做编译器校验。  
  Offline compilation for deployment, startup latency reduction, and compiler validation.

### 架构影响 / Architectural Consequence
Pipeline 创建必须拆分为以下阶段：  
Pipeline creation must be split into the following stages:
- 语义层 pipeline 构建 / semantic pipeline construction
- shader lowering 到内部 IR / shader lowering to internal IR
- codegen 到 CUDA 风格源码或 LLVM IR / codegen to CUDA-like source or LLVM IR
- runtime module 加载与 executable 绑定 / runtime module loading and executable binding

这样可以避免把 runtime 或 compiler 假设硬编码进 Vulkan pipeline 对象。  
This avoids baking runtime or compiler assumptions directly into Vulkan pipeline objects.

## 已确认内部 IR 结构与 Shader ABI / Confirmed Internal IR Shape and Shader ABI
### 双层 IR 结构 / Two-Layer IR Structure
采用两层 IR，而不是一层：`SPIR-V -> SemanticIR -> KernelIR -> {CUDA 风格源码 | LLVM IR}`。`SemanticIR` 负责表达 shader 语义本身，`KernelIR` 负责把这些语义包装成可执行的 stage entry、线程映射和运行时 ABI。这样可以避免将图形语义与后端执行细节硬绑定。  
Use two IR layers instead of one: `SPIR-V -> SemanticIR -> KernelIR -> {CUDA-like Source | LLVM IR}`. `SemanticIR` expresses shader semantics, while `KernelIR` wraps them into executable stage entries, thread mapping, and runtime ABI. This prevents hard-coupling graphics semantics with backend execution details.

### `SemanticIR` 内容 / `SemanticIR` Contents
`SemanticIR` 应采用强类型、SSA 风格和显式基本块 CFG。它应覆盖算术、转换、控制流、复合类型、资源访问、image/sampler、atomic、barrier、subgroup/quad、builtin I/O 等指令类别，并带有显式地址空间，例如 `Uniform`、`PushConstant`、`Storage`、`Workgroup`、`Private`、`Input`、`Output`、`Attachment`。此外，它还应保存 stage metadata，如 entry point、execution mode、插值模式、sample shading、early/late tests、depth write、discard/demote。  
`SemanticIR` should be strongly typed, SSA-style, and use explicit basic-block CFG. It should cover arithmetic, conversion, control flow, composite types, resource access, image/sampler, atomics, barriers, subgroup/quad, and builtin I/O, with explicit address spaces such as `Uniform`, `PushConstant`, `Storage`, `Workgroup`, `Private`, `Input`, `Output`, and `Attachment`. It should also carry stage metadata such as entry point, execution mode, interpolation mode, sample shading, early/late tests, depth write, and discard/demote.

在资源访问建模上，`SemanticIR` 必须显式区分 combined image sampler、separate image、separate sampler、storage image 和 texel buffer 访问，并把 NonUniform 资源索引作为显式语义保留下来，而不是在后端 codegen 时临时推断。这样才能无歧义地支持 descriptor indexing、storage image 读写和不同采样路径。  
For resource-access modeling, `SemanticIR` must explicitly distinguish combined image samplers, separate images, separate samplers, storage images, and texel-buffer access, and it must preserve NonUniform resource indexing as explicit semantics instead of inferring it during backend codegen. This is necessary to support descriptor indexing, storage-image read/write behavior, and distinct sampling paths without ambiguity.

### `KernelIR` 职责 / `KernelIR` Responsibilities
`KernelIR` 负责线程/块/warp 映射，以及 tile、quad、helper lane 的形成规则；负责资源表加载与参数解包，把 Vulkan 资源绑定转换成后端可执行参数；负责 stage wrapper，把 `SemanticIR` 主体包装成可启动的 vertex / fragment / compute kernel；同时定义 fixed-function 交界面，例如 fragment 输入的 barycentrics、coverage、sample mask、attachment handles。  
`KernelIR` owns thread/block/warp mapping, along with tile, quad, and helper-lane formation rules. It handles resource-table loading and parameter unpacking, converting Vulkan bindings into executable backend parameters. It also defines stage wrappers that package the `SemanticIR` body into launchable vertex, fragment, and compute kernels, and it defines the fixed-function boundary such as fragment barycentrics, coverage, sample mask, and attachment handles.

对 fragment 阶段，`KernelIR` 必须把 2x2 quad 常驻视为执行不变量：当 shader 需要导数、LOD 或 helper 相关语义时，同一 quad 的四个 lane 必须被共同调度；helper lane 必须执行并参与导数/采样计算，但不得提交 color、depth、stencil 或 image/store 副作用；`discard` / `demote` 只能改变导出掩码，不能破坏 quad 身份或让导数语义失效。tile dispatch、lane packing 和 ABI 都要显式保留 quad 身份。  
For fragment execution, `KernelIR` must treat 2x2 quad residency as an execution invariant: when the shader requires derivative, LOD, or helper-related semantics, the four lanes of a quad must be scheduled together; helper lanes must execute and participate in derivative and sampling calculations, but they must not commit color, depth, stencil, or image/store side effects; `discard` / `demote` may only change export masks and must not destroy quad identity or invalidate derivative semantics. Tile dispatch, lane packing, and the ABI must all preserve quad identity explicitly.

### Stage Kernel 粒度 / Stage Kernel Granularity
图形流水不应以单个 mega-kernel 作为主架构。跨 stage 的语义边界必须保留为独立 kernel：`vs`、`tcs`、`tes`、`gs`、primitive setup/binning/raster、`fs`、以及必要时的 `blend/resolve/present` 都应分阶段调度。这样才能正确表达 primitive amplification、patch/primitive 级缓冲、quad/helper-lane 约束、资源可见性和阶段间同步。  
The graphics pipeline should not use a single mega-kernel as its primary architecture. Cross-stage semantic boundaries must remain separate kernels: `vs`, `tcs`, `tes`, `gs`, primitive setup/binning/raster, `fs`, and when needed `blend/resolve/present` should all be scheduled as distinct stages. This is necessary to model primitive amplification, patch- and primitive-level buffering, quad/helper-lane constraints, resource visibility, and stage-to-stage synchronization correctly.

单个 stage 内部则采用“固定 `__global__` wrapper + 可编程 `__device__` shader body”的代码形态。wrapper 负责线程映射、vertex/fragment 输入解包、builtin 装配、descriptor/push constant 访问和输出写回；shader body 只承载该 stage 的可编程语义，并由 codegen 在编译期拼接，交由编译器决定是否内联。项目不应依赖运行时 device function pointer 或跨 stage hook 机制。  
Inside a single stage, use a “fixed `__global__` wrapper + programmable `__device__` shader body” structure. The wrapper owns thread mapping, vertex/fragment input unpacking, builtin assembly, descriptor/push-constant access, and output writeback; the shader body contains only the programmable semantics of that stage, is composed by codegen at compile time, and is left to the compiler for inlining decisions. The project should not depend on runtime device-function pointers or cross-stage hook mechanisms.

mega-kernel 仅可作为非常局部的实验优化，而不是默认设计。对于图形 bring-up 和长期维护来说，它会放大寄存器压力、降低 occupancy、扩大编译缓存粒度，并使 `tcs/tes/gs` 这类放大阶段、raster/fragment 边界和调试定位都变得更困难。默认原则应为：跨 stage 独立 kernel，stage 内再追求局部内联。  
A mega-kernel may exist only as a narrowly scoped experiment, not as the default design. For graphics bring-up and long-term maintenance, it increases register pressure, reduces occupancy, widens compilation-cache granularity, and makes amplification stages such as `tcs/tes/gs`, raster/fragment boundaries, and debugging substantially harder. The default rule should be: separate kernels across stages, then pursue local inlining within each stage.

### 统一 ABI 设计 / Unified ABI Design
所有 codegen 路径共享一套 POD 风格 ABI，避免依赖 C++ 对象或宿主指针语义。公共参数头应包含 pipeline 常量、specialization constants、push constants、descriptor table、dynamic offsets、draw/dispatch 参数。stage 专用参数则按 vertex、fragment、compute 分别建模，例如 vertex fetch 描述、tile 描述、primitive slice、attachment/depth-stencil 描述和 shared memory 布局。  
All codegen paths must share a POD-style ABI, avoiding C++ object or host-pointer semantics. The common parameter header should contain pipeline constants, specialization constants, push constants, descriptor tables, dynamic offsets, and draw/dispatch parameters. Stage-specific parameters should be modeled separately for vertex, fragment, and compute, such as vertex-fetch descriptors, tile descriptors, primitive slices, attachment/depth-stencil descriptors, and shared-memory layout.

为了防止 CUDA 风格源码路径和 LLVM IR 路径逐渐漂移，项目需要定义一套规范化 ABI 描述，并在测试或 CI 中对同一份 `KernelIR` 分别执行两条 codegen 路径，再比较归一化后的 ABI header、参数布局和 stage 参数元数据是否一致。共享 ABI 不是口头约束，而是需要持续验证的契约。  
To prevent the CUDA-like source path and the LLVM IR path from drifting apart over time, the project should define a normalized ABI description and, in tests or CI, run both codegen paths on the same `KernelIR` and compare the normalized ABI header, parameter layout, and stage-parameter metadata for equivalence. Shared ABI is not just a stated constraint; it is a contract that must be continuously verified.

### 关键原则 / Key Principles
`SemanticIR` 不得出现后端专有类型；`KernelIR` 可以绑定运行时模型，但不能向 Vulkan 前端泄漏；源码生成与 LLVM IR 生成必须共享同一份 ABI 与资源模型；不能继续保留多套“`SPIR-V -> 直接生成后端代码`”的分叉路径。  
`SemanticIR` must not contain backend-specific types. `KernelIR` may bind to the runtime model, but must not leak into the Vulkan frontend. Source generation and LLVM IR generation must share the same ABI and resource model. The project must not keep multiple divergent paths of `SPIR-V -> directly emit backend code`.

### 建议的代码形态 / Recommended Code Shape
建议新增 `ShaderSemanticIR`、`KernelIR`、`KernelABI` 和 `CodegenTarget::{CudaLikeSource, LlvmIR}`。现有 `SpirvShader` 则改造成三段式流程：`SpirvShader -> SemanticIR builder -> KernelIR lowerer -> codegen backend`。  
Add `ShaderSemanticIR`, `KernelIR`, `KernelABI`, and `CodegenTarget::{CudaLikeSource, LlvmIR}`. The current `SpirvShader` should evolve into a three-stage flow: `SpirvShader -> SemanticIR builder -> KernelIR lowerer -> codegen backend`.

## 已确认内存模型、Barrier 与同步 / Confirmed Memory Model, Barriers, and Synchronization
### 资源分层 / Split Resource Semantics from Backend Allocation
Vulkan 前端继续保留 `VkDeviceMemory`、`VkBuffer`、`VkImage` 的对象语义；后端新增 `BackendMemory`、`BackendBuffer`、`BackendImage`，专门负责 runtime 分配、地址、对齐、导入导出和 presentable backing。这样后续从 CUDA 兼容 runtime 切到 native driver 时，不需要重写 Vulkan 对象模型。  
Keep Vulkan object semantics in the frontend for `VkDeviceMemory`, `VkBuffer`, and `VkImage`, while adding `BackendMemory`, `BackendBuffer`, and `BackendImage` for runtime allocation, addressing, alignment, import/export, and presentable backing. This allows the runtime to switch from a CUDA-compatible layer to a native driver later without rewriting the Vulkan object model.

### Layout 作为逻辑状态 / Treat Layout as Logical State
Vulkan 的 `image layout`、`access mask` 和 `pipeline stage` 必须继续完整跟踪，但后端不要求存在一一对应的物理 layout。对后端来说，layout 更像是资源可见性、压缩状态、tile cache 状态以及采样/渲染权限的组合；真正需要执行的动作由 barrier lowering 决定。  
Vulkan `image layout`, `access mask`, and `pipeline stage` must still be fully tracked, but the backend does not need a one-to-one physical layout. For the backend, layout is better treated as a combination of visibility, compression state, tile-cache state, and sampling/render permissions; the actual actions are decided by barrier lowering.

### Barrier Lowering 规则 / Barrier Lowering Rules
每个资源子范围都维护状态：最近写者、可见性范围、layout、queue ownership 和 tile/cache dirty 状态。`vkCmdPipelineBarrier2`、render pass 依赖和 subpass 依赖最终统一 lowering 为资源状态转移加执行依赖，例如 flush / invalidate tile 或 backend cache、resolve 或提交 attachment 内容、插入 queue/stream 级依赖，以及必要时的 ownership transfer。关键点是 barrier 是语义边界，而不是固定映射到某一个 runtime API。  
Each resource subrange tracks state such as last writer, visibility scope, layout, queue ownership, and tile/cache dirty state. `vkCmdPipelineBarrier2`, render-pass dependencies, and subpass dependencies should all lower into resource-state transitions plus execution dependencies, such as flushing or invalidating tile/backend caches, resolving or committing attachment contents, inserting queue/stream-level dependencies, and transferring ownership when needed. The key point is that a barrier is a semantic boundary, not a fixed mapping to a single runtime API call.

### 统一同步时间线 / Unified Synchronization Timeline
`Fence` 表示可被 host 观察的 submission 完成；`Binary Semaphore` 在内部可映射为时间线上的一次性点；`Timeline Semaphore` 应直接映射到后端 timeline counter；`Event` 若底层不原生支持，可先在命令处理器中模拟。整体上，所有同步对象都应统一收敛到一个后端 timeline/dependency 框架。  
A `Fence` represents submission completion observable by the host; a `Binary Semaphore` can map internally to a one-shot point on a timeline; a `Timeline Semaphore` should map directly to a backend timeline counter; and an `Event` can initially be emulated in the command processor if the backend does not support it natively. Overall, all synchronization objects should converge to one backend timeline/dependency framework.

### Host 可见内存语义 / Host-Visible Memory Semantics
必须显式区分 `HOST_VISIBLE | HOST_COHERENT` 与非 coherent 的 `HOST_VISIBLE` 内存，并将 `vkFlushMappedMemoryRanges` / `vkInvalidateMappedMemoryRanges` 纳入统一的可见性模型。即便初期 runtime 比较简单，也不应省略这套语义层 bookkeeping。  
The implementation must explicitly distinguish `HOST_VISIBLE | HOST_COHERENT` memory from non-coherent `HOST_VISIBLE` memory, and it must integrate `vkFlushMappedMemoryRanges` / `vkInvalidateMappedMemoryRanges` into the unified visibility model. Even if the initial runtime is simple, this semantic bookkeeping should not be skipped.

### 已确认结论 / Confirmed Conclusion
前端负责 Vulkan 同步语义，后端负责执行与缓存动作；layout 是逻辑状态，barrier lowering 才决定实际 flush、wait 和 transition；所有同步对象最终统一映射到一个后端 timeline/dependency 框架。  
The frontend owns Vulkan synchronization semantics, while the backend owns execution and cache actions; layouts are logical state, and barrier lowering decides real flush, wait, and transition actions; all synchronization objects ultimately map to one backend timeline/dependency framework.


## 已确认 WSI 与 Present 细节 / Confirmed WSI and Present Details
### 保留抽象边界，替换底层实现 / Keep Abstractions, Replace the Internals
继续保留 `src/WSI` 中 `SurfaceKHR`、`SwapchainKHR`、`PresentImage` 等对象边界，但 `PresentImage` 不再默认是 CPU 可直接写入的 image，而应绑定到可显示的 backend image。`src/WSI` 负责 Vulkan WSI 语义，底层显示分配、展示提交和平台集成下沉到 backend present adapter。  
Keep object boundaries such as `SurfaceKHR`, `SwapchainKHR`, and `PresentImage` in `src/WSI`, but `PresentImage` should no longer assume a CPU-writable image and instead bind to a displayable backend image. `src/WSI` remains responsible for Vulkan WSI semantics, while display allocation, presentation submission, and platform integration move into a backend present adapter.

### Swapchain Image 双模式 / Two Swapchain Image Modes
需要支持两类 swapchain image：其一是原生可展示 image，即 swapchain image 本身可被显示控制器或窗口系统直接消费；其二是可渲染 image 加 present 前 copy/blit，即先渲染到 renderable image，再在 present 前复制或转换到 display image。建议两种都支持，但以第二种作为更稳妥的通用 fallback。  
Support two kinds of swapchain images: a native presentable image that can be consumed directly by the display controller or window system, and a renderable image plus present-time copy/blit, where rendering happens into a renderable image and is copied or transformed into a display image before present. Support both, but use the second as the safer general fallback.

原生 displayable-image 快路径只能在若干条件同时满足时启用：后端 image 的格式、tiling、sample count、对齐与布局能被显示链路直接接受；present 不需要额外 colorspace 转换或 resolve；GPU 或平台具备稳定的 image import/export / 共享句柄 / display engine 接入能力；同步和所有权转移能在不额外复制的前提下正确表达。只要任何一个条件不满足，就应回退到 renderable image + present copy/blit 路径。  
The native displayable-image fast path should only be enabled when several conditions are simultaneously satisfied: the backend image format, tiling, sample count, alignment, and layout are directly accepted by the display path; present does not require extra colorspace conversion or resolve; the GPU or platform provides stable image import/export, shared-handle, or display-engine integration; and synchronization plus ownership transfer can be expressed correctly without extra copying. If any of these conditions are not met, the implementation should fall back to the renderable-image plus present copy/blit path.

### Acquire / Present 流程 / Acquire and Present Flow
`vkAcquireNextImageKHR` 负责返回可写 image slot 及其 acquire sync point；图形队列渲染到该 image 或中间 renderable image；`vkQueuePresentKHR` 应 lowering 成 backend present job：等待渲染完成，必要时做 resolve、copy 或 colorspace transform，然后提交到平台显示队列；present 完成后 image 回到可 acquire 状态，并更新 swapchain 生命周期。  
`vkAcquireNextImageKHR` returns a writable image slot and its acquire sync point. The graphics queue renders into that image or an intermediate renderable image. `vkQueuePresentKHR` should lower into a backend present job that waits for rendering to finish, performs resolve, copy, or colorspace transform when necessary, and then submits to the platform display queue. After present completes, the image returns to the acquirable state and the swapchain lifecycle is updated.

### 平台无关 Present Adapter / Platform-Neutral Present Adapter
建议新增 `BackendPresentAdapter` 抽象，用于屏蔽 Win32、X11、Wayland、Android 等平台差异。它至少需要负责 displayable image 创建、image import/export 或共享句柄管理、vsync 与 present mode 映射，以及窗口 resize、suboptimal、out-of-date 等情况。  
Add a `BackendPresentAdapter` abstraction to hide differences across Win32, X11, Wayland, Android, and similar platforms. At minimum it should handle displayable-image creation, image import/export or shared-handle management, vsync and present-mode mapping, and resize, suboptimal, and out-of-date situations.

### Present 与同步边界 / Boundary Between Present and Synchronization
`Acquire` 与 `Present` 必须接入统一的 timeline/dependency 框架。Present 不是简单的“显示一下内存”，而是具有资源所有权、可见性和队列依赖语义的操作；因此 present 前后的 layout、queue family ownership、semaphore wait/signal 都必须纳入统一状态机。  
`Acquire` and `Present` must plug into the unified timeline/dependency framework. Present is not merely “show this memory”; it is an operation with resource ownership, visibility, and queue-dependency semantics. Therefore, layout, queue-family ownership, and semaphore wait/signal behavior around present must all be handled by the unified state machine.

### 已确认结论 / Confirmed Conclusion
保留现有 WSI 抽象并新增 backend present adapter；优先支持“renderable image + present copy/blit”，同时为原生 displayable image 预留快路径；将 present 完整纳入统一同步和资源状态管理，而不是做独立旁路。  
Keep the current WSI abstractions and add a backend present adapter; prioritize “renderable image + present copy/blit” while reserving a fast path for native displayable images; and make present part of the unified synchronization and resource-state management rather than a separate side path.

## 已确认验证策略与分阶段落地计划 / Confirmed Validation and Phased Rollout Plan
### 验证策略总则 / Validation Principles
验证必须采用“语义正确性优先、性能优化后置”的策略。最早期阶段应把现有 SwiftShader CPU 路径视为 correctness oracle，用于对比 draw、attachment、barrier、shader 输出和错误码语义；在此基础上再逐步引入针对自研 GPU 后端的性能与稳定性测试。  
Validation must follow a “correctness first, performance second” strategy. In the earliest phase, the existing SwiftShader CPU path should be treated as a correctness oracle for comparing draw behavior, attachments, barriers, shader outputs, and error-code semantics. Performance and stability testing for the custom GPU backend should be added incrementally afterward.

### 测试金字塔 / Test Pyramid
测试分为四层：其一是 IR 和 codegen 单元测试，用于验证 `SpirvShader -> SemanticIR -> KernelIR -> {CUDA-like source | LLVM IR}` 的 lowering 结果；其二是 backend 单元测试，用于验证 memory、synchronization、resource state 和 runtime adapter；其三是 Vulkan 单元测试，优先复用 `tests/VulkanUnitTests/` 并扩展新后端覆盖；其四是回归和 conformance 测试，逐步接入 `tests/regres/` 和 dEQP。  
Testing should have four layers: IR and codegen unit tests to verify `SpirvShader -> SemanticIR -> KernelIR -> {CUDA-like source | LLVM IR}` lowering; backend unit tests for memory, synchronization, resource state, and the runtime adapter; Vulkan unit tests by reusing and extending `tests/VulkanUnitTests/`; and regression and conformance testing through `tests/regres/` and eventually dEQP.

### 分阶段落地 / Phased Rollout
建议分五阶段推进：  
Use five rollout phases:
- **阶段 0 / Phase 0**：抽象重构。先从 `src/Vulkan` 到执行层之间引入 backend-neutral 接口，不改变现有 CPU 路径行为。  
  **Phase 0**: Refactor abstractions. Introduce backend-neutral interfaces between `src/Vulkan` and the execution layer without changing current CPU-path behavior.
- **阶段 1 / Phase 1**：打通 compute 与基础 runtime。完成 backend memory、queue、module、kernel launch，以及 `SPIR-V -> IR -> CUDA-like source` 的最小链路。  
  **Phase 1**: Bring up compute and the basic runtime. Implement backend memory, queue, module, kernel launch, and a minimal `SPIR-V -> IR -> CUDA-like source` path.
- **阶段 2 / Phase 2**：打通 graphics 最小闭环。支持 vertex、primitive setup、tile raster、简单 fragment、color attachment、基础 barrier 和离屏 rendering。  
  **Phase 2**: Bring up the minimal graphics loop. Support vertex, primitive setup, tile raster, simple fragment, color attachments, basic barriers, and offscreen rendering.
- **阶段 3 / Phase 3**：接入 WSI / Swapchain / Present。实现 acquire/present、displayable image 或 present copy path、窗口 resize 和基本 present modes。  
  **Phase 3**: Integrate WSI / Swapchain / Present. Implement acquire/present, displayable images or present-copy paths, resize handling, and basic present modes.
- **阶段 4 / Phase 4**：功能补全与 conformance 收敛。补 depth/stencil、blend、MSAA、复杂 barrier、timeline semaphore、更多格式与 image path，并开始系统化 dEQP 收敛。  
  **Phase 4**: Feature completion and conformance convergence. Add depth/stencil, blend, MSAA, complex barriers, timeline semaphores, more formats and image paths, and begin systematic dEQP convergence.

### 编译链分阶段策略 / Staged Compiler Strategy
在 rollout 上，代码生成也应分阶段：第一阶段以 CUDA 风格源码生成为主，优先追求 bring-up 与可调试性；第二阶段开始引入 LLVM IR 路径，并确保二者共享相同 IR 和 ABI；长期再基于 profile 与热点分析决定哪些阶段优先切到 LLVM IR。  
Code generation should also be staged: start with CUDA-like source generation to optimize for bring-up and debuggability; introduce the LLVM IR path in the second stage while keeping both paths on the same IR and ABI; then use profiling and hotspot analysis to decide which stages should migrate to LLVM IR first.

### 发布与回退策略 / Release and Rollback Strategy
后端切换应由显式 build flag 或 runtime selection 控制，保证 CPU 路径始终可作为调试和回退选项。这样可以降低大规模替换 `src/Device` 风险，并便于在同一套 Vulkan 前端下比较 CPU backend 与 custom GPU backend 的行为差异。  
Backend selection should be controlled by explicit build flags or runtime selection, keeping the CPU path available as a debug and rollback option. This reduces the risk of replacing `src/Device` at scale and makes it easier to compare behavior between the CPU backend and the custom-GPU backend under the same Vulkan frontend.

### 已确认结论 / Confirmed Conclusion
项目应按“先抽象、再 compute、再最小 graphics、再 WSI/present、最后 conformance 收敛”的顺序推进；验证体系必须覆盖 IR、backend、Vulkan 单测和回归/一致性测试；CPU 路径在较长时间内都应保留作为 correctness oracle 与回退通道。  
The project should proceed in the order of abstraction first, then compute, then minimal graphics, then WSI/present, and finally conformance convergence. Validation must cover IR, backend, Vulkan unit tests, and regression/conformance testing. The CPU path should remain available for a long time as both a correctness oracle and a rollback path.


## 待补充章节 / Pending Sections


## 实施状态 / Implementation Status
当前仓库中的 bootstrap 已落地以下基础设施：backend 构建骨架、队列执行缝、独立 backend 单测目标、`SemanticIR` / `KernelIR` / `KernelABI`、`SemanticIRBuilder`、双路径文本 emitter、runtime/fake runtime、compute backend executable、resource state tracker、graphics backend stub、fallback present adapter，以及 bring-up 文档与 smoke tests。  
The current repository bootstrap now includes the following infrastructure: backend build skeleton, queue execution seam, a dedicated backend unit-test target, `SemanticIR` / `KernelIR` / `KernelABI`, `SemanticIRBuilder`, dual-path text emitters, runtime/fake runtime, compute backend executable, resource state tracker, graphics backend stub, fallback present adapter, and bring-up documentation plus smoke tests.
