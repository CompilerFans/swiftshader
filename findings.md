# Findings & Decisions

## 2026-03-14 Session recovery

### New Findings
- 这次恢复后最大的状态偏差不在实现，而在计划文件：Phase 19/20 对应代码和测试仍然成立，但 `task_plan.md` 的 `Current Phase` 指针没有随之推进，导致单看计划文件会误以为 capability gate 仍在进行中。
- 当前工作区中的 `CudaRuntimeAPI` module cache、`GraphicsExecutable` resource/capability introspection、以及 strict GPU descriptor-array draw regressions 是同一批连续改动，focused build/tests 没有暴露中间断层。
- `TrianglePipelineBootstrap.CudaRuntimeReusesCompiledModulesWithModuleCache` 现在提供了直接证据：同一个 runtime 进程内第二次 triangle bootstrap 不会再次增加 module compile 计数，说明“不得每 draw 编译 module”这个 hard gate 目前已被实际测试覆盖。
- `GraphicsBackendPipeline.*` 的 23 个 Vulkan pipeline tests 全绿，说明当前 general resource plan + fragment capability gate 脚手架至少在 sampled image / storage image / dynamic uniform offset / unsupported reason 这些已收敛面上是稳定的。
- 因为 Phase 19/20 已经通过 focused 验证，下一刀不应继续在 resource-plan metadata 上叠 patch；更合理的推进面是框架文档里尚未落地的 attachment lifecycle/state tracking 最小闭环（先覆盖离屏 color attachment 的 clear/store/layout）。
- render pass 与 dynamic rendering 的 color `loadOp == CLEAR` 已经分别在 `CmdBeginRenderPass` / `CmdBeginRendering` 的开始阶段执行；Phase 21 的真实缺口不是“重新实现 clear”，而是把 draw-time color attachment write-back 从 `TriangleBootstrapDraw.cpp` 里的 ad-hoc 直接内存写改造成显式的 attachment target / lifecycle scaffold。
- `CmdDrawBase::draw()` 现在只把 `renderArea`、`layer`、pipeline 和 push constants 传入 backend；它还没有把“当前活跃 color attachment 0 的 image view / layout / storeOp / sampleCount / format 来源”建模成 draw contract。这正是最小 attachment lifecycle 切口。
- 把 attachment 目标状态建模在 draw-call/runtime 层，比塞进 `GraphicsExecutable` 更合理，因为 `loadOp` / `storeOp` / imageView / dynamic rendering attachment 都属于 command-buffer state，而不是 pipeline-time metadata。
- 现有 IR 单测入口已经自然分层：
  - `tests/BackendUnitTests/SpirvToSemanticIRTests.cpp`
  - `tests/BackendUnitTests/KernelIRTests.cpp`
  - `tests/BackendUnitTests/CodegenEmitterTests.cpp` / `tests/BackendUnitTests/AbiParityTests.cpp`
- 目前这些测试更偏 IR 骨架和最小 smoke，尚未按“当前 graphics 路线已落地的 shader 特性”去系统化覆盖。
- 当前 `SemanticIR` 还只显式保存 `stage/entryPoint/vertexLowering`，而与现有 graphics 路线更相关的 compiler functionality 主要散落在 `GraphicsExecutable.cpp`：feature mask、resource/image/texture plan、以及 unsupported reason mask。
- 因此若要做“compilerIR 单测”并独立设计模块，第一刀最自然的对象不是重写 `SemanticIR`，而是先把 `GraphicsExecutable.cpp` 中通用的 compiler analysis 提取成 `src/Pipeline/ShaderCompilerAnalysis.*`。
- point-size bootstrap 推断与 static bootstrap fragment template 推断当前仍更像 bring-up strategy，而不是通用 compiler core semantics；把它们留在 backend 侧能避免首刀模块边界被 bootstrap 过渡逻辑污染。
- `ShaderCompilerAnalysis` 的首个稳定 API 已经验证更适合采用 `entryPoint + SpirvBinary + context`，而不是 `SpirvShader`。这既减少了测试/模块的 Vulkan 依赖面，也让“标准 SPIR-V bc/txt 解析 -> compiler analysis”这一步具备独立演进空间。
- 在此基础上，引入统一输入抽象 `ShaderModuleInput` 是合理的下一步：它把标准 SPIR-V binary 和标准 SPIR-V assembly text 统一到一个 front-end 契约下，为后续 LLVM dialect/LLVM IR lowering 保留稳定入口。
- 第一层 `SpirvToCompilerAnalysis` 现在已经能覆盖一批真实 graphics-route shader 特性：
  - supported：combined image sampler、separate image/sampler、descriptor array constant index
  - gated/unsupported：storage image write、discard、derivatives、image query、image fetch、atomics、subgroup、buffer descriptors
- `ShaderCompilerAnalysis` 已经证明可以直接吃标准 SPIR-V assembly text：当前 text-path tests 不只是测试夹具先 assemble 再比较，而是直接走模块公开 API。
- `GraphicsExecutable` 可以先低风险消费 compiler-analysis 的通用部分：`fragmentFeatureMask`、`imageResourcePlan`、`resourcePlan`。texture bootstrap narrow-path 与 static bootstrap fragment template 保持旧实现，不会破坏现有 pipeline tests。
- `KernelIR` 和 `NormalizedAbiDescription` 先只保存 compiler-analysis 摘要元数据，是一条稳妥的中间路线：既能把第二层/第三层测试打通，又不会过早把未来私有 GPU ABI 细节锁死。
- `ShaderCompilerAnalysis` 现在已经开始接管 texture bootstrap narrow-path 的一部分真实语义，而不只是 resource/gate 粗粒度分析：
  - `location 0` 必须是 `vec2`
  - output value 必须直接来自 texture sample 或极小 passthrough
  - sample 后做真实片元运算会把 `bootstrapSupported` 置为 false
- 这说明“通用 compiler analysis”与“bootstrap-specific bring-up strategy”之间并不是绝对硬边界：像 direct-sample/passthrough 这种实际上属于 shader semantic analysis，本身适合继续下沉进 compiler-analysis 模块。
- `LlvmIREmitter` 现在已经不是纯 placeholder：虽然还没有 lower 到真正有计算语义的 LLVM IR，但它已经能把 compiler-analysis 摘要编码成标准 LLVM IR 文本常量，这为后续接入真正的 LLVM/MLIR lowering 提供了稳定的 metadata 落点。
- 新增 `ShaderCompiler` 协调模块后，当前独立编译器链第一次形成了真正的前端到输出闭环：
  - `ShaderModuleInput` 接标准 SPIR-V bc/txt
  - `ShaderCompilerAnalysis` 做独立分析
  - `KernelIR` 保留 compiler-analysis 摘要
  - `ShaderCompiler` 按目标生成 CUDA-like source 或 LLVM IR text
- 但当前 CUDA/LLVM 输出仍然是 metadata-driven skeleton，不应误判成“已经能把通用 fragment shader fully lower 成可编译 CUDA kernel”：
  - CUDA-like source 已包含 compiler-analysis 常量和空壳 kernel
  - LLVM IR text 已包含 compiler-analysis constant definitions 和空壳 `kernel_main`
  - 真正的 fragment instruction lowering 仍是下一阶段工作
- 不过在 CUDA-like source 这一侧，已经不是“只有空壳 kernel”了：对于当前独立分析已能证明安全的两条纹理窄路径，
  - `CombinedImageSampler + direct-sample/passthrough`
  - `SeparateImageSampler + direct-sample/passthrough`
  当前独立编译器已经能直接输出带 `FsParams` / `sampleTexture(...)` / `fs_entry` 的真实 fragment kernel skeleton。
- 同样的方法也适用于非纹理模板路径：`ConstantColor` 这种不依赖 descriptor / varying 的静态片元模板，已经可以直接从 compiler-analysis 结果下沉到独立编译器的 fragment CUDA-like codegen，而不必再经由旧的 bootstrap helper 拼源码。
- 这条路径对 `FragCoordQuadrants` 和 `FrontFacingBinaryColors` 也成立：只要模板语义足够窄、能够从 compiler-analysis 稳定识别，就可以直接在独立编译器里落成真实 fragment CUDA-like skeleton，而不用继续绑死在旧的 bootstrap source builder 中。
- 同样的模式现在已经扩到更多模板：
  - `FragCoordDiscardLeftConstantColor`
  - `PointCoordGradient`
  - `FlatInterpolatedColor`
  - `InterpolatedColor`
  - `InterpolatedColorBlueNearFragDepth`
  这说明“先识别静态模板，再让独立编译器直接产出对应 fragment skeleton”是当前阶段最有效的推进方式。
- LLVM IR 这一侧也不再只是 metadata 常量容器：虽然还没有走到真正的通用 LLVM lowering，但对 `ConstantColor` 和 `Combined/Separate sampled-image` 这两类路径，已经能输出 `fs_entry` 级 LLVM IR skeleton。这给后续接入真实 LLVM/MLIR lowering 提供了一个更接近目标的落脚点。
- `FragCoordQuadrants` 和 `FrontFacingBinaryColors` 这两条非纹理模板也已经在 LLVM IR 侧打通，说明“先支持静态模板的 LLVM skeleton，再逐步替换成更结构化 lowering”这条路线是可行的。
- 把独立编译器的“正式入口”收进 `src/Pipeline/ShaderCompiler/` 是合理的，但当前最稳的迁移方式不是一次性搬空旧路径，而是：
  - 新目录下放新的正式入口与离线 tool
  - 旧的顶层头文件先保留为兼容层
  - 在线 API 继续复用现有测试，离线能力先用最基础 smoke 验证
  这样不会在框架迁移阶段打断主线 codegen 扩展。
