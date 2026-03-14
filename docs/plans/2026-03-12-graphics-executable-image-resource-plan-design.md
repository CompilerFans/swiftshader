# Design: Image Resource Plan (Sampled + Storage)

## Goal
Model both sampled-image and storage-image descriptor usage inside `GraphicsExecutable`, so the backend can reason about a fragment shader's image resources without collapsing everything into texture-bootstrap compatibility.

## Non-Goals
- Implementing full GPU draw execution or a complete backend resource model.
- Emulating storage-image read/write behavior in triangle bootstrap.
- Supporting dynamic / non-uniform descriptor indexing in bootstrap.

## Problem
The existing narrow triangle-bootstrap path is able to materialize a `Texture2DColor` config for simple sampled-image shaders. But without explicitly modeling storage-image operations:
- A fragment shader can still be classified as bootstrap-compatible based on its output path even if it performs `imageStore` side effects.
- In strict GPU mode, triangle bootstrap would silently skip those storage-image side effects while still writing the color attachment, producing incorrect behavior that can be hard to diagnose.

Separately, a sampled-image-only plan is not sufficient as the long-term metadata carrier for image resources: storage images and input attachments need an explicit place in pipeline-time metadata if we want broader GPU backend bring-up without re-scanning SPIR-V at draw time.

## Approach
1. Introduce an image resource plan in `GraphicsExecutable`:
   - `sampledDescriptors`: descriptor bindings that feed sampled-image operations used by the fragment shader's texture sampling provenance (combined image sampler, separate sampled image, separate sampler).
   - `storageDescriptors`: descriptor bindings that feed storage-image read/write operations (`VK_DESCRIPTOR_TYPE_STORAGE_IMAGE` and `VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT`).
   - Each descriptor ref carries `set`, `binding`, constant `arrayElement` (default `0`), `descriptorType`, and `descriptorCount`.
2. Extract provenance from SPIR-V:
   - Reuse the existing sampled-image provenance walk for texture sampling instructions.
   - Add a storage-image provenance scan for `OpImageRead` / `OpImageWrite`.
3. Tighten bootstrap eligibility for correctness:
   - If the fragment shader contains any storage-image read/write operation, mark texture bootstrap as unsupported even if the output path is a direct sampled-color passthrough.
4. Expose plan data to tests via `PipelineIntrospection`:
   - Keep Vulkan tests decoupled from backend internals by only querying counts and first binding info through the bridge.

## Testing
- `GraphicsBackendPipeline.RejectsTextureBootstrapBindingWhenFragmentHasStorageImageWrite`
  - Ensures bootstrap support is rejected for a direct-sample shader that also performs `imageStore`.
- `GraphicsBackendPipeline.ExtractsImageResourcePlanFor*`
  - Verifies sampled descriptor counts for combined/separate plans and storage descriptor discovery for storage-image-write shaders.

---

# 设计：Image Resource Plan（Sampled + Storage）

## 目标
在 `GraphicsExecutable` 中同时建模 sampled-image 与 storage-image 的 descriptor 使用情况，让后端能在 pipeline-time 获得明确的 image 资源元数据，而不是把所有信息挤进“是否支持 texture bootstrap”的一个布尔值。

## 非目标
- 直接实现完整 GPU draw 执行或完整的 backend resource model。
- 让 triangle bootstrap 去模拟 storage image 的 read/write 语义。
- 在 bootstrap 下支持 dynamic / non-uniform descriptor indexing。

## 问题
当前 triangle bootstrap 的窄路径可以为简单 sampled-image shader 物化 `Texture2DColor` config。但如果不显式建模 storage image：
- fragment shader 即使含 `imageStore` side-effect，也可能仅凭输出路径被误判为 bootstrap-compatible。
- strict GPU 模式下 triangle bootstrap 会静默跳过 storage image side-effect，却仍然写回 color attachment，导致难以诊断的错误。

同时，长远来看只有 sampled-image plan 也不够：storage image / input attachment 必须在 pipeline-time 有明确的元数据载体，否则后续 bring-up 必然会回到 draw-time 扫 SPIR-V 的错层做法。

## 方案
1. 在 `GraphicsExecutable` 中新增 image resource plan：
   - `sampledDescriptors`：fragment shader 的 sampled-image 相关 descriptors（combined image sampler、separate sampled image、separate sampler）。
   - `storageDescriptors`：fragment shader 的 storage-image read/write 相关 descriptors（`VK_DESCRIPTOR_TYPE_STORAGE_IMAGE` / `VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT`）。
   - descriptor ref 记录 `set`/`binding`、常量 `arrayElement`（缺省 `0`）、`descriptorType` 与 `descriptorCount`。
2. 从 SPIR-V 提取 provenance：
   - sampled-image 继续复用现有的 texture-sampling provenance 回溯。
   - storage-image 通过扫描 `OpImageRead` / `OpImageWrite` 建 plan。
3. 收紧 bootstrap 资格，保证正确性：
   - 一旦 fragment shader 含任何 storage-image read/write 操作，即使输出路径是 direct-sample passthrough，也必须将 texture bootstrap 标记为 unsupported。
4. 通过 `PipelineIntrospection` 暴露给测试：
   - Vulkan tests 只通过 bridge 读取计数与首个 binding 信息，不直接依赖 backend 内部结构。

## 测试
- `GraphicsBackendPipeline.RejectsTextureBootstrapBindingWhenFragmentHasStorageImageWrite`
  - direct-sample + `imageStore` 的 shader 必须拒绝 bootstrap。
- `GraphicsBackendPipeline.ExtractsImageResourcePlanFor*`
  - 验证 combined/separate 的 sampled descriptor 计数，以及 storage-image-write shader 的 storage descriptor 提取。

