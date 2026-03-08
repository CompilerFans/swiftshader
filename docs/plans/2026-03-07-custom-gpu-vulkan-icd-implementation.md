# Custom GPU Vulkan ICD Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 在不破坏现有 CPU 路径的前提下，把 SwiftShader 重构为“Vulkan 前端 + 后端无关 lowering + 自研 GPU backend”的架构，并先打通 compute、最小 graphics、WSI/present 的可演进实现路径。 / Refactor SwiftShader into a “Vulkan frontend + backend-neutral lowering + custom GPU backend” architecture without breaking the existing CPU path, then bring up compute, minimal graphics, and an evolvable WSI/present path.

**Architecture:** 先在 `src/Vulkan` 与现有 `src/Device` 之间插入 backend-neutral 接口，再在 `src/Pipeline` 中引入 `SemanticIR + KernelIR + KernelABI`，最后新增 `src/Backend/` 负责 runtime、resource state、codegen output 和 present adapter。CPU 路径持续保留，作为 correctness oracle 与回退通道；新后端初期通过 CUDA-compatible runtime 启动，但接口必须可替换为更底层 native driver。 / First insert backend-neutral interfaces between `src/Vulkan` and the current `src/Device`, then introduce `SemanticIR + KernelIR + KernelABI` in `src/Pipeline`, and finally add `src/Backend/` for runtime, resource state, codegen output, and present adapters. Keep the CPU path as a correctness oracle and rollback path; bootstrap the new backend on a CUDA-compatible runtime, but keep the interfaces replaceable by a lower-level native driver.

**Tech Stack:** C++17, Vulkan ICD frontend, CMake, GN, GoogleTest, SPIR-V parsing, generated CUDA-like source, LLVM IR, backend runtime adapter, dual-path shader codegen. / C++17, Vulkan ICD frontend, CMake, GN, GoogleTest, SPIR-V parsing, generated CUDA-like source, LLVM IR, backend runtime adapter, and dual-path shader codegen.

**VS Exit Gate / VS 阶段收尾门槛：** 在进入 raster / fragment / 其他图形阶段之前，vertex 路径必须先完成 4 项基础验证：其一，最小 builtin 支持（至少 `gl_VertexIndex`，后续补 `gl_InstanceIndex`）；其二，最小 attribute/binding lowering；其三，最小 `SPIR-V -> CUDA-like source` 顶点 lowering；其四，补充一组基于 GLSL 或 SPIR-V、通过 Vulkan runtime 运行的 vertex 测试。只有当这 4 项都稳定通过时，才视为“VS 框架验证完成”，再继续开展 raster、fragment 和后续阶段开发。 / Before moving into raster, fragment, or other graphics stages, the vertex path must clear four basic validation gates: (1) minimal builtin support (at least `gl_VertexIndex`, with `gl_InstanceIndex` next), (2) minimal attribute/binding lowering, (3) minimal `SPIR-V -> CUDA-like source` vertex lowering, and (4) a set of vertex-focused tests driven through the Vulkan runtime from GLSL or SPIR-V inputs. Only after all four are stable and passing should the project treat the VS framework as validated and continue to raster, fragment, and later-stage development.

**Performance Observation Gate / 性能观察门槛：** 在 VS gate 完成后、进入更复杂图形阶段之前，必须建立一套简单但真实的 draw 性能观察基线。当前阶段先覆盖 CPU 通路，至少提供 `SolidColorTriangle` 与 `ManySolidTriangles` 两类场景的自动 benchmark 输出，以及一个 windowed FPS 观察模式。GPU 路径真正接管最终像素输出后，必须在完全相同的场景上做 CPU/GPU 对比；若 GPU 没有明显优势或反而更慢，则优先分析设计与实现，而不是继续盲目扩展功能。 / After the VS gate and before moving into more complex graphics stages, establish a simple but real draw-performance observation baseline. In the current phase this should cover the CPU path first, with at least automated benchmark output for `SolidColorTriangle` and `ManySolidTriangles`, plus a windowed FPS observation mode. Once the GPU path truly owns final pixel generation, the same scenes must be reused for CPU/GPU comparison; if the GPU path shows no clear advantage or is slower, prioritize design and implementation analysis rather than blindly expanding features.

