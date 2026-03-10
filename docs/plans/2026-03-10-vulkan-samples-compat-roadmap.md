# Vulkan Samples Compatibility Roadmap
# Vulkan Samples 兼容支持路线图

## Goal / 目标
- Use the local `~/gfx/Vulkan-Samples` programs as an external acceptance ladder for the SwiftShader + CUDA backend work.
- 把本地 `~/gfx/Vulkan-Samples` 作为 SwiftShader + CUDA 后端演进的外部验收阶梯。
- Prioritize only samples that stay within the `VS + PS + core graphics` scope currently under development.
- 优先处理当前开发范围内的 `VS + PS + 核心图形路径` 样例。

## Current Support Snapshot / 当前支持快照
### Already covered or mostly covered / 已覆盖或大体覆盖
- `hello_triangle`
  - surface / swapchain / render pass / basic graphics pipeline / present / sync are already exercised by visible benchmarks and draw tests.
  - surface / swapchain / render pass / 基础图形管线 / present / 同步，现已通过可见 benchmark 与 draw tests 间接覆盖。
- `hello_triangle_1_3`
  - now runs headless against SwiftShader (`vulkan_samples sample hello_triangle_1_3 --headless-surface ...`), so dynamic rendering is externally validated at least for the baseline triangle path.
  - 现在可以在 SwiftShader 上以 headless 方式运行（`vulkan_samples sample hello_triangle_1_3 --headless-surface ...`），因此 dynamic rendering 至少已在“基础三角形”路径上获得外部样例验证。
- `separate_image_sampler`
  - now runs headless against SwiftShader (`vulkan_samples sample separate_image_sampler --headless-surface ...`).
  - 现在可以在 SwiftShader 上以 headless 方式运行（`vulkan_samples sample separate_image_sampler --headless-surface ...`）。
- `dynamic_uniform_buffers`
  - now runs headless against SwiftShader (`vulkan_samples sample dynamic_uniform_buffers --headless-surface ...`).
  - 现在可以在 SwiftShader 上以 headless 方式运行（`vulkan_samples sample dynamic_uniform_buffers --headless-surface ...`）。
- `texture_loading`
  - narrow combined-image-sampler path is implemented and now validated for static textured draws, cross-frame descriptor updates without command-buffer re-recording, instanced textured draws, and the same instanced-texture path under `vkCmdSetVertexInputEXT`.
  - 窄 `combined image sampler` 路径已经实现，并且现在不仅覆盖静态纹理绘制、跨帧更新 descriptor 后切换采样纹理、普通实例化贴图绘制，还覆盖了同一路径在 `vkCmdSetVertexInputEXT` 下的实例化贴图绘制。
- `instancing`
  - `gl_InstanceIndex`, `VK_VERTEX_INPUT_RATE_INSTANCE`, and a combined `indexed + baseVertex + firstInstance` draw path now all have repo-local coverage.
  - `gl_InstanceIndex`、`VK_VERTEX_INPUT_RATE_INSTANCE`，以及组合了 `indexed + baseVertex + firstInstance` 的 draw 路径现在都已有仓库内覆盖。
- `swapchain_images`
  - swapchain/image-count visible paths are exercised by benchmark windows, and repo-local draw coverage now includes both a configurable `minImageCount` case that requests triple buffering and a multi-frame color-cycle case that reuses that triple-buffered swapchain across successive presents.
  - swapchain/image-count 的可见路径已被 benchmark 窗口覆盖，且仓库内 draw 覆盖现在包含一个可配置 `minImageCount` 的三缓冲用例，以及一个在同一三缓冲 swapchain 上连续 present 多帧并轮转颜色的用例。

### Partial / 部分覆盖
- `vertex_dynamic_state`
  - the repository now has repo-local `vkCmdSetVertexInputEXT` coverage for a solid-color triangle, a multi-attribute interpolated-color triangle, an `instance-rate` offset case, a combined `indexed + baseVertex + firstInstance + instance-rate` path, and a textured instanced draw; broader sample-style coverage still needs more formats/topologies.
  - 仓库内已经有 `vkCmdSetVertexInputEXT` 的纯色三角形、多 attribute 颜色插值三角形、`instance-rate` 偏移组合、`indexed + baseVertex + firstInstance + instance-rate` 组合路径，以及贴图实例化绘制覆盖；更广的样例风格覆盖仍需补更多格式/拓扑。
