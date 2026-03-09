# Texture Bootstrap Design
# 纹理贴图 Bootstrap 设计

## Goal / 目标
- Add a minimal but real texture sampling path to the current CUDA bootstrap graphics backend.
- 为当前 CUDA bootstrap 图形后端增加一条最小但真实的纹理采样路径。
- Scope is intentionally narrow: one `combined image sampler`, one interpolated `vec2 uv`, one `texture()` sample, one color output.
- 范围刻意收窄：只支持一个 `combined image sampler`、一个插值 `vec2 uv`、一次 `texture()` 采样、一个颜色输出。

## Why this scope / 为什么选这个范围
- The repo already has a real Vulkan texture benchmark and descriptor/image setup in `tests/VulkanBenchmarks/TriangleBenchmarks.cpp`.
- 仓库里已经有真实 Vulkan 纹理 benchmark 以及 descriptor/image 建设路径，位于 `tests/VulkanBenchmarks/TriangleBenchmarks.cpp`。
- The current bootstrap already supports position, color, barycentrics, `PointCoord`, and narrow fragment modes.
- 当前 bootstrap 已支持 position、color、barycentric、`PointCoord` 和若干窄片元模式。
- The shortest useful next step is therefore not generic descriptor lowering, but a narrow texture family that reuses existing descriptor/image infrastructure.
- 因此下一步最有价值的不是通用 descriptor lowering，而是一条复用现有 descriptor/image 基础设施的窄纹理家族。

## Approaches / 方案对比
### Option A: Fake texture path / 伪纹理路径
- Hard-code texels in bootstrap tests only.
- 只在 bootstrap 测试里硬编码纹素。
- Fast, but does not prove descriptor/image integration.
- 快，但无法证明 descriptor/image 集成。

### Option B: Narrow real texture family / 窄而真实的纹理家族
- Reuse real Vulkan descriptor/image/sampler setup.
- 复用真实 Vulkan descriptor/image/sampler 建设。
- Support one sampled texture path with limited sampler semantics.
- 只支持一条有限 sampler 语义的 sampled texture 路径。
- Recommended.
- 推荐采用。

### Option C: Generic descriptor/image lowering / 通用 descriptor/image lowering
- Too broad for the current stage.
- 对当前阶段过大。

## Chosen design / 选定设计
- Detect a narrow shader family:
- 检测一类窄 shader 家族：
  - VS writes `gl_Position` and forwards `location 1` `vec2` to fragment `location 0`.
  - VS 输出 `gl_Position`，并把 `location 1` 的 `vec2` 传到片元 `location 0`。
  - FS samples exactly one `combined image sampler` via `texture(tex, uv)` and writes `location 0` color.
  - FS 通过 `texture(tex, uv)` 采样一个 `combined image sampler`，并写 `location 0` 颜色。
- Extend bootstrap data flow:
- 扩展 bootstrap 数据流：
  - `GraphicsBootstrapVertexOutput` carries `u/v`.
  - `GraphicsBootstrapVertexOutput` 携带 `u/v`。
  - `FragmentBootstrapConfig` carries per-vertex `uv` and a narrow texture descriptor.
  - `FragmentBootstrapConfig` 携带每顶点 `uv` 和一份窄纹理描述。
  - `FragmentBootstrap` uploads texture bytes to device memory and samples in `fs_main`.
  - `FragmentBootstrap` 把纹理字节上传到 device memory，并在 `fs_main` 中采样。
- Keep texture format limited to `RGBA8 UNORM`, sampler limited to `nearest/linear` and `clamp-to-edge/repeat`, `LOD 0` only.
- 纹理格式先限制为 `RGBA8 UNORM`，sampler 先只支持 `nearest/linear` 与 `clamp-to-edge/repeat`，只做 `LOD 0`。

## Integration points / 接入点
- `src/Backend/GraphicsBootstrap.*`
  - Add `u/v` varying output.
  - 增加 `u/v` varying 输出。
- `src/Backend/FragmentBootstrap.*`
  - Add `Texture2DColor` shader kind and narrow sampler/image params.
  - 增加 `Texture2DColor` 模式和窄 sampler/image 参数。
- `src/Backend/TrianglePipelineBootstrap.*`
  - Carry per-vertex `uv` through barycentric interpolation contract.
  - 通过 barycentric 契约传递每顶点 `uv`。
- `src/Device/Renderer.cpp`
  - Detect the narrow texture shader family and extract descriptor-backed texture data.
  - 识别窄纹理 shader 家族，并提取 descriptor 绑定的纹理数据。
- `tests/VulkanUnitTests/DrawTests.cpp`
  - Add textured triangle tests with BMP dump.
  - 增加带 BMP dump 的纹理三角形测试。

## Tests / 测试
- `TexturedTriangleNearest`
  - Checkerboard texture, nearest sampling, stable pixel assertions.
  - 棋盘纹理，nearest 采样，稳定像素断言。
- `IndexedTexturedTriangle`
  - Same path with indexed draw.
  - 同路径的 indexed draw。
- All new draw tests must dump images under `draw-test-artifacts/`.
- 所有新增 draw 测试都必须输出图片到 `draw-test-artifacts/`。

## Deferred / 延后事项
- Multiple textures / multiple descriptor sets
- 多纹理 / 多 descriptor set
- Mipmaps / derivatives / explicit LOD
- mipmap / 导数 / 显式 LOD
- Storage image / image writes
- storage image / image write
- General SPIR-V image lowering
- 通用 SPIR-V image lowering