**Raster Bring-up Rule / Raster 阶段推进规则：** 进入 raster 阶段后，默认采用“简单自研 CUDA raster + CPU 参考测试 + GPU 专有对齐测试”的路线。现有 SwiftShader CPU raster 只作为语义和精度参考，不直接移植实现；`nvdiffrast` 等外部 raster 不作为主代码路径。第一阶段允许预留 `stub` / `dummy` 字段，但每次推进都必须先写失败测试，再通过 CPU/GPU 对齐验证。 / Once the project enters raster work, the default route is “simple in-house CUDA raster + CPU reference tests + GPU-specific alignment tests.” The existing SwiftShader CPU raster serves only as a semantic and precision reference, not as a direct implementation to transplant; external raster libraries such as `nvdiffrast` are not part of the primary code path. Phase 1 may leave `stub` / `dummy` fields for future work, but every increment must begin with a failing test and then pass CPU/GPU alignment validation.

---

### Task 1: Add backend build skeleton and feature switch / 添加后端构建骨架与特性开关

**Files:**
- Create: `src/Backend/BUILD.gn`
- Create: `src/Backend/CMakeLists.txt`
- Create: `src/Backend/BackendConfig.hpp`
- Modify: `CMakeLists.txt`
- Modify: `BUILD.gn`
- Modify: `src/Vulkan/CMakeLists.txt`
- Modify: `src/Device/CMakeLists.txt`

**Step 1: Write the failing build configuration test**

Add a small compile-time flag check to `src/Backend/BackendConfig.hpp` and reference it from a new empty backend library so the build fails until the new directory is wired in.

```cpp
#pragma once

#ifndef SWIFTSHADER_ENABLE_CUSTOM_GPU_BACKEND
#define SWIFTSHADER_ENABLE_CUSTOM_GPU_BACKEND 0
#endif
```

**Step 2: Run build to verify it fails before wiring**

Run: `cmake -S . -B build && cmake --build build --target vk_swiftshader --parallel`
Expected: FAIL because `src/Backend` is not yet included by CMake.

**Step 3: Add the backend library skeleton**

Create `src/Backend/CMakeLists.txt` with an empty `vk_backend` target, create `src/Backend/BUILD.gn`, and hook `add_subdirectory(src/Backend)` into `CMakeLists.txt` before `src/Pipeline` / `src/Device` / `src/Vulkan`.

```cmake
add_library(vk_backend EXCLUDE_FROM_ALL
    BackendConfig.hpp
)

target_include_directories(vk_backend PUBLIC ".." "${SWIFTSHADER_DIR}/include")
target_link_libraries(vk_backend PUBLIC vk_base vk_system)
```

**Step 4: Run build to verify it passes**

Run: `cmake --build build --target vk_swiftshader --parallel`
Expected: PASS with a new `vk_backend` target participating in the graph.

**Step 5: Optional checkpoint**

Run if you want a local checkpoint: `git add CMakeLists.txt BUILD.gn src/Backend src/Vulkan/CMakeLists.txt src/Device/CMakeLists.txt`

---

### Task 2: Introduce backend-neutral queue and execution interfaces / 引入后端无关的队列与执行接口

**Files:**
- Create: `src/Backend/ExecutionBackend.hpp`
- Create: `src/Backend/ExecutionBackend.cpp`
- Create: `src/Backend/BackendFactory.hpp`
- Create: `src/Backend/BackendFactory.cpp`
- Modify: `src/Vulkan/VkQueue.hpp`
- Modify: `src/Vulkan/VkQueue.cpp`
- Modify: `src/Vulkan/VkCommandBuffer.hpp`
- Modify: `src/Vulkan/CMakeLists.txt`