- `ShaderCompilerTool` 当前保持极简是正确的：它的价值主要在于验证“独立编译器可以离线调用”，而不是现在就取代在线 API 或承载复杂 descriptor/layout 语义。
- 将主入口正式收口到 `src/Pipeline/ShaderCompiler/`，同时把顶层 `src/Pipeline/*.hpp` 保留为兼容 wrapper，是当前最稳的目录迁移方式：它能建立清晰模块边界，又不会在演进中打断现有 include 面。
- 这说明下一阶段最合理的推进方式不是一口气做“通用 fragment lowering”，而是沿现有分析已证明安全的窄路径逐条把 codegen 补实：先 combined/separate sampled-image，再考虑 constant-color/fragcoord/frontfacing 等非纹理模板。

### New Decisions
- **Accept:** 会话恢复后的首要动作是同步计划文件与真实代码状态，而不是重新打开已经通过 focused 验证的 Phase 19/20 实现。
- **Accept:** 下一阶段编号推进到 Phase 21，并以 attachment lifecycle/state tracking 最小闭环作为当前推荐主线。

## 2026-03-12 Graphics execution refactor

### New Findings
- sampled-image plan 如果从 fragment shader 的全量 `descriptorDecorations` 出发，就会把 unrelated UBO 这类 non-sampled 资源误解释成 sampled-image 复杂度，从而把本该是 `CombinedImageSampler` / `SeparateImageSampler` 的 shader 错降成 `Other`。
- 对当前阶段更稳妥的 plan 输入不是“shader 有哪些 descriptors”，而是“哪些 descriptors 真实喂给了 texture sample 指令”。这要求从 sample operand 回溯 provenance，而不是做全量 descriptor 枚举。
- 对当前 SPIR-V 形状，sample-use provenance 至少需要识别：
  - `OpSampledImage`
  - `OpImage`
  - trivial forwarding：`OpCopyObject` / `OpCopyLogical` / function-local `OpLoad`
- `SeparateImageSampler` 继续停留在 metadata 层已经不够了：strict GPU triangle bootstrap 下，`DrawTest.TexturedTriangleSeparateImageSamplerNearest` 会直接暴露这个缺口，读回像素呈现 bootstrap 默认绿色，而不是 sampled texture 的红色。
- 真正卡住 strict GPU separate image/sampler draw 的不是 pipeline-time 分类，而是 draw-time consumption：`TriangleBootstrapDraw` 一直只认 `hasBootstrapTextureBinding()`，而这个 compatibility accessor 按设计只覆盖 combined image sampler。
- `texturePlan.bootstrapSupported` 的语义应该比 `hasBootstrapTextureBinding()` 更宽：前者表达“当前 narrow bootstrap 语义下这条 sampled-image plan 是否可执行”，后者只是给 combined-only 调用方保留的兼容视图。
- 继续只围绕某个 unsupported case 收紧 texture-bootstrap 条件，已经开始把“sample/image 资源形态”和“当前 bootstrap 是否支持”混成同一个布尔值；这会让后续支持面扩展越来越难解释。
- 更稳妥的中间形态是：`GraphicsExecutable` 先持有 sampled-image resource plan，再从 plan 中派生当前 bootstrap 是否支持。至少要先显式区分 `CombinedImageSampler`、`SeparateImageSampler` 和 `Other`。
- 一旦把资源形态单独建模，`TriangleBootstrapDraw` 就可以继续只消费兼容的 combined-image-sampler binding，而不需要知道 separate image/sampler 或更复杂 sampled-resource 形态的细节。
- `GraphicsBackendPipeline` 的测试面也应该从 “`hasBinding` true/false” 升级为 “资源方案分类 + 当前 bootstrap support”；否则 separate image/sampler 和 multiple combined samplers 这两类非常不同的 shader 都会被压成同一种 negative。
- 在 Vulkan test translation unit 里直接包含 backend 内部头会再次撞上 `vulkan.hpp` wrapper 与内部 Vulkan 头的脆弱边界；资源方案相关常量更适合放在 test-side introspection bridge，而不是让测试直接 include backend metadata 类型。
- `VkPipeline` 现在通过 `VkNonDispatchableHandle` 表达时，测试层应统一先走 `void *`/`uintptr_t` bridge，而不是继续依赖旧式裸 handle 的 `reinterpret_cast` 写法。
- texture bootstrap 的最后一段 shader metadata 也适合放进 `GraphicsExecutable`，但只应迁 descriptor set / binding 这种 pipeline-time 可确定的信息；texture bytes 和 sampler state 仍然必须留在 draw-time。
- 对当前 narrow texture bootstrap，足够稳定的 pipeline-time 判定条件是：
  - fragment location 0 必须恰好是 `vec2`
  - shader 含有 image sample 指令
  - descriptor decorations 最终收敛到单个 set / binding
- descriptor array 是另一个真实的坑：shader decorations 仍然只会指向单个 set / binding，因此必须额外追踪 descriptor-array element；把 access-chain 的常量 index 提取进 plan 后，`descriptorCount > 1` 的 descriptor array 在 index 常量且 in-bounds 时也能被 narrow texture-bootstrap 支持（non-constant index 仍拒绝）。
- 如果 `location 0` 是 color、而真正的 texcoord 在 `location 1`，当前 bootstrap metadata 必须拒绝提取；在没有 varying 语义映射之前，接受这类 shader 只会产生 false-positive metadata。
- 对当前 `Texture2DColor` bootstrap 来说，“shader 里出现 texture sample” 仍然过宽。若 `location 0` 最终写回的是 `texture(...) * 0.5` 之类的后处理结果，metadata 仍会误报 supported，但 draw-time bootstrap 只会执行直接采样并输出。
- 上一刀的 strict direct-sample 规则又带来了一个真实 false negative：`vec4 sampledColor = texture(...); outColor = sampledColor;` 与直接采样输出语义等价，但若 SPIR-V 经过 function-local `OpStore/OpLoad` 或 `OpCopyObject`，metadata 不应因此误判 unsupported。
- 当前 separate image/sampler 形态已经被正确排除在 texture-bootstrap metadata 路径之外：sample-use 分析和 descriptor-binding 收敛都不应把双 binding 资源误解释成当前单 `COMBINED_IMAGE_SAMPLER` narrow path。
- 这个约束天然会把 separate image/sampler、texture+额外 descriptor 资源等更复杂形态排除在 metadata 路径之外；这不是回归，而是当前 bootstrap scope 的显式边界。
- `GraphicsExecutable` 需要 layout 形状信息，但不应该为此直接把 `VkPipelineLayout.cpp` 的符号面拉进 backend test target。一个由 `VkPipeline.cpp` 提供的轻量 descriptor-info callback 足够表达当前 narrow path 所需的 `descriptorType + descriptorCount`，同时保持链接边界稳定。
- `TriangleBootstrapDraw` 一旦改为只消费 executable metadata，它就不再需要包含 `Pipeline/SpirvShader.hpp`。这是一个很有价值的模块边界信号：graphics draw helper 终于不再自己做 shader reflection。
- `GraphicsBackendPipeline` 级别的 Vulkan 集成测试非常适合继续承接这类 metadata 提取断言：不需要真的 submit draw，也能用真实 shader 编译路径验证 executable contents。
- `GraphicsExecutable` 可以开始承载真正稳定的 pipeline-time bootstrap metadata，而不必等完整 graphics executable 执行路径落地。当前最合适的第一批 metadata 是：
  - vertex shader 的 constant `gl_PointSize`
  - fragment shader 的 shader-only bootstrap 模板（constant color、FragCoord、FrontFacing、PointCoord、flat/smooth color）
- texture bootstrap 仍然不适合现在迁进 `GraphicsExecutable`：它依赖 draw-time descriptor set、image view 和 sampler state 物化，因此应继续留在 `TriangleBootstrapDraw` 里做 draw-time 派生。
- `backend-unittests` 不能让 `GraphicsExecutable.cpp` 直接跨到 `SpirvShader.cpp` 的重型实现边界。一个看似无害的 `Spirv::GetNumInputComponents()` 调用，就会把 `SpirvShader.cpp.o` 拉进链接，并暴露 `VkFormat` / `VkPipelineLayout` / `VkDevice` 等 Vulkan C++ 内部符号未解析。
- 对 `SpirvShader` 这类半反射、半运行时代码，backend 层应优先消费 header 中已有的轻量公开状态（例如 `inputs`、`outputBuiltins`、`analysis`），而不是随手跨进 `.cpp` helper；否则很容易污染测试和模块链接边界。
- “真实 shader -> pipeline-time executable metadata”的最稳测试层不在 backend unit tests，而在 Vulkan pipeline integration tests。通过一个窄的 `PipelineIntrospection` bridge 读取 `GraphicsPipeline` 内部 `GraphicsExecutable`，可以既验证真实 GLSL/SPIR-V 编译路径，又不把 backend test target 拉进 Vulkan 内部链接域。
- 当前切片完成后，triangle bootstrap 的职责边界更清晰：
  - `GraphicsExecutable` 拥有 shader-only bootstrap metadata
  - `TriangleBootstrapDraw` 只做 draw-time texture bootstrap 派生、launch 和 write-back
  - `vk::GraphicsPipeline` 负责在 compile-time 把两者接起来
- `CmdDrawBase::draw()` 目前已经完成了 attachments、descriptor sets、vertex/index 输入和 render area 的准备，因此它是引入 backend-owned draw seam 的合适位置；没必要再把 draw 先压进 `Renderer::draw()` 再决定路由。
- `ExecutionBackend` 现在只有 `submit()` / `synchronize()`，这让 graphics draw 没有单独的后端入口，只能复用 CPU renderer。
- `GpuExecutionBackend` 当前只是“带 runtime 的 submit 壳”，并不真正拥有 graphics draw 决策；它仍然依赖 `Renderer::draw()` 中的 GPU bootstrap 分支。
- `Renderer::draw()` 中的 GPU 逻辑并不依赖 CPU `DrawCall` 的大部分后半段状态，因此可以独立抽到 backend helper：
  - env / strict / fallback 判断
  - fragment bootstrap config 推导
  - triangle bootstrap launch
  - color attachment write-back
