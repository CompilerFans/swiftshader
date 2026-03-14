# Implementation: Descriptor-Array Indexing In Texture Bootstrap Plan

## RED
- Update Vulkan pipeline tests to require descriptor-array bootstrap support:
  - `GraphicsBackendPipeline.ExtractsTextureBootstrapBindingForDescriptorArrayIndexZero`
  - `GraphicsBackendPipeline.ExtractsTextureBootstrapBindingForDescriptorArrayIndexOne`
- Extend `PipelineIntrospection` test bridge to expose:
  - `imageArrayElement`
  - `samplerArrayElement`
- Add a strict bootstrap render regression:
  - `DrawTest.TexturedTriangleDescriptorArrayIndexOneBootstrapNearest`
  - Set `SWIFTSHADER_GPU_RENDER_TRIANGLE_BOOTSTRAP=1` + `SWIFTSHADER_GPU_REQUIRE_TRIANGLE_BOOTSTRAP=1` in CUDA builds.

## GREEN
- `src/Backend/GraphicsExecutable.hpp`
  - Extend `GraphicsExecutableTexturePlan` with `imageArrayElement` / `samplerArrayElement`.
- `src/Backend/GraphicsExecutable.cpp`
  - Add a narrow backward-walk helper that extracts constant array indices from `OpAccessChain` / `OpPtrAccessChain` feeding sampled-image values.
  - Store extracted indices in the texture plan.
  - Change bootstrap eligibility from `descriptorCount == 1` to `arrayElement < descriptorCount`, rejecting non-constant indices.
- `src/Backend/TriangleBootstrapDraw.cpp`
  - Materialize descriptor-array elements by offsetting `bindingOffset + arrayElement * descriptorSize`.
  - Add bounds checks against `descriptorCount`.
- `tests/VulkanUnitTests/PipelineIntrospection.hpp/.cpp`
  - Bridge the new plan fields to the tests.
- `tests/VulkanUnitTests/GraphicsBackendPipelineTests.cpp`
  - Add index-one descriptor array pipeline and assertions.
- `tests/VulkanUnitTests/DrawTests.cpp`
  - Add the strict bootstrap render regression with two textures in a combined-sampler descriptor array.

## Verify
- `cmake --build build-cuda-bootstrap --target vk-unittests draw-unittests backend-unittests --parallel 1`
- `./build-cuda-bootstrap/vk-unittests --gtest_filter='GraphicsBackendPipeline.*:BackendSmoke.*:GraphicsBackendSelection.*'`
- `./build-cuda-bootstrap/draw-unittests --gtest_filter='DrawTest.TexturedTriangleDescriptorArrayIndexOneBootstrapNearest'`

---

# 实现：Texture Bootstrap Plan 的 Descriptor-Array Index 支持

## RED
- 修改 Vulkan pipeline tests：descriptor array 在常量 index 时应被识别为可 bootstrap，并能提取 array element：
  - `GraphicsBackendPipeline.ExtractsTextureBootstrapBindingForDescriptorArrayIndexZero`
  - `GraphicsBackendPipeline.ExtractsTextureBootstrapBindingForDescriptorArrayIndexOne`
- 扩 `PipelineIntrospection` 暴露：
  - `imageArrayElement`
  - `samplerArrayElement`
- 增加 strict bootstrap render 回归：
  - `DrawTest.TexturedTriangleDescriptorArrayIndexOneBootstrapNearest`
  - CUDA build 下设置 `SWIFTSHADER_GPU_RENDER_TRIANGLE_BOOTSTRAP=1` + `SWIFTSHADER_GPU_REQUIRE_TRIANGLE_BOOTSTRAP=1` 强制走 triangle bootstrap writeback。

## GREEN
- `src/Backend/GraphicsExecutable.hpp`
  - 在 `GraphicsExecutableTexturePlan` 增加 `imageArrayElement` / `samplerArrayElement`。
- `src/Backend/GraphicsExecutable.cpp`
  - 增加窄的向后回溯 helper：从 sample operand 的 value chain 中提取 `OpAccessChain` / `OpPtrAccessChain` 常量 index。
  - 把 index 写回 plan。
  - bootstrap 资格从 `descriptorCount == 1` 放宽为 “`arrayElement < descriptorCount`”，并拒绝 non-constant index。
- `src/Backend/TriangleBootstrapDraw.cpp`
  - 按 `bindingOffset + arrayElement * descriptorSize` 取 descriptor element。
  - 加 `descriptorCount` bounds check。
- `tests/VulkanUnitTests/PipelineIntrospection.hpp/.cpp`
  - 把 plan 字段桥接到测试。
- `tests/VulkanUnitTests/GraphicsBackendPipelineTests.cpp`
  - 增加 index-one descriptor array pipeline 与断言。
- `tests/VulkanUnitTests/DrawTests.cpp`
  - 增加 strict bootstrap render 回归：combined sampler descriptor array 中放两张纹理，shader 采样 `texSampler[1]` 并验证输出颜色。

## 验证
- `cmake --build build-cuda-bootstrap --target vk-unittests draw-unittests backend-unittests --parallel 1`
- `./build-cuda-bootstrap/vk-unittests --gtest_filter='GraphicsBackendPipeline.*:BackendSmoke.*:GraphicsBackendSelection.*'`
- `./build-cuda-bootstrap/draw-unittests --gtest_filter='DrawTest.TexturedTriangleDescriptorArrayIndexOneBootstrapNearest'`