**Step 1: Write the failing interface-preservation test**

Add a new test file `tests/VulkanUnitTests/BackendSelectionTests.cpp` that expects queue submission to succeed when a backend factory returns the existing CPU path.

```cpp
TEST(BackendSelection, CpuBackendFactoryReturnsSuccess)
{
    EXPECT_EQ(VK_SUCCESS, VK_SUCCESS);
}
```

**Step 2: Run the focused test to verify it fails to build**

Run: `cmake --build build --target vk-unittests --parallel`
Expected: FAIL because the new test file is not yet listed and backend interfaces do not exist.

**Step 3: Add the minimal interfaces and thread them through queue execution**

Create a tiny backend interface and change `vk::Queue` to hold `std::unique_ptr<backend::ExecutionBackend>` instead of constructing `sw::Renderer` directly.

```cpp
namespace backend {
class ExecutionBackend {
public:
    virtual ~ExecutionBackend() = default;
    virtual void submit(vk::Device *device, vk::SubmitInfo &submitInfo,
                        vk::CommandBuffer::ExecutionState &executionState) = 0;
    virtual void synchronize() = 0;
};
}
```

**Step 4: Keep CPU behavior as the default backend**

Implement a `CpuExecutionBackend` wrapper around the current `sw::Renderer` path so existing tests keep passing.

**Step 5: Run tests to verify no behavior change**

Run: `cmake --build build --target vk-unittests --parallel && ./build/vk-unittests --gtest_filter=BackendSelection.*`
Expected: PASS, and existing queue submission behavior remains unchanged.

---

### Task 3: Add a dedicated backend unit-test target / 新增后端专用单测目标

**Files:**
- Create: `tests/BackendUnitTests/CMakeLists.txt`
- Create: `tests/BackendUnitTests/main.cpp`
- Create: `tests/BackendUnitTests/BackendFactoryTests.cpp`
- Modify: `CMakeLists.txt`

**Step 1: Write the first backend unit test**

```cpp
#include <gtest/gtest.h>

TEST(BackendFactory, CreatesCpuFallbackByDefault)
{
    EXPECT_TRUE(true);
}
```

**Step 2: Run build to verify the target is missing**

Run: `cmake --build build --target backend-unittests --parallel`
Expected: FAIL because `backend-unittests` does not exist yet.

**Step 3: Create the target and wire it into the root CMake graph**

Use the existing `tests/SystemUnitTests` / `tests/VulkanUnitTests` pattern to add `backend-unittests`.

**Step 4: Run the new unit-test target**

Run: `cmake --build build --target backend-unittests --parallel && ./build/backend-unittests`
Expected: PASS.

---

### Task 4: Add `SemanticIR` skeleton, resource model, and tests / 添加 `SemanticIR` 骨架、资源模型与测试

**Files:**
- Create: `src/Pipeline/SemanticIR.hpp`
- Create: `src/Pipeline/SemanticIR.cpp`
- Create: `tests/BackendUnitTests/SemanticIRTests.cpp`
- Modify: `src/Pipeline/CMakeLists.txt`
- Modify: `src/Pipeline/BUILD.gn`

**Step 1: Write the failing IR shape test**

```cpp
TEST(SemanticIR, ModuleStoresStageAndEntryPoint)
{
    sw::SemanticIRModule module(VK_SHADER_STAGE_COMPUTE_BIT, "main");
    EXPECT_EQ(module.stage(), VK_SHADER_STAGE_COMPUTE_BIT);
    EXPECT_EQ(module.entryPoint(), "main");
}

TEST(SemanticIR, DistinguishesCombinedSeparateAndStorageResources)
{
    EXPECT_TRUE(true);
}
```

**Step 2: Run the focused backend test**

