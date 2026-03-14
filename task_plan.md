# Task Plan: GPU graphics execution refactor

## Goal
沿着“backend-owned graphics execution”主线持续收口：前几刀已经把 draw 路由收进 `ExecutionBackend`、让 `Renderer::draw()` 变成 CPU-only，并把 triangle bootstrap 的 shader-only 与 texture metadata 逐步迁进 `GraphicsExecutable`；当前重心继续沿 sampled-image resource plan 推进，一方面用真实 sample-use provenance 收口 plan 输入，另一方面开始让 separate image/sampler 真正驱动 strict GPU draw。

## Current Phase
Phase 26: Vkcube-like derivative texture fragment support (complete)

## Phases
### Phase 1: Discovery
- [x] 重新梳理 `VkCommandBuffer -> Renderer -> DrawCall` 的现有 draw 链路
- [x] 确认 GPU draw 目前只是 `Renderer::draw()` 内的 bootstrap 旁路
- [x] 明确第一刀不直接做完整 `GraphicsExecutable`
- **Status:** complete

### Phase 2: Design & Planning
- [x] 写图形执行重构设计文档
- [x] 写分任务实现计划（按 TDD 拆分）
- [x] 记录首个切片的边界与验收标准
- **Status:** complete

### Phase 3: Draw routing seam
- [x] 先写失败测试，覆盖 draw 路由策略/接口
- [x] 给 `ExecutionBackend` 增加 draw dispatch seam
- [x] 让 `CmdDrawBase` 通过 backend 发起 draw，而不是直接调 `renderer->draw()`
- **Status:** complete

### Phase 4: GPU bootstrap extraction
- [x] 把 triangle bootstrap 路由从 `Renderer::draw()` 抽到 backend helper
- [x] 让 `GpuExecutionBackend` 负责 GPU bootstrap / CPU fallback / strict abort 决策
- [x] 清掉 `Renderer::draw()` 中与 runtime/env 绑定的 GPU 分支
- **Status:** complete

### Phase 5: Verification & cleanup
- [x] 跑通针对性 backend / draw / Vulkan 测试
- [x] 更新 bring-up 文档中的 draw 架构说明
- [x] 记录剩余后续项：graphics executable、transfer/present/backend memory model
- **Status:** complete

### Phase 6: Graphics executable scaffold
- [x] 写 metadata-only `GraphicsExecutable` 设计与实现计划
- [x] 先写 backend / Vulkan 失败测试，锁住 stage 组合和 pipeline hookup
- [x] 给 `vk::GraphicsPipeline` 接入 backend executable 构建与生命周期
- [x] 跑通 focused backend / draw / Vulkan 验证
- **Status:** complete

### Phase 7: Graphics executable bootstrap metadata
- [x] 写 bootstrap-analysis 设计与实现计划
- [x] 先写失败测试，锁住 bootstrap point size / fragment template 提取行为
- [x] 让 `GraphicsExecutable` 持有 shader-only bootstrap metadata
- [x] 让 `TriangleBootstrapDraw` 改为消费 executable metadata，并保留 texture descriptor 的 draw-time 物化
- [x] 跑通 focused 和 broader backend / draw / Vulkan 验证
- **Status:** complete

### Phase 8: Graphics executable texture bootstrap metadata
- [x] 写 texture-bootstrap metadata 设计与实现计划
- [x] 先写失败测试，锁住 texture binding metadata 的默认值和 graphics pipeline 提取行为
- [x] 让 `GraphicsExecutable` 持有 texture bootstrap descriptor set / binding metadata
- [x] 让 `TriangleBootstrapDraw` 用 executable metadata 做 texture config 物化，并停止直接读取 fragment shader
- [x] 跑通 focused 和 broader backend / draw / Vulkan 验证
- **Status:** complete

### Phase 9: Graphics executable texture bootstrap narrowing
- [x] 写 texture-bootstrap narrowing 设计与实现计划
- [x] 先写失败测试，锁住 “location 0 不是 `vec2` texcoord” 时不得提取 texture binding metadata
- [x] 把 texture bootstrap metadata 的 narrow path 收紧为 `fragment location 0 == vec2`
- [x] 跑通 focused 和 broader backend / draw / Vulkan 验证
- **Status:** complete