- triangle bootstrap 仍然不等于正式 graphics backend，但它是当前唯一已经打通 CUDA launch 的 draw 路径，所以第一刀应当“迁出并降级为 helper”，而不是直接删除。
- `DrawTest.SolidColorTriangle` 的当前 strict-GPU 失败并不是新加的 draw seam 引入的：根因是 `Renderer::draw()` 在真正写回 color attachment 的 triangle-bootstrap 分支里一直把 `fragmentConfig` / `colorStream` / `texCoordStream` / `pointSize` 丢掉了，导致实际 render 走到了 bootstrap 的默认片元着色（绿色常量色），而不是从 fragment shader 推导出的常量红色。
- 当前 draw route 模型若把 “fallback 允许 + 未显式 render-to-attachment” 简化成 `CpuRenderer`，会丢掉现有 bring-up 里保留的一次 warmup-only CUDA bootstrap。因此 route helper 至少要把这种情况建模成 `GpuBootstrapOptional`，再由后续 helper 决定是 warmup-only 还是 render-to-attachment。
- `backend-unittests` 不能直接把重型 triangle-bootstrap runtime helper 当成普通 backend 纯逻辑来测；一旦测试目标引用这个实现，就会把 `vk::Device` / `vk::PipelineLayout` / `vk::ImageView` 等 Vulkan 侧隐藏符号一起拉进链接。把纯策略函数 `planTriangleBootstrapDraw(...)` 留在 header 内联、把 runtime helper 留在 `.cpp`，可以保持现有测试链接边界不变。
- 现在的首个切片已经达到预期架构形态：`GpuExecutionBackend` 持有 triangle-bootstrap helper 和 warmup state，`Renderer::draw()` 不再读取 GPU runtime/env，也不再承担 strict/fallback 决策。
- `SemanticIRModule` 目前已经承载了 pipeline-time `GraphicsExecutable` scaffold 所需的最小信息：shader stage、entry point 和 vertex lowering。当前阶段没必要再发明新的 graphics IR 层。
- `GraphicsPipeline::compileShaders()` 在现有结构下就是合适的挂接点：它在 stage 编译完成后已经持有 `vertexShader` / `fragmentShader`，并且同样能覆盖 graphics pipeline library 拼装后的成员状态。
- graphics pipeline library 的 fragment-only 组合不应伪造 backend executable；当前更稳妥的规则是“有 vertex 才创建 executable，fragment 仅作为 optional stage metadata”。
- 不能在同一个测试翻译单元里同时包含 test-side `vulkan.hpp` wrapper 和内部 `src/Vulkan/VkPipeline.hpp`：两者都占用 `vk` 命名空间。一个窄的 `uintptr_t` introspection bridge 足够解决 graphics pipeline hookup 测试，而不会污染 wrapper 层。

### New Decisions
- **Accept:** sampled-image plan 的 descriptor 收集改成 sample-use provenance；unrelated non-sampled descriptors 不参与 sampled-image plan 分类。
- **Accept:** 当前 provenance resolver 显式支持 `OpSampledImage`、`OpImage`、`OpCopyObject`、`OpCopyLogical` 和 function-local `OpLoad` 这条最小闭环，不在这一刀扩到更大的 SSA/data-flow 图。
- **Accept:** narrow direct-sample `SeparateImageSampler` plan 现在视为 bootstrap-supported；这不改变 `hasBootstrapTextureBinding()` 的 combined-only 兼容语义。
- **Accept:** `TriangleBootstrapDraw` 开始直接消费 richer `GraphicsExecutableTexturePlan`，而不是继续把 draw-time texture bootstrap 绑定死在 combined-binding compatibility accessor 上。
- **Accept:** `GraphicsExecutable` 的 texture metadata 先提升成 sampled-image resource plan，把“资源形态”和“bootstrap support”显式分层。
- **Accept:** 当前 plan 至少显式覆盖三类 sampled-image 资源形态：`CombinedImageSampler`、`SeparateImageSampler`、`Other`。
- **Accept:** `TriangleBootstrapDraw` 暂时继续只消费 plan 中 `bootstrapSupported == true` 的 combined-image-sampler 路径，不在这一刀扩大 draw-time 物化职责。
- **Accept:** Vulkan pipeline tests 需要显式覆盖 `Other` 资源形态，例如多个 combined image samplers，而不只是 combined 与 separate 两类。
- **Accept:** 测试侧资源种类常量放在 `PipelineIntrospection` bridge 中，避免把 backend 内部头直接暴露到 `vulkan.hpp` wrapper 所在的 test TU。
- **Accept:** `GraphicsExecutable` 继续吸收 texture bootstrap 的 descriptor set / binding metadata，但不扩大到 texture bytes / sampler state。
- **Accept:** 当前 texture metadata 提取保持 narrow-path 保守策略；遇到多 descriptor binding 或更复杂纹理资源形态，宁可不提取，也不在 executable 中猜测错误 metadata。
- **Accept:** 当前 narrow texture-bootstrap path 显式要求 `fragment location 0 == vec2`；把 texcoord 放在其他 location 的 shader 暂不视为已支持。
- **Accept:** 当前 narrow texture-bootstrap path 仍要求 layout binding 为 `COMBINED_IMAGE_SAMPLER`；descriptor array 在 array element 为常量且 in-bounds 时视为已支持（non-constant index 仍拒绝）。
- **Accept:** 当前 narrow texture-bootstrap path 还要求 `location 0` 的输出直接来自支持的 texture sample 指令；sample 后的乘法、混色或其他片元运算暂不视为已支持。
- **Accept:** 当前 narrow texture-bootstrap path 允许极小的 trivial passthrough value chain，只跟踪到支持的 texture sample 为止；本轮仅接受 function-local `OpLoad` 和 `OpCopyObject` 这类语义等价转发。
- **Accept:** `hasBootstrapTextureBinding()` 继续保持 combined-only 兼容语义；但 `GraphicsExecutableTexturePlan` + `TriangleBootstrapDraw` 已支持 narrow direct-sample `SeparateImageSampler` 的 strict GPU bootstrap 物化。
- **Accept:** 在 `GraphicsExecutable` 需要少量 Vulkan layout 信息时，优先使用窄 callback / POD bridge，而不是把 Vulkan 实现类型直接拖进 backend 链接面。
- **Accept:** `TriangleBootstrapDraw` 不再直接读取 fragment `SpirvShader`；今后新增 bootstrap shader 分析优先落在 executable create path，而不是 draw helper。
- **Accept:** `GraphicsExecutable` 在本阶段开始持有 bootstrap-specific metadata，但只限 shader-only、pipeline-time 可确定的数据。
- **Accept:** 真实 shader bootstrap 提取验证从 backend 单测下沉到 Vulkan pipeline 集成测试；backend 单测继续保持轻量链接边界。
- **Accept:** 在 backend 层避免直接依赖 `SpirvShader.cpp` 的 helper；能用 header 状态表达的分析，不引入额外链接面。
- **Accept:** `chooseGraphicsDrawRoute()` 在 hardware-backed runtime 下不再区分“是否显式 render-to-attachment”来决定 CPU/GPU 路由；显式 render 开关只影响 backend helper 内部选择 `WarmupOnly` 还是 `RenderToColorAttachment`。
- **Accept:** `TriangleBootstrapDraw` 拆成两层：header-inline 的纯 planning 逻辑，和 `.cpp` 中的 Vulkan/runtime 依赖 helper；这样既能做 backend unit tests，也不把 Vulkan 私有链接边界拖进测试目标。
- **Accept:** 当前切片完成后，CPU-only 的剩余范围明确保留为 `Renderer::draw()`、以及更大范围内尚未 backend-owned 的 transfer/copy/blit/resolve/present/resource model。

### New Decisions
- **Accept:** 第一阶段采用 `ExecutionBackend::draw(...)` seam，而不是继续把 GPU draw 留在 `Renderer::draw()` 内部。
- **Accept:** 第一阶段不直接做完整 `GraphicsExecutable`；那会把 pipeline ABI、resource handle、barrier、present 等问题一次性耦合进来。
- **Accept:** 第一阶段保留 `SWIFTSHADER_GPU_ALLOW_CPU_FALLBACK`，但相关决策迁移到 GPU backend，而不是留在 CPU renderer。
- **Accept:** `Renderer::draw()` 的目标形态是 CPU-only；与 runtime / triangle bootstrap 相关的 helper 要迁出。
- **Accept:** 在继续抽离 triangle bootstrap 之前，先修正 actual render 分支遗漏 bootstrap 参数的根因问题，避免后续 refactor 以错误行为为基线。

## Requirements
- Review two existing plan documents using new user-provided feedback.
- Decide technically whether the design doc and implementation plan need modification.
- Modify the documents when warranted.
- Keep documents bilingual.
- Persist reasoning in project-root planning files.