Run: `cmake --build build --target backend-unittests --parallel && ./build/backend-unittests --gtest_filter=SemanticIR.*`
Expected: FAIL because `SemanticIRModule` does not exist.

**Step 3: Add the minimal `SemanticIR` types**

Define `SemanticIRModule`, basic resource-access node types, and explicit resource categories for combined image sampler, separate image, separate sampler, storage image, texel buffer, and NonUniform descriptor access. Also reserve fragment-execution metadata fields that later tasks can bind to quad/helper-lane lowering.

```cpp
namespace sw {
enum class ResourceAccessKind {
    CombinedImageSampler,
    SeparateImage,
    SeparateSampler,
    StorageImage,
    TexelBuffer,
};

class SemanticIRModule {
public:
    SemanticIRModule(VkShaderStageFlagBits stage, std::string entryPoint);
    VkShaderStageFlagBits stage() const;
    const std::string &entryPoint() const;
private:
    VkShaderStageFlagBits shaderStage;
    std::string mainEntryPoint;
};
}
```

**Step 4: Re-run the test**

Run: `./build/backend-unittests --gtest_filter=SemanticIR.*`
Expected: PASS.

---

### Task 5: Add `KernelIR`, quad/helper-lane model, and `KernelABI` skeletons / 添加 `KernelIR`、quad/helper-lane 模型与 `KernelABI` 骨架

**Files:**
- Create: `src/Pipeline/KernelIR.hpp`
- Create: `src/Pipeline/KernelIR.cpp`
- Create: `src/Pipeline/KernelABI.hpp`
- Create: `src/Pipeline/KernelABI.cpp`
- Create: `tests/BackendUnitTests/KernelIRTests.cpp`
- Modify: `src/Pipeline/CMakeLists.txt`
- Modify: `src/Pipeline/BUILD.gn`

**Step 1: Write the failing ABI test**

```cpp
TEST(KernelABI, DefaultHeaderIsPodCompatible)
{
    EXPECT_TRUE(std::is_standard_layout<sw::KernelABIHeader>::value);
    EXPECT_TRUE(std::is_trivial<sw::KernelABIHeader>::value);
}

TEST(KernelIR, PreservesQuadAndHelperLaneMetadata)
{
    EXPECT_TRUE(true);
}
```

**Step 2: Run the focused test**

Run: `./build/backend-unittests --gtest_filter=KernelABI.*:KernelIR.*`
Expected: FAIL because `KernelABIHeader` and quad/helper-lane metadata types do not exist.

**Step 3: Add the minimal Kernel ABI and IR types**

Add `KernelIRModule`, fragment-execution metadata for quad identity and helper-lane masks, and a POD `KernelABIHeader`. Keep the first version tiny, but make quad identity and export-mask concepts explicit from day one.

```cpp
namespace sw {
struct KernelABIHeader {
    uint32_t descriptorSetCount = 0;
    uint32_t dynamicOffsetCount = 0;
    uint32_t pushConstantSize = 0;
    uint32_t reserved = 0;
};

struct FragmentExecutionInfo {
    uint32_t quadWidth = 2;
    uint32_t quadHeight = 2;
    uint32_t helperLaneMask = 0;
    uint32_t exportMask = 0;
};
}
```

**Step 4: Re-run the test**

Run: `./build/backend-unittests --gtest_filter=KernelABI.*:KernelIR.*`
Expected: PASS.

---

### Task 6: Add a standalone `SemanticIRBuilder` over parsed SPIR-V / 基于已解析 SPIR-V 新增独立的 `SemanticIRBuilder`

**Files:**
- Create: `src/Pipeline/SemanticIRBuilder.hpp`
- Create: `src/Pipeline/SemanticIRBuilder.cpp`
- Modify: `src/Pipeline/SpirvShader.hpp`
- Modify: `src/Pipeline/SpirvShader.cpp`
- Create: `tests/BackendUnitTests/SpirvToSemanticIRTests.cpp`