### Phase 10: Graphics executable texture bootstrap layout validation
- [x] 写 texture-bootstrap layout-validation 设计与实现计划
- [x] 先写失败测试，锁住 layout 形状不匹配时不得误报 texture bootstrap binding metadata（含 descriptor array / descriptorCount 约束）
- [x] 让 `GraphicsExecutable` 在 texture metadata 提取时同时验证 layout 上的 descriptor type / descriptorCount
- [x] 跑通 focused 和 broader backend / draw / Vulkan 验证
- **Status:** complete

### Phase 11: Graphics executable texture bootstrap direct-sample validation
- [x] 写 texture-bootstrap direct-sample 设计与实现计划
- [x] 先写失败测试，锁住 sample 后又做额外片元运算的 shader 不得被识别为当前 texture bootstrap binding metadata
- [x] 把 texture bootstrap metadata 的 narrow path 收紧为 `location 0` 必须直接存储 texture sample 的结果
- [x] 跑通 focused 和 broader backend / draw / Vulkan 验证
- **Status:** complete

### Phase 12: Graphics executable texture bootstrap sample passthrough validation
- [x] 写 texture-bootstrap sample-passthrough 设计与实现计划
- [x] 先写失败测试，锁住“sample 结果经 trivial pass-through 再写回”仍应被识别为当前 texture bootstrap binding metadata
- [x] 把 texture bootstrap metadata 的 narrow path 扩为允许极小的 sample passthrough value chain
- [x] 跑通 focused 和 broader backend / draw / Vulkan 验证
- **Status:** complete

### Phase 13: Graphics executable texture bootstrap separate-image-sampler boundary
- [x] 写 separate-image-sampler boundary 设计与实现计划
- [x] 增加 Vulkan pipeline negative test，锁住 separate image/sampler 不得被识别为当前 texture bootstrap binding metadata
- [x] 更新跟踪文档，明确当前 narrow path 只覆盖 combined image sampler
- [x] 跑 focused Vulkan 验证
- **Status:** complete

### Phase 14: Graphics executable sampled-image resource plan
- [x] 写 sampled-image resource plan 设计与实现计划
- [x] 把 `GraphicsExecutable` 的 texture metadata 提升成显式 texture plan，区分 `CombinedImageSampler` / `SeparateImageSampler` / `Other`
- [x] 保留 `hasBootstrapTextureBinding()` 作为兼容入口，只对当前 supported combined-image-sampler path 返回 `true`
- [x] 扩 Vulkan pipeline tests，覆盖 combined / separate / multi-sampled-resource (`Other`) 三类形态
- [x] 跑 focused 与 broader backend / draw / Vulkan 验证
- **Status:** complete

### Phase 15: Graphics executable sampled-image provenance
- [x] 写 sampled-image provenance 设计与实现计划
- [x] 先写失败测试，锁住 unrelated non-sampled descriptors 不得把 sampled-image plan 降成 `Other`
- [x] 把 texture plan descriptor 收集改成真实 sample-use provenance，而不是 fragment shader 全量 descriptor decorations
- [x] 跑 focused 与 broader backend / draw / Vulkan 验证
- **Status:** complete

### Phase 16: Triangle bootstrap separate-image-sampler materialization
- [x] 写 separate-image-sampler materialization 设计与实现计划
- [x] 复现 strict GPU `DrawTest.TexturedTriangleSeparateImageSamplerNearest` 的失败
- [x] 让 narrow direct-sample `SeparateImageSampler` plan 标记为 bootstrap-supported
- [x] 让 `TriangleBootstrapDraw` 直接消费 richer texture plan，并物化 separate image/sampler 的 `Texture2DColor` config
- [x] 跑 focused 与 broader backend / draw / Vulkan 验证
- **Status:** complete

### Phase 17: Texture bootstrap descriptor-array indexing
- [x] 先写失败测试，锁住 combined/separate descriptor array 在常量 index 情况下应被识别为可 bootstrap 的 texture plan
- [x] 扩 `GraphicsExecutableTexturePlan`：记录 `imageArrayElement` / `samplerArrayElement`，并从 sample operand 的 access-chain 中提取常量 index
- [x] 扩 `TriangleBootstrapDraw`：按 array element + descriptor size 物化正确的 descriptor element
- [x] 增加 strict GPU bootstrap render 的 draw regressions：`DrawTest.TexturedTriangleDescriptorArrayIndexOneBootstrapNearest` / `DrawTest.TexturedTriangleSeparateImageSamplerDescriptorArrayIndexOneBootstrapNearest`
- [x] 跑 focused 与 broader backend / draw / Vulkan 验证
- **Status:** complete

