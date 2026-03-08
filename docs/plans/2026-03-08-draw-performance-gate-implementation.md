# Draw Performance Gate Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 为当前 CPU 路径建立简单但真实的 draw 性能基线，同时提供窗口可视 FPS 观察模式，为后续 GPU draw 路径复用同一套 benchmark 场景做准备。 / Establish a simple but real draw-performance baseline for the current CPU path, while also providing a window-visible FPS mode so the same benchmark scenes can later be reused by the GPU draw path.

**Architecture:** 复用 `tests/VulkanBenchmarks/TriangleBenchmarks.cpp` 作为自动 benchmark 主入口，补充一个批量纯色三角形场景，并在 `DrawTester` 基础上增加可复用的 windowed FPS 运行模式。自动 benchmark 用于输出稳定的 `avg_ms` / `median_ms` / `fps`，窗口模式用于人工感官观察，两者共用相同的简单绘制场景。 / Reuse `tests/VulkanBenchmarks/TriangleBenchmarks.cpp` as the main automated benchmark entry, add a batched solid-triangle scene, and build a reusable windowed FPS mode on top of `DrawTester`. The automated benchmark reports stable `avg_ms` / `median_ms` / `fps`, while the windowed mode provides visual observation; both use the same simple draw scenes.

**Tech Stack:** C++17, VulkanWrapper, Google Benchmark, DrawTester, Window, CPU draw path, optional future GPU backend reuse. / C++17, VulkanWrapper, Google Benchmark, DrawTester, Window, the CPU draw path, and later reuse by the GPU backend.

---

### Task 1: Add CPU ManySolidTriangles benchmark / 添加 CPU 批量纯色三角形 benchmark

**Files:**
- Modify: `tests/VulkanBenchmarks/TriangleBenchmarks.cpp`

**Step 1: Write the failing benchmark-oriented test hook**

Add a new benchmark case declaration for `ManySolidTriangles`, with configurable triangle counts such as `1K`, `16K`, and `64K`. Initially leave it unimplemented so the file fails to compile because the new function is referenced but missing.

**Step 2: Run build to verify it fails**

Run: `cmake --build build-cuda-bootstrap --target vk-benchmarks --parallel $(nproc)`  
Expected: FAIL because `ManySolidTriangles` is declared in the benchmark registration but not yet defined.

**Step 3: Write the minimal implementation**

Implement `ManySolidTriangles` by:
- generating a packed `vec3 position` vertex buffer for many CPU-side triangles,
- reusing the same fixed-color VS/FS structure as `TriangleSolidColor`,
- recording one or a small fixed number of draw calls,
- measuring steady-state frame rendering.

**Step 4: Run build and benchmark**

Run:
- `cmake --build build-cuda-bootstrap --target vk-benchmarks --parallel $(nproc)`
- `cd build-cuda-bootstrap && ./vk-benchmarks --benchmark_filter=TriangleSolidColor|ManySolidTriangles`

Expected: PASS, with benchmark output showing timing rows for both simple and batched triangle cases.

**Step 5: Commit**

```bash
git add tests/VulkanBenchmarks/TriangleBenchmarks.cpp
git commit -m "tests: add draw performance baseline benchmark"
```

### Task 2: Normalize draw benchmark output / 统一 draw benchmark 输出格式

**Files:**
- Modify: `tests/VulkanBenchmarks/TriangleBenchmarks.cpp`

**Step 1: Write the failing output-format check**

Add a small benchmark metadata path so the new cases must report:
- case name
- triangle count
- backend tag
- mode tag

Make the initial output-format assertion fail by checking for a missing label.

**Step 2: Run benchmark to verify the output is incomplete**

Run: `cd build-cuda-bootstrap && ./vk-benchmarks --benchmark_filter=ManySolidTriangles --benchmark_min_time=0.1`  
Expected: FAIL or missing metadata in the printed output.

**Step 3: Write the minimal implementation**

Use Google Benchmark counters or labels so the result always includes:
- `triangle_count`
- `backend=cpu`
- `mode=headless`

**Step 4: Re-run benchmark**

Run: `cd build-cuda-bootstrap && ./vk-benchmarks --benchmark_filter=TriangleSolidColor|ManySolidTriangles --benchmark_min_time=0.1`  
Expected: PASS with clearly attributable output rows.

**Step 5: Commit**

```bash
git add tests/VulkanBenchmarks/TriangleBenchmarks.cpp
git commit -m "tests: label draw performance benchmark output"
```

### Task 3: Add windowed FPS observation mode / 添加窗口 FPS 观察模式

**Files:**
- Modify: `tests/VulkanWrapper/DrawTester.hpp`
- Modify: `tests/VulkanWrapper/DrawTester.cpp`
- Create: `tests/VulkanBenchmarks/DrawFpsObserver.cpp`
- Modify: `tests/VulkanBenchmarks/CMakeLists.txt`

**Step 1: Write the failing runtime entry**

Add a new small executable target for a windowed FPS observer and make the build fail until the source file and helper APIs exist.

**Step 2: Run build to verify it fails**

Run: `cmake --build build-cuda-bootstrap --target draw-fps-observer --parallel $(nproc)`  
Expected: FAIL because the new target or source file is missing.

**Step 3: Write the minimal implementation**

Implement a small windowed tool that:
- reuses the `SolidColorTriangle` draw scene,
- calls `show()`,
- renders continuously for a fixed duration or until closed,
- prints once per second:
  - current FPS
  - average FPS
  - average frame time in ms

**Step 4: Run the observer manually**

Run: `cd build-cuda-bootstrap && ./draw-fps-observer --case=solid --seconds=10`  
Expected: a visible window plus periodic FPS lines in the terminal.

**Step 5: Commit**

```bash
git add tests/VulkanWrapper/DrawTester.hpp tests/VulkanWrapper/DrawTester.cpp tests/VulkanBenchmarks/DrawFpsObserver.cpp tests/VulkanBenchmarks/CMakeLists.txt
git commit -m "tests: add windowed draw fps observer"
```

### Task 4: Record the CPU baseline in project docs / 在文档中记录 CPU baseline

**Files:**
- Modify: `progress.md`
- Modify: `task_plan.md`
- Modify: `findings.md`
- Modify: `docs/plans/2026-03-07-custom-gpu-vulkan-icd-implementation.md`

**Step 1: Write the failing documentation checklist**

Create a checklist entry requiring:
- automated benchmark command,
- windowed FPS command,
- current CPU-only scope,
- future GPU comparison rule.

**Step 2: Verify the checklist is incomplete**

Inspect the current files and confirm the new performance baseline is not yet documented.

**Step 3: Write the minimal documentation**

Document:
- the exact benchmark commands,
- the window observer command,
- the current CPU-only limitation,
- the rule that future GPU draw must reuse the same scenes.

**Step 4: Verify docs**

Run:
- `rg -n "ManySolidTriangles|draw-fps-observer|CPU baseline|GPU comparison" progress.md task_plan.md findings.md docs/plans/2026-03-07-custom-gpu-vulkan-icd-implementation.md`

Expected: all new entries are present.

**Step 5: Commit**

```bash
git add progress.md task_plan.md findings.md docs/plans/2026-03-07-custom-gpu-vulkan-icd-implementation.md
git commit -m "docs: record draw performance baseline"
```