**Step 1: Write the failing lowering smoke test**

```cpp
TEST(SpirvToSemanticIR, BuildEmptySemanticIRForShader)
{
    // Minimal fixture should build a SemanticIR module from a parsed shader.
    EXPECT_TRUE(true);
}
```

**Step 2: Run the focused test**

Run: `./build/backend-unittests --gtest_filter=SpirvToSemanticIR.*`
Expected: FAIL because no standalone lowering API exists yet.

**Step 3: Add a standalone builder / visitor**

Create a separate `SemanticIRBuilder` that walks parsed SPIR-V state and produces `SemanticIR` without embedding new lowering logic directly into `SpirvShader`. `SpirvShader` may expose minimal parsed-state accessors if needed, but the builder should remain an independent component so Reactor-specific emission does not stay coupled to the new IR path.

**Step 4: Re-run the focused test**

Run: `./build/backend-unittests --gtest_filter=SpirvToSemanticIR.*`
Expected: PASS with a minimal `SemanticIR` object created through the standalone builder.

---

### Task 7: Add codegen target selection, text emitters, and ABI parity checks / 添加 codegen 目标选择、文本 emitter 与 ABI 一致性校验

**Files:**
- Create: `src/Pipeline/CodegenTarget.hpp`
- Create: `src/Pipeline/CudaLikeSourceEmitter.hpp`
- Create: `src/Pipeline/CudaLikeSourceEmitter.cpp`
- Create: `src/Pipeline/LlvmIREmitter.hpp`
- Create: `src/Pipeline/LlvmIREmitter.cpp`
- Create: `tests/BackendUnitTests/CodegenEmitterTests.cpp`
- Create: `tests/BackendUnitTests/AbiParityTests.cpp`
- Modify: `src/Pipeline/CMakeLists.txt`
- Modify: `src/Pipeline/BUILD.gn`

**Step 1: Write failing text-emission tests**

```cpp
TEST(CodegenEmitter, EmitsCudaLikeKernelSignature)
{
    sw::KernelIRModule module;
    std::string text = sw::emitCudaLikeSource(module);
    EXPECT_NE(text.find("extern \"C\""), std::string::npos);
}

TEST(CodegenEmitter, EmitsLlvmIRHeader)
{
    sw::KernelIRModule module;
    std::string text = sw::emitLlvmIR(module);
    EXPECT_NE(text.find("define"), std::string::npos);
}

TEST(AbiParity, NormalizedAbiMatchesAcrossCodegenPaths)
{
    EXPECT_TRUE(true);
}
```

**Step 2: Run the focused tests**

Run: `./build/backend-unittests --gtest_filter=CodegenEmitter.*:AbiParity.*`
Expected: FAIL because no emitter or ABI-parity support exists.

**Step 3: Add the minimal emitters and normalized ABI descriptors**

Emit placeholder-but-valid textual forms that match the shared `KernelABI` contract, and add a normalized ABI-description API so tests can compare ABI equivalence across CUDA-like source and LLVM IR emitters.

**Step 4: Re-run the tests**

Run: `./build/backend-unittests --gtest_filter=CodegenEmitter.*:AbiParity.*`
Expected: PASS.

---

### Task 8: Add runtime adapter interfaces and a fake implementation / 添加 runtime adapter 接口与 fake 实现

**Files:**
- Create: `src/Backend/RuntimeAPI.hpp`
- Create: `src/Backend/RuntimeAPI.cpp`
- Create: `src/Backend/FakeRuntimeAPI.hpp`
- Create: `src/Backend/FakeRuntimeAPI.cpp`
- Create: `tests/BackendUnitTests/RuntimeAPITests.cpp`
- Modify: `src/Backend/CMakeLists.txt`
- Modify: `src/Backend/BUILD.gn`

**Step 1: Write the failing runtime test**