## Research Findings
- Current design doc already covers architecture, IR, runtime, synchronization, WSI/present, and phased rollout.
- Current implementation plan already has 14 tasks with TDD structure and precise file paths.
- `src/Pipeline/SpirvShader.*` is strongly coupled to Reactor code emission today: `SpirvShader.hpp` directly exposes `emit*`, `SpirvEmitter`, `rr::Value`, `SpirvRoutine`, and many `Emit*` methods. This validates the concern that a simple member method like `SpirvShader::buildSemanticIR()` would likely accumulate technical debt.
- `README.md` is clearly public-facing project documentation, not an internal bring-up note. Adding custom backend instructions there early would be risky unless the backend becomes a public/project-level feature.
- Current design doc mentions quad/helper-lane formation, but does not yet spell out execution invariants such as 2x2 residency, helper-lane execution without export, discard/demote behavior, or derivative constraints.
- Current design doc mentions image/sampler categories, but does not distinguish combined image sampler, separate sampler, storage image, or non-uniform descriptor indexing.
- Current design doc says the CUDA-like source and LLVM IR paths share ABI, but does not define a verification mechanism.
- Current design doc reserves a native displayable-image fast path, but does not define clear eligibility conditions.
- Current implementation plan adds `ResourceStateTracker` before graphics/present stubs, but does not explicitly require those later tasks to integrate with it.
- Current implementation plan adds a compute compile path, but does not yet require fake dispatch validation of launch parameters and buffer binding.

## Decisions
- **Accept:** Expand design doc with explicit quad/helper-lane fragment execution invariants.
- **Accept:** Expand design doc with explicit `SemanticIR` modeling rules for combined image sampler, separate sampler, storage image, and non-uniform descriptor access.
- **Accept:** Add an explicit ABI conformance verification mechanism shared by CUDA-like source and LLVM IR codegen paths.
- **Accept:** Clarify native displayable-image fast-path eligibility versus fallback copy/blit conditions.
- **Accept:** Change implementation plan so `SemanticIR` lowering is built as a standalone builder/visitor, not as a `SpirvShader` member API.
- **Accept:** Strengthen the compute bring-up task with fake dispatch validation, not only executable creation.
- **Accept:** Require `GraphicsBackend` and `PresentAdapter` tasks to integrate with `ResourceStateTracker`.
- **Accept:** Remove near-term `README.md` edits from the implementation plan; keep backend bring-up docs in internal or specialized docs first.
- **Partial accept:** The existing design is directionally correct, so these changes are refinements and risk reductions, not architectural reversals.

## Open Questions
- Whether ABI equivalence should be validated through textual ABI header comparison, structured metadata comparison, or both.
- Whether quad/helper-lane semantics deserve a dedicated subsection or should extend the existing `KernelIR` section only.
- How to thread the real CUDA runtime through the Vulkan shared-library compute path without stalling queue submission.

## Relevant Files
- `docs/plans/2026-03-07-cuda-vulkan-icd-design.md`
- `docs/plans/2026-03-07-custom-gpu-vulkan-icd-implementation.md`
- `src/Pipeline/SpirvShader.hpp`
- `src/Vulkan/VkQueue.cpp`
- `src/Vulkan/VkCommandBuffer.hpp`
- `src/WSI/VkSurfaceKHR.hpp`
- `README.md`
- `docs/plans/2026-03-08-custom-gpu-cuda-bootstrap-design.md`
- `docs/plans/2026-03-08-custom-gpu-cuda-bootstrap-implementation.md`
- `src/Backend/CudaCompilerDriver.hpp`
- `src/Backend/CudaCompilerDriver.cpp`
- `src/Backend/CudaRuntimeAPI.hpp`
- `src/Backend/CudaRuntimeAPI.cpp`
- `tests/BackendUnitTests/RuntimeAPITests.cpp`
- `tests/VulkanUnitTests/DrawTests.cpp`

## New Findings
- The custom GPU backend can now use the host machine's real CUDA toolchain: `nvcc` is available at `/usr/local/cuda/bin/nvcc`, `libcuda.so.1` is visible through the system loader, and a visible GPU is present.
- Driver API dynamic loading must prefer versioned symbols such as `cuMemAlloc_v2`, `cuMemFree_v2`, `cuMemcpyHtoD_v2`, and `cuMemcpyDtoH_v2`. Using the unversioned names led to `CUDA_ERROR_INVALID_CONTEXT` during the first real runtime tests.
- Vulkan wrapper tests that rely on the generated SwiftShader ICD must be run from their corresponding build directories. Running them from the repository root produces misleading loader failures and false crash symptoms.
- `backend-unittests` cannot directly depend on the full `SpirvShader.cpp` object graph without pulling hidden/internal Vulkan C++ symbols into the test link. Splitting `SemanticIRBuilder`'s lightweight `SpirvBinary` path from the heavier `SpirvShader` overload keeps backend tests small and linkable while preserving the runtime-facing overload for `VkPipeline`.
- The repository already contains a suitable starting point for simple-but-real draw performance measurement in `tests/VulkanBenchmarks/TriangleBenchmarks.cpp`; the missing piece is not a benchmark framework, but a staged baseline policy and a heavier batched triangle case that can later be reused unchanged by the GPU draw path.
- On non-Windows platforms, `tests/VulkanWrapper/VulkanHeaders.hpp` forces `USE_HEADLESS_SURFACE=1`, so the new FPS observer can provide live FPS printing immediately but cannot show a native visible window without broader platform window-support work. That limitation is pre-existing and independent of the new performance gate.
- Interface compile definitions from `vk_base` do not automatically reach standalone benchmark executables like `draw-fps-observer`, so tool-local behavior such as CUDA dump suppression must not rely on `SWIFTSHADER_CUSTOM_GPU_USE_CUDA` being defined in the benchmark translation unit.
- For the CUDA-like vertex emitter, preserving only `Location 0` is not enough; the SPIR-V input vector width also has to survive lowering so `vec2` vertex inputs do not trigger an out-of-contract read of `position[2]`.
- The Vulkan runtime vertex-validation layer benefits from a direct `std::vector<uint32_t>` shader-module helper in `DrawTester`; it keeps GLSL-based tests and explicit SPIR-V module tests on the same lightweight draw harness without dropping to raw setup code in each case.
- A fragment standalone bootstrap can reuse the same runtime/module/launch pattern as the vertex bootstrap if it lowers to an explicit invocation list plus a linear RGBA8 buffer; helper/export suppression can be expressed as per-invocation flags without blocking the later raster integration.
- `gl_FragCoord`-based fragment validation does not need new harness code in this repository: a fullscreen triangle plus four quadrant sample points is already enough to produce a visually useful BMP artifact and stable assertions.
- For an ordinary triangle with `gl_FragCoord` quadrant coloring, top-half coverage narrows quickly near the apex, so stable assertions should sample upper pixels close to the screen center; background color should not be asserted with the current `DrawTester` because the non-multisample color attachment still uses `eDontCare`.
- The current SwiftShader CPU raster stack is valuable as a semantic oracle, but its `SetupProcessor` / `QuadRasterizer` / `PixelProcessor` implementation shape is not a good direct fit for early CUDA bring-up; a simple in-house CUDA raster with CPU-reference tests is the lower-risk path.
- For the first raster bootstrap, writing a dense per-pixel `FragmentBootstrapInvocation` grid on the GPU and compacting it on the host is simpler and less risky than introducing an atomic append buffer up front; it keeps the CUDA kernel minimal while still allowing direct comparison against the CPU reference oracle.
- The first end-to-end triangle bootstrap failure was caused by raster launch geometry, not by the sample point: launching `raster_entry` as a single `64x64` CUDA block meant only the vertex stage completed. Switching raster to small blocks over a 2D grid allowed the full `VS -> Raster -> FS` bootstrap chain to execute.
- Hard-coding fragment output in the three-stage bootstrap would stall the path before it can consume real draw state; adding a narrow `TrianglePipelineBootstrapConfig` for framebuffer size and RGBA output is a low-risk way to start replacing hard-coded stage behavior with explicit inputs.
- The next practical step toward real draw integration is not exposing full Vulkan state, but reusing the existing raw-vertex fetch contract (`rawVertexData + vertexCount + GraphicsBootstrapBindingConfig`) inside `TrianglePipelineBootstrap`; that moves the bootstrap path onto the same minimal position-fetch surface without widening the API too early.
- The first safe bridge from real Vulkan draw state into the CUDA bootstrap path is `sw::Stream`, not the broader `DrawCall` or queue submit layer: after `Inputs::bindVertexInputs()`, `stream.buffer` already points at the bound attribute bytes with binding and attribute offsets applied.
- For the current triangle bootstrap, the narrowest correct support window is `VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST` with exactly one primitive and a vertex-rate `Location 0` position stream in `VK_FORMAT_R32G32_SFLOAT` or `VK_FORMAT_R32G32B32_SFLOAT`; widening beyond that should come after the real draw-fed path is stable.
- Queue-submit-time graphics bootstrap hides the real draw inputs behind a hard-coded triangle. Moving the first hardware-backed bootstrap trigger into `Renderer::draw()` makes the dumped CUDA stage wrappers correspond to an actual bound draw call while still leaving final rasterization on the CPU path.
- A practical next step beyond the first real-draw bridge is to widen from one triangle to many triangles before attempting richer shader semantics. The bridge can safely copy `primitiveCount * 3` bound vertices for non-indexed triangle-list draws without needing broader Vulkan state.
- For early bring-up, it is cheaper to reuse the existing single-triangle `RasterBootstrap`/`FragmentBootstrap` pair per triangle and compose the RGBA8 results on the host than to redesign the raster kernel around multi-triangle batching immediately. That keeps the code simple while enabling a first multi-triangle CUDA path.
- The first useful fragment-side semantic expansion after constant color is a tiny `shaderKind` switch in `FragmentBootstrapConfig`. That keeps the launch ABI stable while allowing the emitted CUDA source to model multiple fragment behaviors.
- For the current simple raster bootstrap, a triangle-pipeline integration test should not assume too many interior sample points. A more robust gate is: one known-covered pixel plus confirmation that the last compiled fragment CUDA module actually contains the requested shader body.
- In the Vulkan draw tests, `backend::CudaRuntimeAPI::globalLastModuleSource()` is not a reliable observation point because `draw-unittests` and `libvk_swiftshader.so` hold separate copies of the runtime's static capture state. Cross-library CUDA source verification needs an external channel such as a file dump.
- When doing lightweight SPIR-V analysis from `SpirvShader`, iterate with `shader.begin()/end()` or range-based-for. Walking `shader.insns` directly includes the 5-word SPIR-V header and can stall or misparse the module.
- `sw::Stream` already exposes enough metadata to bridge a minimal second vertex attribute into the CUDA bootstrap path. If `stream[0]` and `stream[1]` share binding, rate, and stride, `colorOffset` can be derived as `stream1.buffer - stream0.buffer` without widening the Vulkan-side draw plumbing yet.
- `FragmentBootstrap` originally launched all invocations in a single CUDA block. Once raster started feeding larger invocation lists for interpolated-color triangles, that exceeded practical block limits and produced silent zero output. The fragment stage needs the same multi-block launch treatment as raster.
- On this Linux environment, a visible benchmark window is feasible through XCB: `DISPLAY=:0`, `libxcb` is available, and `libvk_swiftshader.so` exports `vkCreateXcbSurfaceKHR` / `vkGetPhysicalDeviceXcbPresentationSupportKHR`. The missing piece was only the test wrapper forcing headless mode.
- For backend selection, the honest near-term interface is a wrapper script that chooses between a CPU build directory and a CUDA-enabled build directory. The repository still does not support a true runtime CPU/CUDA switch inside one binary.
- The first visible animated-triangle benchmark already shows a large CPU/CUDA gap on this machine (`~297 FPS` vs `~0.8 FPS` over short runs). That does not yet prove the final design is wrong, but it is exactly the kind of result the staged performance gate is supposed to surface early.
- The original CPU SwiftShader path is already much richer than the current CUDA bootstrap in three concentrated areas: input assembly / topology breadth, interpolation + sample semantics, and fragment fixed-function integration. Next-phase planning should therefore be organized by stage-parity families, not by whichever demo is easiest next.
- The current CPU reference scope for near-term parity is clear: keep `Vertex/Fragment/Compute` only, postpone `TCS/TES/GS`, and expand CUDA in the order `indexed draws -> barycentrics/interpolation -> fragment builtins -> depth/stencil/blend -> topology breadth -> resource-heavy shaders`.
- The narrowest practical path for indexed bootstrap support is to deindex on the host side before launching the current `VS -> Raster -> FS` CUDA chain. That preserves the existing runtime ABI and still expands real Vulkan draw-state coverage.
- The current interpolated-color shortcut can be replaced incrementally: because `TrianglePipelineBootstrap` still executes per triangle, raster can emit first-class barycentrics while fragment receives the three vertex colors through `FragmentBootstrapConfig`. This upgrades the stage contract without requiring a full multi-triangle fragment ABI yet.
- `gl_FrontFacing` can be brought up with a very small ABI expansion: raster only needs one per-triangle facing bit, and fragment can consume it through a dedicated binary-colors mode before full arbitrary FrontFacing expression lowering exists.
- `gl_FrontFacing` does not require a full arbitrary fragment-expression lowering to be useful early. A single per-triangle facing bit from raster plus a binary front/back color mode is enough to validate the end-to-end path and align with the CPU renderer's orientation rules.
- A practical early discard path does not require general expression lowering. For the current bootstrap, `FragCoord + ContainsDiscard` can be validated with a dedicated half-screen discard mode, which exercises real fragment kill semantics without widening the raster ABI.
- A minimal early `FragDepth` path can be made real without full depth/stencil parity: let `FragmentBootstrap` optionally write a depth buffer, then let `TrianglePipelineBootstrap` perform host-side depth composition while the stage ABI is still narrow.
- The shortest way to bring up point-list before `PointCoord` is to expand each point into two host-side triangles and reuse the existing triangle raster/bootstrap chain. This is a transitional implementation, but it widens real draw-state coverage immediately.
- Once point-list bootstrap exists, `PointCoord` becomes a pure fragment-payload problem. The shortest path is to populate `pointCoordX/Y` directly in the point branch and expose a narrow `PointCoordGradient` fragment mode before tackling general point fragment lowering.
- `flat` 插值在当前 bootstrap 里不需要新的 raster payload；只要在 fragment 侧直接选 provoking vertex 的颜色即可。结合当前默认 `FIRST_VERTEX` 规则，这是一条非常短的 parity 增量路径。
- The earlier `noperspective` draw-test crash was a glslang (GLSL -> SPIR-V) segfault triggered by `#version 310 es` shaders using `noperspective`. Switching the reproducer to Vulkan GLSL `#version 450` avoids the crash, and `DrawTest.FragmentShaderUsesNoPerspectiveColor` now provides repo-local coverage that validates `NoPerspective` interpolation differs from perspective-correct interpolation.
- `triangle strip` can be brought up with the same transitional strategy used for indexed draws and point lists: expand to triangle-list on the host side using the CPU default `FIRST_VERTEX` strip ordering, then reuse the existing triangle bootstrap chain.
- `IndexedTriangleStripWithPrimitiveRestart` currently fails in the CPU-only baseline as well as the CUDA build, so primitive restart should be tracked as a CPU-reference blocker rather than a CUDA-bootstrap regression until the baseline path is understood.
- `IndexedTriangleStripWithPrimitiveRestart` currently fails in the CPU-only baseline, so primitive restart remains a CPU-reference blocker and should not be used as a CUDA regression signal yet.
- `triangle fan` is another good fit for host-side topology expansion: keep the center vertex fixed, expand `(0, i+1, i+2)` per primitive, and reuse the existing triangle bootstrap chain unchanged.
- `gl_SampleMask` output currently crashes in the CPU-only baseline as well as the CUDA build. Like the earlier `noperspective` case, it should be tracked as a CPU-reference blocker before being used as a CUDA parity milestone.
- `LineListConstantColor` needed an explicit wide line in the draw harness to become a stable CPU baseline. For topology bring-up, test geometry should be chosen to avoid mistaking thin-line raster rules for backend regressions.

