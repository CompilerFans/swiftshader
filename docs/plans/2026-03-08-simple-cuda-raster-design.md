# Simple CUDA Raster Design

**Goal:** 在不引入完整 tile/binning 和不复用外部 raster 项目的前提下，为当前 CUDA backend 落地一个可演进的最小 raster 路径，并用 CPU 参考测试与 GPU 专有测试逐步对齐功能和精度。 / Land an evolvable minimal raster path for the current CUDA backend without pulling in full tile/binning or external raster projects, and align behavior and precision incrementally through CPU-reference tests plus GPU-specific tests.

## Decision

- **主路径：自研简单 CUDA raster。** 第一阶段只覆盖 `triangle list`、单 render target、`bbox + edge function` 覆盖测试、无 MSAA、无 depth/stencil、无 blend。 / **Primary path: build a simple in-house CUDA raster.** Phase 1 covers only `triangle list`, one render target, `bbox + edge function` coverage testing, no MSAA, no depth/stencil, and no blending.
- **CPU 现有实现只作为参考，不直接移植。** `src/Device/SetupProcessor.cpp`、`src/Device/QuadRasterizer.cpp`、`src/Device/PixelProcessor.cpp` 的算法和行为可以作为参考 oracle，但实现本身不适合直接搬到 CUDA backend。 / **Use the existing CPU implementation as reference only, not as code to transplant.** The algorithms and behavior in `src/Device/SetupProcessor.cpp`, `src/Device/QuadRasterizer.cpp`, and `src/Device/PixelProcessor.cpp` act as an oracle, but the implementation itself is not a good direct fit for the CUDA backend.
- **不采用 `nvdiffrast` 一类开源 raster 作为主实现。** 这类项目的目标、资源模型和驱动集成边界与 Vulkan ICD bring-up 不对齐，更适合作为研究参考而不是主代码路径。 / **Do not use projects such as `nvdiffrast` as the primary raster implementation.** Their goals, resource model, and driver-integration boundary do not align with Vulkan ICD bring-up; they are better as research references than as the main code path.

## Bring-up Strategy

- **阶段 1：独立 raster bootstrap。** 输入为三个 post-VS 顶点，输出为 `FragmentBootstrapInvocation` 列表；先不接入真实 draw。 / **Phase 1: standalone raster bootstrap.** Input is three post-VS vertices and output is a list of `FragmentBootstrapInvocation` items; do not connect it to real draw yet.
- **阶段 2：CPU 参考测试。** 用轻量 CPU 参考实现或现有 SwiftShader CPU 路径的等价规则，验证 bbox、coverage、屏幕坐标和 winding 下的最小一致性。 / **Phase 2: CPU reference tests.** Use a lightweight CPU reference implementation or equivalent rules from the existing SwiftShader CPU path to validate bbox, coverage, screen coordinates, and winding behavior.
- **阶段 3：GPU 专有测试。** 在 `backend-unittests` 中直接验证 CUDA raster 输出的 invocation 数量、像素坐标和边界点；必要时允许先留 `stub` / `dummy` 输出接口。 / **Phase 3: GPU-specific tests.** Validate CUDA raster output invocation count, pixel coordinates, and boundary points directly in `backend-unittests`; allow `stub` / `dummy` output interfaces early where needed.
- **阶段 4：接入 fragment bootstrap。** 当独立 raster 结果稳定后，再把其输出交给现有 `FragmentBootstrap`，形成 `VS -> Raster -> FS` 的最小 GPU 图形链。 / **Phase 4: connect to the fragment bootstrap.** Once standalone raster output is stable, feed it into the existing `FragmentBootstrap` to form a minimal `VS -> Raster -> FS` GPU graphics chain.

## Testing Policy

- **双轨验证：**
  - **CPU 参考测试 / CPU reference tests:** 用同一组三角形输入，生成期望的 bbox 和像素覆盖集合，作为功能与精度对齐基线。 / Use the same triangle inputs to generate expected bbox and pixel coverage sets as the baseline for functionality and precision alignment.
  - **GPU 执行测试 / GPU execution tests:** 通过 `CudaRuntimeAPI` 跑真实 CUDA kernel，检查输出 invocation 是否与参考结果匹配。 / Run real CUDA kernels through `CudaRuntimeAPI` and compare their invocation outputs against the reference results.
- **循序渐进容许 stub。** 初期允许 `RasterBootstrapConfig`、`RasterBootstrapOutput` 等接口先保留未实现字段，但每次前进一步都要有失败测试和对齐验证。 / **Progressive stubs are allowed.** Early interfaces such as `RasterBootstrapConfig` and `RasterBootstrapOutput` may keep unimplemented fields at first, but every step forward must come with a failing test and an alignment check.

## Exit Criteria

- `backend-unittests` 拥有独立的 raster CPU 参考测试与 CUDA 执行测试。 / `backend-unittests` contains standalone raster CPU reference tests and CUDA execution tests.
- 对单三角形 case，GPU raster 输出在功能和基础精度上与 CPU 参考一致。 / For the single-triangle case, GPU raster output matches the CPU reference in functionality and basic precision.
- 已具备稳定的 `RasterBootstrap -> FragmentBootstrap` 对接接口，为后续接入真实 draw 做准备。 / A stable `RasterBootstrap -> FragmentBootstrap` interface exists, ready for later real draw integration.