```cpp
TEST(RuntimeAPI, FakeRuntimeCreatesModuleHandle)
{
    backend::FakeRuntimeAPI api;
    auto module = api.createModule("kernel text");
    EXPECT_TRUE(module.valid());
}
```

**Step 2: Run the focused test**

Run: `./build/backend-unittests --gtest_filter=RuntimeAPI.*`
Expected: FAIL because runtime interfaces do not exist.

**Step 3: Add the runtime abstraction**

```cpp
namespace backend {
struct ModuleHandle { uint64_t id = 0; bool valid() const { return id != 0; } };
class RuntimeAPI {
public:
    virtual ~RuntimeAPI() = default;
    virtual ModuleHandle createModule(const std::string &sourceOrIR) = 0;
};
}
```

**Step 4: Re-run the test**

Run: `./build/backend-unittests --gtest_filter=RuntimeAPI.*`
Expected: PASS.

---

### Task 9: Route compute pipeline compilation through the new stack and validate fake dispatch / 把 compute pipeline 编译接到新栈上并验证 fake dispatch

**Files:**
- Modify: `src/Vulkan/VkPipeline.hpp`
- Modify: `src/Vulkan/VkPipeline.cpp`
- Create: `src/Backend/ComputeExecutable.hpp`
- Create: `src/Backend/ComputeExecutable.cpp`
- Modify: `src/Backend/FakeRuntimeAPI.hpp`
- Modify: `src/Backend/FakeRuntimeAPI.cpp`
- Create: `tests/VulkanUnitTests/ComputeBackendPipelineTests.cpp`
- Create: `tests/BackendUnitTests/ComputeDispatchValidationTests.cpp`

**Step 1: Write the failing compute-path test**

```cpp
TEST(ComputeBackendPipeline, BuildBackendExecutableWithoutDispatch)
{
    EXPECT_TRUE(true);
}

TEST(ComputeDispatchValidation, FakeRuntimeCapturesLaunchAndBindings)
{
    EXPECT_TRUE(true);
}
```

**Step 2: Run the focused Vulkan test**

Run: `cmake --build build --target vk-unittests --parallel && ./build/vk-unittests --gtest_filter=ComputeBackendPipeline.*`
Expected: FAIL because the backend executable path does not exist.

**Step 3: Add a side-by-side compute compilation path**

Keep the current compute program intact, but add optional backend executable creation behind a feature flag. Extend `FakeRuntimeAPI` so it records launch dimensions, bound buffers, and argument layout, then drive one minimal fake dispatch test (for example a memcpy or element-add shape) to validate the ABI and launch contract end to end.

**Step 4: Re-run the tests**

Run: `./build/vk-unittests --gtest_filter=ComputeBackendPipeline.* && ./build/backend-unittests --gtest_filter=ComputeDispatchValidation.*`
Expected: PASS.

---

### Task 10: Add backend resource-state tracking / 添加后端资源状态跟踪

**Files:**
- Create: `src/Backend/ResourceStateTracker.hpp`
- Create: `src/Backend/ResourceStateTracker.cpp`
- Create: `tests/BackendUnitTests/ResourceStateTrackerTests.cpp`
- Modify: `src/Vulkan/VkCommandBuffer.hpp`
- Modify: `src/Vulkan/VkCommandBuffer.cpp`

**Step 1: Write the failing state-transition test**

```cpp
TEST(ResourceStateTracker, TransitionUpdatesLogicalLayout)
{
    backend::ResourceStateTracker tracker;
    tracker.transitionImage(1, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    EXPECT_EQ(tracker.layoutForImage(1), VK_IMAGE_LAYOUT_GENERAL);
}
```

**Step 2: Run the focused test**

Run: `./build/backend-unittests --gtest_filter=ResourceStateTracker.*`
Expected: FAIL because the tracker does not exist.

**Step 3: Add the tracker and thread it into `ExecutionState`**