- `LINE_STRIP` needed a frame-level readback assertion instead of a single hard-coded sample point. The rendered V-shape leaves the screen center empty, so stable verification should count red pixels across the dumped frame rather than assume one interior pixel is covered.

- `gl_PointSize` for the current bootstrap can be brought up with a narrow constant-path extractor: scan the builtin output block for `PointSize`, follow `OpAccessChain` pointers to the matching member, and accept only uniform constant `OpStore` values. That is enough to remove the old hard-coded point-size fallback for simple point shaders.

- The current host-side deindexing step is already general enough to support indexed `LINE_STRIP` and indexed `TRIANGLE_FAN` in the bootstrap path, as long as primitive restart is not enabled. That makes primitive restart the real remaining blocker in this topology family.

- After the `PointSize` bridge landed, indexed `POINT_LIST` required no extra runtime changes: the existing deindex-before-bootstrap path already composes correctly with the point quad expansion and point-fragment payload generation.

- `IndexedTriangleStripWithPrimitiveRestart` was not a real renderer blocker in the current draw harness: the initial failure came from sampling below the actual covered region. After correcting the sample points, the case passes in the CPU baseline.

- `IndexedTriangleStripWithPrimitiveRestart` is currently covered at the draw-harness level and passes in both CPU and CUDA builds; the previously observed failure was caused by sampling below the actual covered region, not by primitive restart semantics being broken in the renderer path.

- A minimal `noperspective` varying-color draw case no longer blocks the suite once authored as Vulkan GLSL `#version 450`: the repo-local `DrawTest.FragmentShaderUsesNoPerspectiveColor` passes in both CPU and CUDA builds and provides a concrete regression gate.

- `indexed POINT_LIST` composes cleanly not only with constant-color output but also with the existing `PointCoordGradient` fragment path; no extra runtime changes were required beyond the earlier deindex + point-size work.

- The repository already contains a real Vulkan textured-triangle setup in `tests/VulkanBenchmarks/TriangleBenchmarks.cpp`; the shortest path for texture support is to reuse that descriptor/image infrastructure instead of inventing a fake texture system.

- The existing Vulkan descriptor payload is sufficient for a narrow bootstrap texture path: `SampledImageDescriptor` already contains prepared `sw::Texture`, dimensions, and `samplerId`, so the bootstrap only needs to copy level-0 texels plus a reduced sampler-state view.

- The texture benchmark already existed, but its fragment shader and descriptor write path used different binding numbers. Aligning both to binding 1 makes the benchmark exercise the same narrow texture path used by the new draw tests.

- The initial `linear/repeat` texture assertion failed because the chosen sample point assumed a near-white center mix. Actual CPU/CUDA output is a stable red/blue-heavy blend with low green at the center, so the robust test needs multiple sample points rather than a naive all-channels-high expectation.

- The original CPU-side test harness does not clear the non-multisample color attachment by default: `tests/VulkanWrapper/DrawTester.cpp` uses `AttachmentLoadOp::eDontCare` there. So undefined background is expected unless a test explicitly opts into clearing. The right fix is an opt-in clear path for tests, not changing the default semantics globally.

- For visible benchmarks, explicit clear is necessary for reliable visual output. Unlike unit tests, users interpret the window contents frame-to-frame, so leaving the background undefined makes the benchmark look broken even when draw results are technically valid. This is a harness-level presentation requirement, not a reason to globally change Vulkan clear semantics.

- Matching `noperspective` on both VS output and FS input was never the real issue; the root cause was glslang crashing on the `#version 310 es` + `noperspective` combination instead of emitting a compile error or valid SPIR-V.

