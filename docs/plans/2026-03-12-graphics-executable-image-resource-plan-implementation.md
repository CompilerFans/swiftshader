# Implementation: Image Resource Plan (Sampled + Storage)

## RED
- Add a Vulkan pipeline regression that previously false-positived as texture-bootstrap-supported:
  - `GraphicsBackendPipeline.RejectsTextureBootstrapBindingWhenFragmentHasStorageImageWrite`
  - Fragment shader performs `imageStore(...)` but still outputs a direct sampled color.
- Extend the Vulkan test bridge with a new image resource query:
  - `GraphicsPipelineImageResourceState graphicsPipelineImageResourceState(uintptr_t pipelineHandle)`
- Add Vulkan pipeline tests that require the image plan to be present and correctly counted:
  - `GraphicsBackendPipeline.ExtractsImageResourcePlanForCombinedTexturePipeline`
  - `GraphicsBackendPipeline.ExtractsImageResourcePlanForSeparateImageSamplerPipeline`
  - `GraphicsBackendPipeline.ExtractsImageResourcePlanForStorageImageWritePipeline`

## GREEN
- `src/Backend/GraphicsExecutable.hpp`
  - Add `GraphicsExecutableDescriptorRef` and `GraphicsExecutableImageResourcePlan`.
  - Add `hasImageResourcePlan()` / `imageResourcePlan()` accessors.
- `src/Backend/GraphicsExecutable.cpp`
  - Add storage-image provenance collection by scanning `OpImageRead` / `OpImageWrite`.
  - Build and store the image resource plan during executable creation.
  - Gate `GraphicsExecutableTexturePlan::bootstrapSupported` on absence of storage-image ops.
- `tests/VulkanUnitTests/PipelineIntrospection.hpp/.cpp`
  - Add `GraphicsPipelineImageResourceState`.
  - Bridge the new executable image plan to tests (counts + first storage binding/type).
- `tests/VulkanUnitTests/GraphicsBackendPipelineTests.cpp`
  - Add pipeline configuration for `imageStore` + direct texture sample output.
  - Add plan-level assertions for sampled/storage descriptor counts and binding location.

## Verify
- `cmake --build build-cuda-bootstrap --target vk-unittests backend-unittests draw-unittests --parallel 1`
- `./build-cuda-bootstrap/vk-unittests --gtest_filter='GraphicsBackendPipeline.*:BackendSmoke.*:GraphicsBackendSelection.*'`
- `./build-cuda-bootstrap/backend-unittests --gtest_filter='GraphicsExecutable.*'`
- `./build-cuda-bootstrap/draw-unittests --gtest_filter='DrawTest.TexturedTriangleNearest'`
- `git diff --check`

---

# 实现：Image Resource Plan（Sampled + Storage）

## RED
- 增加 Vulkan pipeline 回归：此前会被误判为 texture-bootstrap-supported 的 shader 必须被拒绝：
  - `GraphicsBackendPipeline.RejectsTextureBootstrapBindingWhenFragmentHasStorageImageWrite`
  - fragment shader 包含 `imageStore(...)` side-effect，但仍输出 direct sampled color。
- 扩 Vulkan 测试 bridge，新增 image resource 查询：
  - `GraphicsPipelineImageResourceState graphicsPipelineImageResourceState(uintptr_t pipelineHandle)`
- 增加 Vulkan pipeline tests：要求 image plan 存在且计数正确：
  - `GraphicsBackendPipeline.ExtractsImageResourcePlanForCombinedTexturePipeline`
  - `GraphicsBackendPipeline.ExtractsImageResourcePlanForSeparateImageSamplerPipeline`
  - `GraphicsBackendPipeline.ExtractsImageResourcePlanForStorageImageWritePipeline`

## GREEN
- `src/Backend/GraphicsExecutable.hpp`
  - 增加 `GraphicsExecutableDescriptorRef` 与 `GraphicsExecutableImageResourcePlan`。
  - 增加 `hasImageResourcePlan()` / `imageResourcePlan()` 访问入口。
- `src/Backend/GraphicsExecutable.cpp`
  - 扫描 `OpImageRead` / `OpImageWrite`，提取 storage-image provenance 并收集 descriptor refs。
  - 在 executable 创建期构建并保存 image resource plan。
  - 只要检测到 storage-image ops，就必须把 `GraphicsExecutableTexturePlan::bootstrapSupported` 标记为 false。
- `tests/VulkanUnitTests/PipelineIntrospection.hpp/.cpp`
  - 增加 `GraphicsPipelineImageResourceState`。
  - 把 executable 的 image plan 桥接到 Vulkan tests（计数 + 首个 storage binding/type）。
- `tests/VulkanUnitTests/GraphicsBackendPipelineTests.cpp`
  - 增加 `imageStore` + direct sample 输出 pipeline 配置。
  - 增加 sampled/storage descriptor 计数与 binding 位置断言。

## 验证
- `cmake --build build-cuda-bootstrap --target vk-unittests backend-unittests draw-unittests --parallel 1`
- `./build-cuda-bootstrap/vk-unittests --gtest_filter='GraphicsBackendPipeline.*:BackendSmoke.*:GraphicsBackendSelection.*'`
- `./build-cuda-bootstrap/backend-unittests --gtest_filter='GraphicsExecutable.*'`
- `./build-cuda-bootstrap/draw-unittests --gtest_filter='DrawTest.TexturedTriangleNearest'`
- `git diff --check`