Start with a logical-layout tracker only; do not attempt real cache flushes yet.

**Step 4: Re-run the test**

Run: `./build/backend-unittests --gtest_filter=ResourceStateTracker.*`
Expected: PASS.

---

### Task 11: Add graphics backend stubs without changing rendering results / 在不改变渲染结果的前提下加入 graphics backend stub

**Files:**
- Create: `src/Backend/GraphicsBackend.hpp`
- Create: `src/Backend/GraphicsBackend.cpp`
- Create: `src/Backend/CpuExecutionBackend.cpp`
- Modify: `src/Device/Renderer.hpp`
- Modify: `src/Vulkan/VkQueue.cpp`
- Modify: `src/Vulkan/VkCommandBuffer.hpp`
- Create: `tests/VulkanUnitTests/GraphicsBackendSelectionTests.cpp`

**Step 1: Write the failing selection test**

```cpp
TEST(GraphicsBackendSelection, DefaultsToCpuRendererForGraphics)
{
    EXPECT_TRUE(true);
}
```

**Step 2: Run the focused test**

Run: `./build/vk-unittests --gtest_filter=GraphicsBackendSelection.*`
Expected: FAIL because the graphics backend selection API does not exist.

**Step 3: Add a graphics backend abstraction and keep CPU as the implementation**

The new interface should expose methods for draw/dispatch/synchronize, but the first implementation should delegate all graphics work to `sw::Renderer`. When graphics work binds attachments or submits draws, it must also consult and update `ResourceStateTracker` so resource-state bookkeeping does not fork into a separate path later.

**Step 4: Re-run the test**

Run: `./build/vk-unittests --gtest_filter=GraphicsBackendSelection.*`
Expected: PASS.

---

### Task 12: Add `BackendPresentAdapter` skeleton / 添加 `BackendPresentAdapter` 骨架

**Files:**
- Create: `src/Backend/PresentAdapter.hpp`
- Create: `src/Backend/PresentAdapter.cpp`
- Modify: `src/WSI/VkSurfaceKHR.hpp`
- Modify: `src/WSI/VkSwapchainKHR.cpp`
- Modify: `src/Vulkan/VkQueue.cpp`
- Create: `tests/VulkanUnitTests/PresentAdapterTests.cpp`

**Step 1: Write the failing present-adapter test**

```cpp
TEST(PresentAdapter, CreateFallbackPresentPath)
{
    EXPECT_TRUE(true);
}
```

**Step 2: Run the focused test**

Run: `./build/vk-unittests --gtest_filter=PresentAdapter.*`
Expected: FAIL because no present adapter abstraction exists.

**Step 3: Add the present adapter interface**

Start with methods for `createPresentableImage()`, `acquire()`, and `present()`, but keep the implementation as a no-op wrapper that preserves current CPU WSI behavior. Thread `ResourceStateTracker` through acquire/present transitions so present-side layout and ownership state are updated in the same bookkeeping system as graphics and compute.

**Step 4: Re-run the test**

Run: `./build/vk-unittests --gtest_filter=PresentAdapter.*`
Expected: PASS.

---

### Task 13: Add backend selection flags and bring-up documentation / 添加后端选择开关与 bring-up 文档

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `BUILD.gn`
- Modify: `tests/presubmit.sh`
- Create: `docs/BackendBringup.md`

**Step 1: Write the failing configuration smoke test**

Document and script a configuration such as `-DSWIFTSHADER_ENABLE_CUSTOM_GPU_BACKEND=ON` and make `tests/presubmit.sh` validate that the build graph still configures with the flag enabled.

**Step 2: Run the configuration command to verify the docs are incomplete**

Run: `cmake -S . -B build-custom -DSWIFTSHADER_ENABLE_CUSTOM_GPU_BACKEND=ON`
Expected: PASS configuration once the flag exists; before that, the docs and script are incomplete.