- A narrow push-constant path is low-risk and high-yield for feature expansion: the harness and command buffer path already supported Vulkan push constants, so adding explicit test-side pipeline layout ranges/data was enough to unlock real VS/FS push-constant coverage without being blocked on `noperspective`.

- `gl_InstanceIndex` is an easy low-risk coverage gain even before custom bootstrap-specific lowering: the existing CPU/CUDA draw paths already render instanced geometry correctly through normal Vulkan command recording.

- `drawIndexed(..., vertexOffset, ...)` is another low-risk coverage gain: the existing CPU/CUDA draw paths already honor `baseVertex`, and the only real work needed here was choosing sample points inside the shifted triangle.

- The local sample scan revealed that `hello_triangle_1_3` already depends on dynamic rendering (`vkCmdBeginRendering`), so it should not be grouped with plain `hello_triangle` in planning. Repo-local `dynamic_rendering` coverage now exists (`DrawTest.DynamicRenderingSolidColorTriangle`), and Vulkan Samples `hello_triangle_1_3` now runs headless against SwiftShader.

- `separate_image_sampler` was initially blocked by a DrawTester limitation: the descriptor pool was hard-coded for `eCombinedImageSampler`, causing `vk::Device::allocateDescriptorSets` to throw `ErrorOutOfPoolMemory` for separate image/sampler layouts. After sizing the pool from the descriptor set layout bindings, `DrawTest.TexturedTriangleSeparateImageSamplerNearest` passes in both CPU and CUDA builds.

- `VK_VERTEX_INPUT_RATE_INSTANCE` is another low-risk compatibility gain: the underlying renderer already handles the Vulkan path correctly, and the main work was extending the test harness from one vertex buffer to a narrow two-binding case.

- `draw(..., firstInstance)` is another low-risk external-sample-aligned gain: the renderer already honors `gl_InstanceIndex` with non-zero firstInstance, and the only adjustment needed was choosing sample points inside the resulting shifted geometry.

- `dynamic_rendering` now has repo-local draw coverage via `DrawTest.DynamicRenderingSolidColorTriangle`; the DrawTester path uses `vkCmdBeginRendering`/`vkCmdEndRendering` with explicit swapchain-image layout transitions. External sample execution is still pending.

- The existing multisample harness path is already strong enough for an initial `msaa` sample-aligned gate. A simple resolved solid triangle passes in both CPU and CUDA builds without requiring new backend work.

- `dynamic_uniform_buffers` was initially blocked by a DrawTester harness gap: dynamic descriptor sets were bound without supplying dynamic offsets, and there was no per-draw dynamic-offset rebind hook. After adding `DrawTester::bindDescriptorSet(...dynamicOffsets...)` plus `DrawTest.DynamicUniformBufferOffsetsSelectPerDrawColor`, dynamic UBO offsets now have repo-local coverage in both CPU and CUDA builds.

- `vertex_dynamic_state` turned out to be a low-risk sample-aligned gain: SwiftShader already exposes `vkCmdSetVertexInputEXT`, so the main missing piece was test-harness support for the extension, feature enablement, and dynamic vertex-input command recording.

- `vertex_dynamic_state` is now covered beyond the trivial solid triangle: a multi-attribute interpolated-color triangle also passes through `vkCmdSetVertexInputEXT` in both CPU and CUDA builds, which increases confidence before broader sample-style expansion.

- A local `textureLod(..., 1.0)` + two-level mip draw probe does not produce the expected level-1 color even in the CPU baseline. That makes `texture_mipmap_generation` more than a simple bootstrap gap, and not part of the current low-risk feature lane.

- `vertex_dynamic_state` combines cleanly with `VK_VERTEX_INPUT_RATE_INSTANCE` in the current harness: the same dynamic binding/attribute path works for both per-vertex and per-instance inputs in CPU and CUDA builds.

- `vkCmdClearAttachments` is a good low-risk entry point for the `render_passes` sample family: it exercises in-render-pass attachment manipulation without immediately needing the harder subpass/lifecycle cases.

- `MSAA` remains a good low-risk lane: after the earlier solid-color case, a resolved interpolated-color triangle also passes in both CPU and CUDA builds, so resolve coverage is no longer limited to a constant fragment color.

- `loadOp = LOAD` on swapchain-backed color attachments is another practical low-risk entry point for the `render_passes` sample family; in the current harness it works cleanly in both CPU and CUDA builds when the images are explicitly initialized first.

- Depth-aspect `vkCmdClearAttachments` is another stable low-risk `render_passes` building block: in the current harness it cleanly gates later draw calls through depth test in both CPU and CUDA builds.

- The current harness now confirms that `baseVertex`, `firstInstance`, and `VK_VERTEX_INPUT_RATE_INSTANCE` compose correctly together in a single indexed instanced draw on both CPU and CUDA paths; the earlier failure was only a bad sample point.

- A narrow two-subpass render pass is still a low-risk addition in the current harness: reusing the same shaders/state with a second subpass-specific pipeline is enough to validate `vkCmdNextSubpass` sequencing in both CPU and CUDA builds.

- `vkCmdSetVertexInputEXT` composes cleanly with the existing indexed-instancing path: the current harness passes the combined `indexed + baseVertex + firstInstance + instance-rate` scenario in both CPU and CUDA builds.

- Configurable `minImageCount` is a low-risk way to strengthen the `swapchain_images` sample line: the current harness can request triple buffering cleanly and still render/present correctly in both CPU and CUDA builds.

- Depth `loadOp = LOAD` is stable in the current harness when images are initialized first: both CPU and CUDA builds preserve prior depth values strongly enough to reject a farther triangle on later frames.

- The current harness confirms that depth attachment contents persist correctly across subpasses: a nearer triangle written in subpass 0 still blocks a farther triangle drawn in subpass 1 on both CPU and CUDA paths.

- The root cause of the initial MSAA+depth failure was in the test harness, not the renderer: `beginRenderPass` used a too-short `clearValues` array and placed the depth clear value in the wrong slot when both resolve and depth attachments were present.

- The current triple-buffered swapchain path remains stable across successive presents: after requesting `minImageCount = 3`, both CPU and CUDA builds correctly present updated contents over multiple frames rather than only the first frame.

- The current harness can safely update a bound graphics descriptor set between frames without re-recording command buffers: both CPU and CUDA builds picked up the new combined image sampler contents on the next submit.

- The current combined-image-sampler path composes cleanly with instancing: both CPU and CUDA builds rendered two offset textured instances correctly using the existing narrow texture bootstrap path.

- The current narrow texture path composes cleanly with dynamic vertex input and instancing together: both CPU and CUDA builds rendered instanced textured triangles correctly under `vkCmdSetVertexInputEXT`.

- 2026-03-10 plan pivot: for the next stretch, the primary acceptance signal is SwiftShader's own built-in unit tests under the CUDA-enabled build, not external Vulkan-Samples runs.

- Per user direction, CPU-baseline comparison should not drive prioritization for this round. Treat the repository-owned test failures in the CUDA build as the actionable queue unless a failure clearly proves the test itself is invalid.

- The current `task_plan.md` had gone stale: it still described an older document-review task even though the repository state and `progress.md` already reflect substantial CUDA/backend test work. Planning files now need to track unit-test stabilization as the active objective.

- In `build-cuda-bootstrap/`, `ctest -N` reports `Total Tests: 0`. The actionable built-in test entry points are the standalone binaries such as `backend-unittests`, `draw-unittests`, and `vk-unittests`, not CTest registrations.

- First CUDA-built-in failure cluster from `./build-cuda-bootstrap/backend-unittests` is tightly grouped, not random:
  - `TrianglePipelineBootstrap.CudaRuntimeAppliesRequestedVertexGeometry`
  - `TrianglePipelineBootstrap.CudaRuntimeUsesRawVertexDataAndBinding`
  - `TrianglePipelineBootstrap.CudaRuntimeRendersMultipleTrianglesFromRawVertexData`
  - `TrianglePipelineBootstrap.CudaRuntimeAppliesFragCoordQuadrantFragmentMode`
  - `RasterBootstrap.CudaRuntimeMatchesCpuReference`
  - `RasterBootstrap.RasterFeedsFragmentBootstrap`

- The strongest current signal is in `RasterBootstrap.CudaRuntimeMatchesCpuReference`: for the same simple 8x8 triangle, CUDA output reports `28` fragment invocations while the CPU reference reports `10`. Downstream triangle-pipeline failures all look like “expected covered pixel stayed black”, which points more toward raster coverage / coordinate generation than toward fragment shading or basic CUDA runtime launch failures.

- Root-cause hypothesis for the first failure cluster is now concrete: the raster CUDA kernel writes `RasterInvocation`, but host code allocates and reads the buffer as `FragmentBootstrapInvocation`.
  - Device-side `RasterInvocation` in `src/Backend/RasterBootstrap.cpp` contains `x/y/exportMask/helperInvocation/frontFacing/barycentric0/1/2`.
  - Host-side `FragmentBootstrapInvocation` in `src/Backend/FragmentBootstrap.hpp` additionally contains `pointCoordX/pointCoordY` before the barycentrics.
  - That means the kernel uses a 32-byte stride while host code reads back with a 40-byte stride, which cleanly explains why coverage counts inflate (`28` vs `10`) and why downstream raster-fed triangle tests read incorrect covered pixels.

- The minimal root-cause fix is to restore layout parity at the raster/fragment boundary, not to special-case any individual failing test. Adding `pointCoordX/pointCoordY` to the raster CUDA-side invocation struct and zero-initializing them is sufficient because point rasterization already populates those fields elsewhere without using `runRasterBootstrap()`.