### Phase 18: Image resource plan (sampled + storage)
- [x] 先写失败测试：fragment 含 `imageStore` 的 direct-sample shader 不得被识别为可 bootstrap 的 texture binding（避免 strict GPU triangle bootstrap 静默跳过 side-effect）
- [x] 给 `GraphicsExecutable` 增加 image resource plan：显式列出 fragment shader 的 sampled-image 相关 descriptors 与 storage image descriptors（含 array element）
- [x] 扩 Vulkan pipeline introspection/tests：验证 combined / separate / storage-image-write pipeline 的资源 plan 计数与 binding 位置
- [x] 跑 focused Vulkan / backend / draw 验证
- **Status:** complete

### Phase 19: General resource plan + capability gate (scaffold)
- [x] 写框架结论落盘：整体 GPU 迁移仍需补的模块面与建议里程碑（见 `docs/plans/2026-03-12-gpu-migration-framework-adjustments.md`）
- [x] 增加更通用的 `GraphicsExecutableResourcePlan`（先建模/暴露，不强行执行）：layout 轮廓 + descriptor refs（含 dynamic/array element）
- [x] 扩 pipeline introspection：覆盖至少 push constants / dynamic offsets / buffer descriptors 的 plan 轮廓（计数与关键 binding）
- [x] 增加 fragment feature mask（discard/storage-image/image-query/derivatives/atomics/subgroup）作为 capability gate 的输入，并在 pipeline/executable 创建期生成
- [x] 建立 capability/side-effect gate 的统一入口：输出可测试的 unsupported reason list，并用于 strict/fallback 决策
- **Status:** complete

### Phase 20: IR-based codegen migration evaluation
- [x] 落盘“从 nvcc 文本源码路径迁到 IR 的时机与可行性”评估（见 `docs/plans/2026-03-12-ir-codegen-migration-timing.md`）
- [x] 明确短期 hard gate：不得每 draw 编译 module；编译/缓存必须收敛到 pipeline-time 或等价层级（已实现 CUDA module cache，避免重复 nvcc 编译）
- [x] 评估并记录可行过渡：NVRTC/in-process compile vs. IR-consuming runtime（以 toolchain/依赖/调试性为约束）
- **Status:** complete

### Phase 21: Attachment lifecycle/state tracking (planning)
- [x] 写 offscreen color attachment lifecycle/state tracking 设计与实现计划（见 `docs/plans/2026-03-14-graphics-attachment-lifecycle-design.md` / `docs/plans/2026-03-14-graphics-attachment-lifecycle-implementation.md`）
- [x] 先写失败测试，锁住 clear/store/layout 的最小 backend-owned 闭环（先覆盖离屏 color attachment）
- [x] 把 strict GPU triangle bootstrap 的 color write-back 从 ad-hoc helper 推进到可跟踪的 attachment lifecycle scaffold
- [x] 扩 focused draw / Vulkan 验证，覆盖 render pass 与 dynamic rendering 下的 offscreen color attachment 路径
- **Status:** complete

### Phase 22: Compiler analysis module extraction (planning)
- [x] 写 compiler analysis 独立模块设计与实现计划（见 `docs/plans/2026-03-14-compiler-analysis-module-design.md` / `docs/plans/2026-03-14-compiler-analysis-module-implementation.md`）
- [x] 新增第一层 `SpirvToCompilerAnalysis` 单测，覆盖当前 graphics 路线上的 supported + gated shader 特性
- [x] 从 `GraphicsExecutable.cpp` 提取通用 compiler analysis 到 `src/Pipeline/ShaderCompilerAnalysis.*`
- [x] 让 `GraphicsExecutable` 改为消费独立 analysis 模块，并补第二层 `KernelIR` / 第三层 emitter-ABI parity 覆盖
- **Status:** complete

### Phase 23: Swapchain present lifecycle tracking
- [x] 把“方向 1 优先、方向 3 服务于方向 1、方向 2 仅做 focused 验收”的决策落入计划
- [x] 先写失败测试，锁住 `PresentAdapter/ResourceStateTracker` 对 swapchain image acquire/present 的逻辑 layout 语义
- [x] 让 `PresentAdapter` 记录有意义的 present-side logical layout，而不是一律写成 `GENERAL`
- [x] 扩 focused backend / Vulkan 验证，保证 swapchain present lifecycle contract 可观测
- **Status:** complete

