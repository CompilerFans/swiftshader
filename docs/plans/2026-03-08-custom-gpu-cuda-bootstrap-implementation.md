# Custom GPU CUDA Bootstrap Implementation Plan

> **For Claude:** Execute this plan incrementally with TDD and frequent commits.

**Goal:** Replace the fake runtime in the custom backend with a real `nvcc` + CUDA Driver API bootstrap path, and keep the solid-color triangle test as the first graphics smoke test.

**Architecture:** Extend `RuntimeAPI` toward real module/memory/launch semantics, add `CudaCompilerDriver` and `CudaRuntimeAPI`, then route custom compute and custom queue-submit bootstrap through them while keeping CPU rendering as the fallback. The CUDA path is enabled only when an explicit build flag is set.

**Tech Stack:** C++17, Vulkan frontend, GoogleTest, `nvcc`, CUDA Driver API, dynamic library loading through `System/SharedLibrary.hpp`.

---

### Task 1: Add failing real-CUDA runtime tests
- Modify: `tests/BackendUnitTests/RuntimeAPITests.cpp`
- Modify: `tests/VulkanUnitTests/ComputeBackendPipelineTests.cpp`
- Modify: `tests/VulkanUnitTests/DrawTests.cpp`
- Step 1: Add a backend test that requires a real CUDA compile + launch + readback path.
- Step 2: Add Vulkan-side assertions that the custom CUDA build uses the real runtime in compute and draw bootstrap paths.
- Step 3: Run the focused tests and verify they fail.

### Task 2: Implement compiler driver and runtime
- Create: `src/Backend/CudaCompilerDriver.hpp`
- Create: `src/Backend/CudaCompilerDriver.cpp`
- Create: `src/Backend/CudaRuntimeAPI.hpp`
- Create: `src/Backend/CudaRuntimeAPI.cpp`
- Modify: `src/Backend/RuntimeAPI.hpp`
- Modify: `src/Backend/FakeRuntimeAPI.hpp`
- Modify: `src/Backend/FakeRuntimeAPI.cpp`
- Step 1: Extend `RuntimeAPI` with real module / memory / launch primitives.
- Step 2: Implement the `nvcc --fatbin` compile path and dynamic Driver API binding.
- Step 3: Run the focused backend test and make it pass.

### Task 3: Route custom backend through real CUDA bootstrap
- Modify: `src/Backend/BackendFactory.cpp`
- Modify: `src/Backend/CustomExecutionBackend.cpp`
- Modify: `src/Backend/ComputeExecutable.cpp`
- Step 1: Use the real runtime when the CUDA build flag is enabled.
- Step 2: Ensure queue submit performs a real CUDA launch before CPU fallback.
- Step 3: Verify the Vulkan compute and draw tests now pass in the CUDA build.

### Task 4: Wire build flags and record verification
- Modify: `CMakeLists.txt`
- Modify: `src/Backend/CMakeLists.txt`
- Modify: `src/Backend/BUILD.gn`
- Modify: `src/Backend/BackendConfig.hpp`
- Modify: `task_plan.md`
- Modify: `progress.md`
- Step 1: Add a dedicated build flag for real CUDA backend bootstrap.
- Step 2: Build with incremental directories and `ccache`.
- Step 3: Record the exact verification commands and results.