- `dynamic_rendering`
  - repo-local draw coverage exists via `DrawTest.DynamicRenderingSolidColorTriangle`, and Vulkan Samples `hello_triangle_1_3` runs headless against SwiftShader.
  - 仓库内已有 `DrawTest.DynamicRenderingSolidColorTriangle` 的 repo-local 覆盖，且 Vulkan Samples `hello_triangle_1_3` 已可在 SwiftShader 上 headless 跑通。
- `render_passes`
  - Render pass basics exist, and repo-local coverage now includes `vkCmdClearAttachments` for both color and depth aspects, `loadOp = LOAD` preservation for both color and depth attachments, and narrow two-subpass paths that validate both color overlay and depth blocking through `vkCmdNextSubpass`; broader sample-driven validation for storeOp and more complex subpass/layout patterns is still missing.
  - render pass 基础已有，且仓库内已经覆盖了活动 render pass 内针对 color/depth 的 `vkCmdClearAttachments`、color/depth attachment 上 `loadOp = LOAD` 的保留语义，以及通过 `vkCmdNextSubpass` 的窄双 subpass 颜色叠加与深度阻挡路径；但 storeOp 和更复杂的 subpass/layout 模式仍待补齐。
- `msaa`
  - multisample paths exist in harnesses/benchmarks, and repo-local draw coverage now includes a resolved solid-color triangle, a resolved interpolated-color triangle, and a depth-tested case where a nearer triangle blocks a farther one through the multisample depth attachment.
  - multisample 路径在 harness/benchmark 中存在，且仓库内 draw 覆盖已经包含 resolve 后的纯色三角形、插值彩色三角形，以及通过多重采样深度附件阻挡更远三角形的深度测试用例。

### Missing from current plan / 当前计划中缺失
- `texture_mipmap_generation`
  - mip generation, blit/copy-driven mip chains, and LOD-sensitive sampling are not yet covered.
  - 尚未覆盖 mip 生成、blit/copy 驱动的 mip 链和 LOD 敏感采样。
- `dynamic_blending`
  - no sample-driven blend support or dynamic blend-state plan yet.
  - 还没有 blend 支持或动态 blend-state 的样例驱动计划。

## Recommended Execution Order / 推荐执行顺序
### Phase 1: Sample-baseline gates / 样例基线门槛
1. `hello_triangle`
2. `swapchain_images`
3. `render_passes`

Reason:
- These establish WSI, present, render pass, and attachment/layout correctness before resource-heavy features.
- 先把 WSI、present、render pass、attachment/layout 的正确性打稳，再上资源型特性。

### Phase 2: Resource binding ladder / 资源绑定阶梯
4. `texture_loading`
5. `separate_image_sampler`
6. `dynamic_uniform_buffers`

Reason:
- This grows from combined sampler → split image/sampler → uniform-buffer indirection.
- 按 `combined sampler -> 分离 image/sampler -> uniform buffer 间接寻址` 递进。

### Phase 3: Texture and sample complexity / 纹理与采样复杂度
7. `texture_mipmap_generation`
8. `msaa`

Reason:
- These add mip chains, blits, resolves, and sample-count-sensitive behavior.
- 这阶段引入 mip 链、blit、resolve 以及 sample-count 敏感行为。

### Phase 4: Dynamic-state samples / 动态状态样例
9. `dynamic_rendering`
10. `vertex_dynamic_state`
11. `dynamic_blending`

Reason:
- These depend on earlier correctness in attachments, descriptors, and resource lifetime.
- 它们依赖前面的 attachment、descriptor、资源生命周期正确性。

## Practical Mapping to Current Work / 与当前开发主线的对接
- `noperspective` now has repo-local coverage (`DrawTest.FragmentShaderUsesNoPerspectiveColor`); treat it as a small correctness gate, not a sample-ladder item.
- `noperspective` 现在已有仓库内覆盖（`DrawTest.FragmentShaderUsesNoPerspectiveColor`）；把它视为小的正确性 gate，而不是样例阶梯项。
- Immediate next milestones should be:
- 近期下一步建议是：
  1. run `texture_mipmap_generation` against SwiftShader
  2. run `dynamic_blending` against SwiftShader

## Acceptance Rule / 验收规则
For each sample family:
- 每个样例族都应满足：
- one repo-local draw or benchmark test first
- 先有一个仓库内的 draw/benchmark 对应用例
- then sample execution against SwiftShader
- 再跑样例本体验证 SwiftShader
- only then mark the capability as “covered”
- 之后才可标记为“已覆盖”
