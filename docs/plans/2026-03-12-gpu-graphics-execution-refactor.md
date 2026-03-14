# GPU Graphics Execution Refactor Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 把 Vulkan draw 的执行入口改为 backend-owned dispatch，并把 triangle bootstrap 从 `Renderer::draw()` 中抽离出去。

**Architecture:** `CmdDrawBase` 先完成 Vulkan 侧输入绑定，再把 draw 参数交给 `ExecutionBackend`。CPU backend 负责 CPU renderer draw；GPU backend 负责 GPU bootstrap / fallback / strict abort 决策。`Renderer::draw()` 收敛为 CPU-only。

**Tech Stack:** C++17, SwiftShader Vulkan runtime, GoogleTest, existing CUDA runtime/bootstrap path

---

### Task 1: Add draw routing model

**Files:**
- Create: `src/Backend/GraphicsDraw.hpp`
- Create: `src/Backend/GraphicsDraw.cpp`
- Create: `tests/BackendUnitTests/GraphicsDrawRoutingTests.cpp`
- Modify: `src/Backend/CMakeLists.txt`
- Modify: `src/Backend/BUILD.gn`
- Modify: `tests/BackendUnitTests/CMakeLists.txt`

**Step 1: Write the failing test**

```cpp
TEST(GraphicsDrawRouting, HardwareRuntimeRequiresGpuPathWhenCpuFallbackDisabled)
{
	EXPECT_EQ(backend::GraphicsDrawRoute::GpuBootstrapRequired,
	          backend::chooseGraphicsDrawRoute(true, true, false, false));
}
```

**Step 2: Run test to verify it fails**

Run: `cmake --build build-cuda-bootstrap --target backend-unittests --parallel 1`
Expected: FAIL because `GraphicsDraw.hpp` / `chooseGraphicsDrawRoute()` do not exist

**Step 3: Write minimal implementation**

```cpp
enum class GraphicsDrawRoute { CpuRenderer, GpuBootstrapOptional, GpuBootstrapRequired };
GraphicsDrawRoute chooseGraphicsDrawRoute(bool hasRuntime, bool hardwareBacked,
                                          bool allowCpuFallback, bool rasterizerDiscard);
```

**Step 4: Run test to verify it passes**

Run: `./build-cuda-bootstrap/backend-unittests --gtest_filter=GraphicsDrawRouting.*`
Expected: PASS

**Step 5: Commit**

```bash
git add src/Backend/GraphicsDraw.* tests/BackendUnitTests/GraphicsDrawRoutingTests.cpp
git commit -m "Backend: add graphics draw routing model"
```

### Task 2: Add backend draw seam

**Files:**
- Modify: `src/Backend/ExecutionBackend.hpp`
- Modify: `src/Backend/CpuExecutionBackend.cpp`
- Modify: `src/Backend/GpuExecutionBackend.cpp`
- Modify: `src/Vulkan/VkCommandBuffer.cpp`

**Step 1: Write the failing test**

```cpp
TEST(GraphicsDrawRouting, CpuRouteSelectedWhenNoRuntime)
{
	EXPECT_EQ(backend::GraphicsDrawRoute::CpuRenderer,
	          backend::chooseGraphicsDrawRoute(false, false, true, false));
}
```

**Step 2: Run test to verify it fails correctly**

Run: `./build-cuda-bootstrap/backend-unittests --gtest_filter=GraphicsDrawRouting.*`
Expected: FAIL until route helper covers this case

**Step 3: Write minimal implementation**

```cpp
class ExecutionBackend {
public:
	virtual void draw(const backend::GraphicsDrawCall &draw) = 0;
};
```

Then make `CmdDrawBase` call `executionBackend->draw(...)` instead of `renderer->draw(...)`.

**Step 4: Run focused build/tests**

Run: `cmake --build build-cuda-bootstrap --target backend-unittests draw-unittests --parallel 1`
Expected: build succeeds

**Step 5: Commit**

```bash
git add src/Backend/ExecutionBackend.hpp src/Backend/CpuExecutionBackend.cpp src/Backend/GpuExecutionBackend.cpp src/Vulkan/VkCommandBuffer.cpp
git commit -m "Vulkan: route draw through execution backend"
```

### Task 3: Extract triangle bootstrap helper from renderer

**Files:**
- Create: `src/Backend/TriangleBootstrapDraw.hpp`
- Create: `src/Backend/TriangleBootstrapDraw.cpp`
- Modify: `src/Backend/CMakeLists.txt`
- Modify: `src/Backend/BUILD.gn`
- Modify: `src/Device/Renderer.cpp`
- Modify: `src/Backend/GpuExecutionBackend.cpp`

**Step 1: Write the failing test**

```cpp
TEST(GraphicsDrawRouting, RasterizerDiscardStaysOnCpu)
{
	EXPECT_EQ(backend::GraphicsDrawRoute::CpuRenderer,
	          backend::chooseGraphicsDrawRoute(true, true, false, true));
}
```

**Step 2: Run test to verify it fails correctly**

Run: `./build-cuda-bootstrap/backend-unittests --gtest_filter=GraphicsDrawRouting.*`
Expected: FAIL until discard case is modeled

**Step 3: Write minimal implementation**

Move GPU-only helpers out of `Renderer.cpp`:
- env-gated route selection
- fragment bootstrap config building
- color attachment writeback
- actual `runTrianglePipelineBootstrap(...)` invocation

Expose one backend helper:

```cpp
bool tryTriangleBootstrapDraw(vk::Device *device,
                              backend::RuntimeAPI &runtime,
                              const backend::GraphicsDrawCall &draw);
```

**Step 4: Run focused tests**

Run:
- `./build-cuda-bootstrap/backend-unittests --gtest_filter=GraphicsDrawRouting.*:TrianglePipelineBootstrap.*`
- `./build-cuda-bootstrap/draw-unittests --gtest_filter=DrawTest.SolidColorTriangle`

Expected: PASS

**Step 5: Commit**

```bash
git add src/Backend/TriangleBootstrapDraw.* src/Device/Renderer.cpp src/Backend/GpuExecutionBackend.cpp
git commit -m "Backend: extract triangle bootstrap draw helper"
```

### Task 4: Verify architecture cleanup

**Files:**
- Modify: `docs/BackendBringup.md`
- Modify: `progress.md`
- Modify: `findings.md`

**Step 1: Verify code shape**

Run:
- `rg -n "runTrianglePipelineBootstrap|GPU_ALLOW_CPU_FALLBACK|CPU DrawCall::run" src/Device/Renderer.cpp src/Backend`
- `rg -n "renderer->draw" src/Vulkan/VkCommandBuffer.cpp src/Backend`

Expected:
- `Renderer.cpp` no longer owns triangle bootstrap routing
- `CmdDrawBase` no longer calls `renderer->draw()` directly

**Step 2: Run validation**

Run:
- `./build-cuda-bootstrap/backend-unittests`
- `./build-cuda-bootstrap/draw-unittests --gtest_filter=DrawTest.SolidColorTriangle`
- `./build-cuda-bootstrap/vk-unittests --gtest_filter='GraphicsBackendSelection.*:BackendSmoke.*'`

Expected: PASS

**Step 3: Update docs/logs**

Record:
- what moved from `Renderer` to backend
- what still remains CPU-only
- next-stage items (`GraphicsExecutable`, transfer/present, async submit)

**Step 4: Commit**

```bash
git add docs/BackendBringup.md progress.md findings.md
git commit -m "Docs: record graphics execution seam refactor"
```