### Phase 24: Command-buffer image barrier state tracking
- [x] 先写失败测试，锁住 `ResourceStateTracker` 对 `VkDependencyInfo` / image barrier 的 layout 跟踪
- [x] 让 `CmdPipelineBarrier` 真正持有 dependency info，并在 execute 时更新 `ExecutionState.resourceStateTracker`
- [x] 跑 focused backend / Vulkan / draw 验证，确认现有 dynamic rendering 路径未回归
- **Status:** complete

### Phase 25: Shared device resource state tracking
- [x] 先写失败测试，锁住 `ResourceStateTracker` copy 后的共享语义
- [x] 把 `ResourceStateTracker` 改成共享底层 state，并让 device / swapchain / execution state 消费同一份 tracker store
- [x] 跑 focused backend / Vulkan / draw 验证，确认 present capture 与 dynamic rendering 路径未回归
- **Status:** complete

### Phase 26: Vkcube-like derivative texture fragment support
- [x] 先写失败测试，锁住 `GraphicsBackendPipeline` 对 derivative-lit textured fragment 的 unsupported-reason 行为
- [x] 给 bootstrap fragment path 增加 `vkcube`-like derivative-lit texture shader kind，并让 executable gate 放行这一窄模式
- [x] 扩 strict GPU draw regression，验证该模式能通过当前 render-to-attachment 路径
- [x] 用真实 `vkcube` 做 focused smoke，确认 strict GPU render path 不再在 `TextureSamplingUnsupported, Derivatives` 上直接 abort
- **Status:** complete