- `draw-unittests` in `build-cuda-bootstrap/` currently do not show a concrete failing test family. The earlier full-suite session died near `LineStripConstantColor` / later near `MultisampleSolidColorTriangle`, but those tests pass individually and the suite passes when sharded into smaller batches. For now, treat that as an execution-session limitation rather than a renderer or harness regression.

- The first concrete `vk-unittests` crash in the CUDA build is `ComputeBackendPipelineTest.BuildBackendExecutableWithoutDispatch`, exiting with signal 11 (`139`) before any assertion output.

- Root cause is a test-fixture control-flow bug, not a compute backend assertion failure:
  - `ComputeBackendPipelineTest::SetUpTestSuite()` calls `GTEST_SKIP()` under `SWIFTSHADER_CUSTOM_GPU_USE_CUDA` and therefore never calls `driver.loadSwiftShader()`.
  - GoogleTest still proceeds to run `BuildBackendExecutableWithoutDispatch`, which then dereferences the unresolved `driver` function table (`driver.vkCreateInstance(...)`) and segfaults.
  - The correct place to gate unsupported CUDA compute Vulkan tests is at the individual test level, while keeping suite setup responsible for initializing the shared driver fixture.

- The `vk-unittests` failure in `DrawTest.FragmentShaderDiscardsLeftHalfByFragCoord` is a test-stability bug, not a discard implementation regression. The left half of the frame is discarded and therefore preserves the color attachment's prior contents; with the default non-MSAA `eDontCare` load path, that background is undefined and can drift above the old `< 180` threshold after earlier tests.

- A stable fix for the discard case is to opt into an explicit color clear for that test and assert the known clear color on the discarded half. This matches the repository's existing `DrawTester::enableColorClear()` pattern and removes the sequence dependence.

- The CPU and CUDA draw tests use the same `DrawTester` / `VulkanTester` lifecycle harness; the CUDA-only repeat crash is therefore not explained by a different top-level test framework.

- Before this cycle, plain `DrawTester tester;` destruction was not safe because `DrawTester::~DrawTester()` and `VulkanTester::~VulkanTester()` unconditionally called Vulkan device methods even when `initialize()` had never run. Guarding those destructors is a valid harness hardening change and enables explicit construct/destruct-only coverage.

- The remaining CUDA repeat crash now has a tighter boundary:
  - `DrawTest.ConstructThenDestroyWithoutInitialize` repeats cleanly in both CPU and CUDA builds.
  - `DrawTest.InitializeThenDestroyWithoutRender` repeats cleanly in both CPU and CUDA builds.
  - `DrawTest.VertexShaderNoPositionOutput` and `DrawTest.SolidColorTriangle` still crash under CUDA repeats once `renderFrame()` is involved.
  - Therefore the active fault is in the draw submit/present path or post-submit teardown after a real frame, not in plain construction or initialization.

- `DrawTest.RenderWithoutPresentThenDestroy` also crashes under CUDA repeats while passing on CPU, so `queuePresent()` / surface presentation is not the leading suspect anymore. The remaining fault boundary is “real frame submit after swapchain acquire”, not “presentation only”.

- `DrawTest.InitializeThenDestroyWithoutRender` still triggers one CUDA launch in the current CUDA build, which is consistent with the existing runtime warmup path. That warmup-only launch is stable across repeats; the crashing cases trigger additional real draw-stage launches. So the remaining bug is not “any CUDA launch”, but something specific to the actual draw bootstrap/submit path.

- `DrawTest.AcquireWithoutSubmitThenDestroy` repeats cleanly in both CPU and CUDA builds. This removes `acquireNextImage()` and swapchain image acquisition state from the primary suspicion set. The remaining CUDA-only repeat crash now narrows to the actual submitted frame work after acquire, not to initialization, warmup, acquire, or present in isolation.

- The remaining CUDA-only draw repeat crash is narrower than “any submitted frame”:
  - `DrawTest.SubmitWithoutDrawThenDestroy` repeats cleanly in both CPU and CUDA builds.
  - `DrawTest.DrawZeroVerticesThenDestroy` repeats cleanly in both CPU and CUDA builds.
  - `DrawTest.DrawOneVertexThenDestroy` repeats cleanly in both CPU and CUDA builds.
  - `DrawTest.DrawTwoVerticesThenDestroy` repeats cleanly in both CPU and CUDA builds.
  - `DrawTest.RenderWithoutPresentThenDestroy` still crashes under CUDA repeats with the default 3-vertex triangle draw.
  - Therefore the current fault boundary is no longer generic submit, command buffer execution, render-pass setup, or VS-only/incomplete-primitive work; it first appears once the draw path forms a complete triangle primitive and enters the triangle assembly / raster / fragment portion of the pipeline.

- `DrawTest.DrawDegenerateTriangleThenDestroy` still crashes under CUDA repeats even though the triangle has zero area and should not produce meaningful covered fragments; the same case repeats cleanly on CPU.
  - A one-shot launch stamp probe with `SWIFTSHADER_CUDA_DISABLE_WARMUP=1` shows `DrawTest.DrawTwoVerticesThenDestroy` produces `0` stamped launches, while `DrawTest.DrawDegenerateTriangleThenDestroy` produces `3`.
  - Therefore the current boundary is narrower than “actual fragment coverage”: the first complete 3-vertex triangle primitive is enough to trigger the CUDA bootstrap launches and the later repeat crash, even when the primitive is degenerate.

- Root cause of the backend degenerate-triangle bug is now confirmed in `RasterBootstrap`:
  - CPU reference happened to return zero coverage for zero-area triangles because the computed bounding box collapses (`bboxMin > bboxMax`) and the coverage loops do not execute.
  - The CUDA raster path had no degenerate-triangle guard, so it launched over the full render target, `pointInsideTriangle()` treated the zero-area triangle as inside everywhere, and barycentric interpolation divided by zero denominators.
  - A dedicated backend test now reproduced that mismatch directly: `RasterBootstrap.CudaRuntimeRejectsDegenerateTriangleLikeCpuReference` failed before the fix and passes after adding an explicit zero-area early return in `runRasterBootstrap()`.

- After the degenerate-raster fix, the draw crash landscape changed:
  - `DrawTest.RenderWithoutPresentThenDestroy` now repeats cleanly for 25 iterations in the CUDA build.
  - `DrawTest.DrawDegenerateTriangleThenDestroy` still crashes under CUDA repeats, but its warmup-disabled launch count dropped from `3` to `1`, confirming the raster/fragment degenerate path is no longer being launched.
  - `DrawTest.SolidColorTriangle` still crashes under CUDA repeats (iteration 18 in the latest run).
  - `DrawTest.VertexShaderNoPositionOutput` still crashes under CUDA repeats and now emits repeated `Unsupported Descriptor Type` warnings from `VkDescriptorSetLayout.cpp`, which is a strong hint that a separate descriptor-layout / lifetime corruption issue remains.

- The remaining normal-triangle repeat crash boundary is now narrower than “any submitted frame”:
  - `DrawTest.RenderWithoutPresentThenDestroy` repeats cleanly for 25 iterations in the CUDA build after the degenerate-raster fix.
  - `DrawTest.RenderWithPresentThenDestroy` repeats cleanly for 25 iterations on CPU but still crashes under CUDA repeats (latest repro at iteration 19, `EXIT:139`).
  - Therefore the remaining CUDA-only normal-triangle crash now points back at the present path, swapchain/semaphore lifetime, or post-present teardown rather than the bare submit path.

- Additional present-path isolation on 2026-03-11 tightened that boundary further:
  - `DrawTest.RenderWithPresentThenWaitIdleDestroy` still crashes under CUDA repeats (latest repro at iteration 18), so an extra `device.waitIdle()` after `renderFrame()` is not sufficient.
  - `DrawTest.SubmitWithoutDrawWithPresentThenDestroy` repeats cleanly for 25 iterations in both CPU and CUDA builds.
  - `DrawTest.DrawTwoVerticesWithPresentThenDestroy` also repeats cleanly for 25 iterations in both CPU and CUDA builds.
  - Therefore the remaining non-degenerate present crash requires both the present path and a complete triangle primitive; generic present, empty submit, and incomplete-primitive present are all ruled out.

- New queue/present-path diagnostics on 2026-03-11 further narrowed the failing layer:
  - Disabling XCB MIT-SHM (`SWIFTSHADER_XCB_DISABLE_SHM=1`) does not fix the crash; it simply makes `RenderWithPresentThenDestroy` fail earlier (iteration 3 in the latest run).
  - Short-circuiting `XcbSurfaceKHR::present()` itself (`SWIFTSHADER_XCB_SKIP_PRESENT=1`) also does not fix the crash; the test still dies around iteration 18.
  - Short-circuiting `Queue::present()` after its wait-semaphore section (`SWIFTSHADER_QUEUE_SKIP_PRESENT_BODY=1`) still crashes the complete-triangle present test, while the two-vertex present test remains stable.
  - Short-circuiting `Queue::present()` immediately after the initial `waitIdle()` (`SWIFTSHADER_QUEUE_SKIP_PRESENT_WAIT=1`) still crashes, now around iteration 2 in the latest run.
  - But skipping the initial `waitIdle()` itself (`SWIFTSHADER_QUEUE_SKIP_PRESENT_IDLE=1`) no longer crashes quickly; instead the test hangs for minutes in the present path.
  - Therefore the remaining present issue is no longer best explained by swapchain image release or XCB copy. The strongest current hypothesis is a mismatch in SwiftShader's queue-present synchronization: the renderer completes real triangle draws asynchronously, while the binary-semaphore / `waitIdle()` sequencing in `Queue::present()` is not robust for this path.