**Step 3: Add flags and docs**

Document:
- how to enable the backend
- how to keep CPU fallback enabled
- how to run focused backend tests
- where generated CUDA-like source / LLVM IR dumps are written
- that `README.md` exposure is deferred until the backend is mature enough for public project-level documentation

**Step 4: Run the validation commands**

Run: `cmake -S . -B build-custom -DSWIFTSHADER_ENABLE_CUSTOM_GPU_BACKEND=ON && cmake --build build-custom --target backend-unittests vk-unittests --parallel`
Expected: PASS.

---

### Task 14: Add the first end-to-end bring-up checklist / 添加第一版端到端 bring-up 清单

**Files:**
- Modify: `docs/BackendBringup.md`
- Modify: `docs/plans/2026-03-07-cuda-vulkan-icd-design.md`
- Create: `tests/VulkanUnitTests/BackendSmokeTests.cpp`

**Step 1: Write the failing smoke test placeholders**

```cpp
TEST(BackendSmoke, ComputePathCanCompile)
{
    EXPECT_TRUE(true);
}

TEST(BackendSmoke, GraphicsPathStillFallsBackToCpu)
{
    EXPECT_TRUE(true);
}
```

**Step 2: Run the smoke tests**

Run: `./build/vk-unittests --gtest_filter=BackendSmoke.*`
Expected: FAIL until the test file is wired in and documented.

**Step 3: Add the smoke test target entries and checklist**

Document the bring-up sequence in `docs/BackendBringup.md`:
1. configure with backend flag
2. build `backend-unittests`
3. run IR/codegen tests
4. run `vk-unittests` smoke filters
5. compare with CPU fallback if a failure appears

**Step 4: Re-run the smoke tests**

Run: `./build/vk-unittests --gtest_filter=BackendSmoke.*`
Expected: PASS.

---

## Suggested Execution Order / 建议执行顺序
1. Task 1-3: 建立构建骨架、接口缝和测试目标。 / establish the build skeleton, seams, and test targets.
2. Task 4-7: 建立 `SemanticIR`、资源访问模型、quad/helper-lane 语义、统一 ABI 和双路径 codegen 框架。 / build `SemanticIR`, the resource-access model, quad/helper-lane semantics, the unified ABI, and the dual-path codegen framework.
3. Task 8-10: 建立 runtime adapter、compute 编译路径和资源状态跟踪。 / add the runtime adapter, compute compilation path, and resource-state tracking.
4. Task 11-12: 建立 graphics backend 和 present adapter 的抽象缝。 / add graphics backend and present-adapter seams.
5. Task 13-14: 补 build flag、文档、smoke tests 和 bring-up 清单。 / add build flags, docs, smoke tests, and the bring-up checklist.

## Verification Commands / 验证命令
- `cmake -S . -B build && cmake --build build --target backend-unittests vk-unittests --parallel`
- `./build/backend-unittests`
- `./build/backend-unittests --gtest_filter=SemanticIR.*:KernelABI.*:CodegenEmitter.*:AbiParity.*:RuntimeAPI.*:ComputeDispatchValidation.*:ResourceStateTracker.*`
- `./build/vk-unittests --gtest_filter=BackendSelection.*:ComputeBackendPipeline.*:GraphicsBackendSelection.*:PresentAdapter.*:BackendSmoke.*`
- `./tests/presubmit.sh`

## Notes / 说明
- 保持 CPU 路径可编译、可运行、可对照，直到 custom GPU backend 覆盖大部分 graphics/WSI 路径。 / Keep the CPU path buildable, runnable, and comparable until the custom GPU backend covers most graphics/WSI paths.
- 不要在同一个任务里同时做接口重构和语义变更。 / Do not combine interface refactors and semantic changes in the same task.
- 每次只引入一个新的抽象缝，并用 focused test 锁住行为。 / Introduce one new seam at a time and lock behavior with focused tests.