## Decisions Made
| Decision | Rationale |
|----------|-----------|
| 第一刀先做 draw routing seam，不直接上完整 `GraphicsExecutable` | 能先修正最明显的错层问题，风险最低 |
| `Renderer::draw()` 目标形态是 CPU-only | CPU renderer 继续承担现有 CPU 语义，不再混 GPU 分支 |
| GPU triangle bootstrap 暂时保留，但降级为 backend-owned helper | 现阶段它仍是必要 bring-up/验证工具 |
| 继续保留 `SWIFTSHADER_GPU_ALLOW_CPU_FALLBACK` | 仍需要可控 bring-up/诊断开关，但语义中心不再放在 fallback |
| 当前 `GraphicsExecutable` 只做 metadata scaffold，不改变 draw 执行路径 | 先建立 pipeline-time backend ownership，避免把 graphics execution / resource model 一次性耦合进来 |
| `GraphicsExecutable` 先只吸收 shader-only bootstrap metadata，不吸 descriptor/materialized texture state | texture bootstrap 依赖 draw-time binding state，过早塞进 pipeline object 会错层 |
| texture bootstrap 只迁 descriptor set / binding metadata，不迁 texture bytes / sampler state | 保持 pipeline-time 与 draw-time 职责边界清晰，同时去掉 draw helper 对 fragment shader 的直接依赖 |
| 当前 texture bootstrap metadata 只接受 `fragment location 0 == vec2` 的窄路径 | 先避免 false-positive metadata，等后续有真正 varying 语义映射后再扩范围 |
| 当前 texture bootstrap metadata 仍要求 layout 上的 descriptor type 为 `COMBINED_IMAGE_SAMPLER`，并要求 descriptor-array index 为常量且 in-bounds | 通过把 array element 纳入 plan，descriptor array 不再需要一刀切拒绝；non-constant index 仍然保守拒绝 |
| 当前 texture bootstrap metadata 还要求 `location 0` 的输出直接来自 texture sample 结果 | 现有 `Texture2DColor` bootstrap 只能表达“直接采样并输出”，不能猜测 sample 后的额外片元运算 |
| 当前 texture bootstrap metadata 允许极小的 trivial passthrough（如 `OpLoad`/`OpCopyObject`）通向 texture sample | 保留 direct-sample 语义边界的同时，避免把语义等价的局部临时变量写法误判成 unsupported |
| texture bootstrap 一旦检测到 fragment shader 含 storage image read/write（`OpImageRead`/`OpImageWrite`），必须拒绝 bootstrapSupported | triangle bootstrap 不能模拟 storage image side-effect；必须避免 strict GPU bootstrap 静默错误 |
| `hasBootstrapTextureBinding()` 继续不支持 separate image/sampler | 保持 combined-only compatibility；separate image/sampler 通过 sampled-image plan + draw-time 物化支持 narrow direct-sample case |
| texture metadata 现在先建模 sampled-image resource plan，再从中派生 bootstrap support | 需要先把 combined / separate / other 资源方案建模清楚，避免继续把“资源形态”和“当前 bootstrap 可执行性”揉成一个布尔值 |
| sampled-image plan 的 descriptor 收集以真实 sample-use provenance 为准，而不是 fragment shader 的全量 descriptor decorations | non-sampled UBO / 其他 descriptor 不应污染 sampled-image plan 分类 |
| `TriangleBootstrapDraw` 现在直接消费 `GraphicsExecutableTexturePlan`，而不是只消费 combined-binding compatibility accessor | separate image/sampler 需要 richer plan 才能驱动 draw-time 物化；兼容 accessor 保留给仍只理解 combined 的调用方 |
| `GraphicsExecutable` 新增 image resource plan（sampled + storage），为后续更一般的 GPU 资源建模预留稳定入口 | 避免继续把“纹理 bootstrap”当成资源方案；sampled image 与 storage image 都必须有明确 plan 载体 |
| 当前三方向优先级：先做 framework refactor（方向 1），再做直接服务于 framework 的 compiler 语义扩展（方向 3），vkcts 仅做 focused 验收（方向 2） | 当前最大瓶颈是 graphics execution framework 仍主要停在 bootstrap helper；先扩大测试面只会得到大量结构性失败，先扩 compiler 也会被执行框架吞掉收益 |
| Direction 1 的下一刀选择 swapchain/present lifecycle tracking | `PresentAdapter + ResourceStateTracker` 已接入 `VkSwapchainKHR`，但 acquire/present 语义仍是空心的 `GENERAL`；这是纯框架改动，能继续推进 backend-owned graphics/present ownership |
| Direction 1 的第二刀选择 command-buffer image barrier state tracking | `ExecutionState.resourceStateTracker` 已存在，但 `CmdPipelineBarrier` 之前只是全管线同步，不携带/消费 barrier 数据；先把 image layout transition 纳入 tracker，比继续堆 metadata 更接近真正的 backend-owned execution state |
| Direction 1 的第三刀选择 shared device resource-state store | 仅有 barrier tracking 和 swapchain tracking 还不够；如果 tracker 仍按对象各自复制存储，状态不会跨 submit/present 连续。先把 tracker 做成共享底层 state，再把 device/swapchain/execution path 接到同一份 store，才有资格继续扩 queue/present/resource lifecycle |
| 当前为真实 Vulkan app bring-up，允许做“服务于 framework 的窄 shader 语义支持” | `vkcube` 当前 blocker 已从纯框架层转到一类真实 fragment 模式；对这类 `vkcube`-like shader 做窄支持，仍属于方向 1 的主线推进，而不是转去做泛化 compiler 扩张 |

## Key Questions
1. `GraphicsExecutable` 下一步要承载哪些真正执行期信息，而不是只停留在 metadata？
2. triangle bootstrap helper 如何自然演进到正式 graphics executable 入口？
3. 哪些路径仍会继续保留 CPU-only（copy/blit/resolve/present/resource model）？

## Errors Encountered
| Error | Attempt | Resolution |
|-------|---------|------------|
| None yet | - | - |