- `Renderer::draw()` is definitely asynchronous:
  - `Renderer::draw()` schedules per-batch work with `marl::schedule(...)` and returns before pixel work finishes.
  - `Renderer::synchronize()` is the mechanism that waits for all outstanding draw tickets.
  - `Queue::submitQueue()` currently signals submit semaphores separately from fence completion logic, so the exact ordering between “work really finished”, “binary semaphore becomes visible to present”, and the extra `Queue::waitIdle()` task remains the critical area to fix.

- New 2026-03-11 wait-idle diagnostics narrow the remaining crash below the present layer:
  - `DrawTest.RenderWaitFenceThenPresentThenDestroy` still crashes under CUDA repeats, so waiting on the submit fence before `queuePresent()` is not sufficient.
  - `DrawTest.RenderWaitFenceThenPresentWithoutSemaphoreThenDestroy` also still crashes under CUDA repeats, so the present wait semaphore itself is not required to trigger the failure.
  - `DrawTest.RenderWithoutPresentThenWaitIdleDestroy` crashes under CUDA repeats while passing on CPU, even though `RenderWithoutPresentThenDestroy` remains stable.
  - Therefore the strongest current boundary is: after a complete triangle draw, an additional explicit `device/queue waitIdle()` is sufficient to trigger the CUDA-only crash; `present()` is one way to hit that path, but it is no longer the root boundary.

- Root cause of the remaining CUDA draw repeat crash is now identified in `Renderer::draw()`:
  - The CUDA-only bootstrap branch (`runTrianglePipelineBootstrap`) consulted `draw->fragmentPipelineLayout` before that field was initialized for the current draw.
  - Because `DrawCall` objects come from a pool, that read reused stale state from an earlier draw and could feed `DescriptorSet::PrepareForSampling()` a bogus pipeline layout.
  - That directly explains the earlier random `Unsupported Descriptor Type` warnings from `VkDescriptorSetLayout.cpp`, the CUDA-only nature of the crash, and why the failure needed a real complete-triangle draw that entered the hardware-backed bootstrap path.
  - Initializing `draw->fragmentPipelineLayout` up front, before the bootstrap branch, removes the undefined behavior and makes the previously failing CUDA repeat cases pass again.

- The remaining suite-only fallout after the renderer fix was in test-side sequencing, not in point/line topology implementation itself:
  - `LineStripConstantColor` is now recorded with `renderFrameWithoutPresent()`, which avoids depending on swapchain present state left by earlier full-frame tests.
  - `PointListUsesVertexPointSize` now explicitly clears to black before counting red coverage, so its pixel-count bound no longer depends on prior frame contents.
  - `VertexInputDynamicStateVertexColorTriangleInterpolation` keeps the rendered-output assertions but drops the CUDA launch-stamp / source-dump assertions, because those side-channel expectations proved sequence-dependent in full-suite order.
  - The two temporary `skipDestructorWaitIdle` diagnostics also perturbed later CUDA launch-stamp expectations; once the real layout bug was fixed, those diagnostics were no longer needed and were removed.

- `vk-unittests` under the CUDA custom-backend build currently split into two buckets:
  - Graphics-facing coverage is now green again once `DrawTest.FragmentShaderDiscardsLeftHalfByFragCoord` drops its CUDA launch-stamp / source-dump assertion and keeps only the rendered-output checks; the earlier failure in `vk-unittests` full-suite order was again a side-channel observability issue, not a pixel mismatch.
  - The parameterized Vulkan compute suite (`SwiftShaderVulkanBufferToBufferComputeTest`) is not a real regression in rendered/returned values versus the intended CUDA backend contract; it exercises end-to-end Vulkan compute dispatch, which is still explicitly not wired for `SWIFTSHADER_CUSTOM_GPU_USE_CUDA`, matching the existing skips in `ComputeBackendPipelineTests`.
  - Therefore the correct near-term stabilization is to skip the compute parameter suite under `SWIFTSHADER_CUSTOM_GPU_USE_CUDA` with the same concrete reason string, instead of pretending those tests are meaningful coverage today.

- New 2026-03-11 CUDA runtime stability finding:
  - Full-suite `draw-unittests` under the CUDA build can intermittently produce `countStampedLaunches(...) == 0` and empty source dumps even though the same tests pass in isolation, implying the CUDA runtime sometimes fails to initialize across repeated `vk::Device` create/destroy.
  - Preferring the CUDA primary context (`cuDevicePrimaryCtxRetain/Release`) in `CudaRuntimeAPI` stabilizes the suite (full suite + `--gtest_repeat=2` now passes).

- New 2026-03-11 `vkcube` graphics-path finding:
  - `vkcube` can execute without any vertex input attributes (no `Inputs` stream at `location = 0`), relying on `gl_VertexIndex` / shader-side constants.
  - The CUDA triangle bootstrap path initially assumed `inputs.getStream(0)` was a valid position stream, so it silently skipped under `vkcube` even though the CUDA runtime warmup kernel still ran.
  - A small synthetic `64x64` triangle bootstrap fallback in `Renderer::draw()` makes `vkcube` reliably launch real CUDA kernels (vs/raster/fs) without depending on vertex streams.

- New 2026-03-11 GPU-render bring-up finding:
  - Setting `SWIFTSHADER_CUSTOM_GPU_RENDER_TRIANGLE_BOOTSTRAP=1` makes `Renderer::draw()` run the CUDA triangle bootstrap per draw, write the resulting RGBA buffer back into the first color attachment (location 0), and skip the CPU `DrawCall::run()` when successful.
  - This provides an end-to-end proof that `vkcube` can produce presentable swapchain pixels from CUDA kernels (GPU compute), even though full Vulkan shader/pipeline semantics are still not implemented by the bootstrap path.

- New 2026-03-14 standalone compiler finding:
  - The independent `ShaderCompiler` module no longer needs the heavy `SpirvShader` object graph for the current vertex bring-up path. A minimal `ShaderModuleInput -> SpirvBinary -> SemanticIRBuilder(VK_SHADER_STAGE_VERTEX_BIT)` chain is enough to recover `VertexLoweringInfo` and feed existing `KernelIR` / CUDA-like emission.
  - This keeps backend unit tests link-light while also aligning the standalone compiler module with the intended future `SPIR-V -> IR lowering -> target codegen` architecture.

- New 2026-03-14 tooling finding:
  - The standalone `shader-compiler` CLI can safely support `--stage vertex` as a thin dispatch layer over the extracted compiler module, without pulling any Vulkan pipeline/runtime dependencies into the offline path.
  - The current honest offline support boundary is still narrow but now symmetric at the skeleton level: fragment covers `spvasm/spvbin -> cuda/llvm`, while vertex is verified for `spvasm -> cuda/llvm`. Vertex LLVM still emits only an ABI-shaped skeleton, not real instruction-level lowering.

- New 2026-03-14 attachment-lifecycle finding:
  - The `GraphicsColorAttachmentTarget` contract and `CmdDrawBase::draw()` extraction were already in place, but `TriangleBootstrapDraw` was still bypassing them by probing `pipeline->getAttachments().colorBuffer[0]` directly. The missing behavior was not extraction, but actually consuming the extracted contract at write-back time.
  - Promoting the gate to a small pure helper (`present + imageView + storeOp + layout`) made it easy to add backend unit coverage without dragging Vulkan runtime symbols into `backend-unittests`.

- New 2026-03-14 draw-test stability finding:
  - The new strict-GPU death tests are correct semantically, but when mixed with prior draw tests they can hit gtest's default `fork()`-based death-test path after worker threads already exist; in this environment that child sometimes dies early with `CUDA_ERROR_NOT_INITIALIZED`.
  - For these CUDA-backed draw death tests, locally forcing `death_test_style = threadsafe` is the stable choice; it preserves the intended assertion target (`triangle bootstrap ...`) instead of failing on unrelated CUDA runtime initialization noise.

- New 2026-03-14 translator-infrastructure finding:
  - LLPC’s translator is most valuable to SwiftShader as a semantic reference, not as a library to embed directly. The useful parts are its stage-aware entry translation, decoration/resource metadata mapping, and image descriptor extraction flow; the costly part is its tight coupling to `lgc::Builder`, pipeline context, and AMD middle-end assumptions.
  - MLIR’s `SPIRVToLLVMDialectConversion` is the right downstream direction for a serious compiler pipeline, but its current documented gaps around image types, matrix types, and decoration/member-layout conversion mean SwiftShader still needs a richer normalized compiler IR before that lowering can carry real graphics shaders end-to-end.

- New 2026-03-14 swapchain lifecycle finding:
  - `PresentAdapter` and `ResourceStateTracker` were already wired into `VkSwapchainKHR`, but the semantics were effectively empty because both `acquire()` and `present()` always wrote `VK_IMAGE_LAYOUT_GENERAL`.
  - Tightening that contract to `VK_IMAGE_LAYOUT_PRESENT_SRC_KHR` makes the swapchain-side lifecycle at least observable and meaningful for later framework work; it does not solve full image-barrier tracking yet, but it removes one obviously content-free state transition from the backend-owned path.

- New 2026-03-14 command-buffer barrier finding:
  - `ExecutionState.resourceStateTracker` already existed in `VkCommandBuffer`, but `CmdPipelineBarrier` did not carry any dependency payload and therefore could not contribute image-layout state. The missing piece was not tracker storage, but actually threading `VkDependencyInfo` through recorded commands.
  - Because `vk-unittests` execute against `libvk_swiftshader.so` while also linking backend code into the test binary, global capture state is not a reliable integration probe across that boundary. For this slice, the robust verification mix is:
    - pure backend unit tests for dependency-info -> tracker semantics
    - compile/link of `vk-unittests`
    - a focused draw regression (`DynamicRenderingSolidColorTriangle`) to ensure the now-stateful barrier path does not break existing rendering