## Notes
- 旧的 CUDA 稳定化/compute bring-up 历史信息保存在 `progress.md` 和 git 历史中；`task_plan.md` 从本次开始切换为图形执行重构主线。
- 本轮首个验收不追求“完整 GPU graphics backend”，而是先把 draw 的调度层次改对。
- 当前切片已完成：draw dispatch 现在由 backend 直接接管，`Renderer::draw()` 收敛为 CPU-only。
- 当前第二个切片也已完成：`vk::GraphicsPipeline` 现在会在有 vertex stage 时构建 metadata-only `GraphicsExecutable`，但 draw 仍沿用现有 backend helper / CPU renderer 路径。
- 当前第三个切片也已完成：`GraphicsExecutable` 现在会在 pipeline 创建期提取 constant point size 和非 texture fragment bootstrap 模板，`TriangleBootstrapDraw` 不再自己重扫这些 shader-only 信息。
- 当前第四个切片也已完成：`GraphicsExecutable` 现在也会提取 narrow texture bootstrap 的 descriptor set / binding metadata，`TriangleBootstrapDraw` 已不再直接读取 fragment `SpirvShader`。
- 当前第五个切片也已完成：texture bootstrap metadata 的支持边界已显式收紧为 `fragment location 0 == vec2`；`location 1` 才是 texcoord 的 shader 不再被误判为当前 narrow path。
- 当前第六个切片也已完成：texture bootstrap metadata 现在也会校验 layout 形状；descriptor array 不再被误判为当前 narrow combined-image-sampler path。
- 当前新增切片也已完成：`GraphicsExecutableTexturePlan` 现在能提取 combined descriptor array 的常量 index，并在 strict GPU triangle bootstrap render 下按 array element 正确物化/采样。
- 当前第七个切片也已完成：texture bootstrap metadata 现在还要求 `location 0` 直接存储 texture sample 结果；sample 后再做乘法等片元运算的 shader 不再被误判为当前 narrow `Texture2DColor` path。
- 当前第八个切片也已完成：texture bootstrap metadata 现在允许极小的 sample passthrough value chain；`vec4 sampledColor = texture(...); outColor = sampledColor;` 这类语义等价 shader 不再被误判为 unsupported。
- 当前第九个切片也已完成：`GraphicsBackendPipeline` 现在显式覆盖 separate image/sampler negative case，锁住该 descriptor 形态继续留在当前 narrow texture-bootstrap path 之外。
- 2026-03-14 会话恢复已确认：Phase 19/20 对应实现仍在当前工作区，且 focused `backend-unittests` / `vk-unittests` / `draw-unittests` 全绿；之前未同步的主要问题是 `Current Phase` 指针仍停在旧的 Phase 19。
- 2026-03-14 compiler sidecar 新增收口：独立 `ShaderCompiler` 现已支持最小 vertex standalone compile 路径，`shader-compiler` tool 也已支持 `--stage vertex` 的 CUDA-like / LLVM IR skeleton 输出；该离线能力仍是架构预留和 smoke 验证，不改变主线 `GraphicsExecutable` 当前 phase。
- 2026-03-14 attachment lifecycle 切片已收口：`TriangleBootstrapDraw` 现已消费 `GraphicsDrawCall::colorAttachment0`，并显式 gate `present/storeOp/layout`；strict GPU draw 对 `storeOp = DONT_CARE` 的 render pass / dynamic rendering 两条路径都已由测试锁住。
- 2026-03-14 `ShaderCompiler` 目录下已补充 `TranslatorReference.md`，把 LLPC translator 与 MLIR SPIR-V -> LLVM 路线的可借鉴语义和能力边界落成实现参考，供后续“标准 SPIR-V -> normalized IR -> LLVM/backend”开发使用。
- 2026-03-14 主线方向已收口：下一阶段优先继续 framework refactor，不扩大为全面 vkcts 覆盖，也不把主力切到更广的 shader 语义扩展；compiler 工作只做直接解锁 framework 的部分。
- 2026-03-14 Direction 1 已启动：先从 `PresentAdapter + ResourceStateTracker + VkSwapchainKHR` 的 swapchain lifecycle contract 开刀，把 acquire/present 的逻辑 layout 变成可测试的 backend-owned 状态；后续更大的 framework 切口应继续推进 command-buffer barrier / execution path 对 resource state 的接入。
- 2026-03-14 Direction 1 第二刀已收口：`CmdPipelineBarrier` 现已持有 `vk::DependencyInfo` 并把 image barrier layout transition 写入 `ExecutionState.resourceStateTracker`；后续更大的框架切口可继续扩到 wait-events / queue submit / present path 的统一 state model。
- 2026-03-14 Direction 1 第三刀已收口：`ResourceStateTracker` 现已共享底层 state，`vk::Device`、`VkSwapchainKHR`、以及 submit-time `ExecutionState` 都开始接到同一个 tracker store；这样 acquire/present 与 command-buffer barrier 终于不会各记各的状态。
- 2026-03-14 已开始面向真实 Vulkan app (`vkcube`) 反推最小缺口：在保持 framework 主线不偏航的前提下，补了一类 `vkcube`-like derivative-lit texture fragment 窄支持；这让 strict GPU render path 不再被该类 shader 直接 gate 死。
- 后续主线保留为：把 `GraphicsExecutable` 从 metadata scaffold 演进成正式 graphics execution 入口，以及 transfer/copy/blit/resolve/present 的 backend ownership 和更明确的 backend memory/resource model。
