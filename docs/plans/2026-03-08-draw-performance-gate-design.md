# Draw Performance Gate Design

## Goal / 目标
在当前阶段先建立一套简单但真实的绘制性能验证路径，先覆盖 CPU 通路，输出自动 benchmark 数据与窗口显示 FPS；后续当 GPU 通路真正接管最终像素输出后，复用完全相同的场景做 CPU/GPU 对比，并据此判断是否需要暂停扩展特性、优先分析设计或实现性能问题。  
Establish a simple but real draw-performance validation path for the CPU route first, with both automated benchmark numbers and window-visible FPS. Once the GPU route truly owns final pixel generation, reuse the exact same scenes for CPU/GPU comparison and use the results to decide whether to continue feature expansion or instead stop and analyze design or implementation performance issues.

## Scope / 范围
当前阶段只做 CPU baseline，不把性能结果作为 pass/fail。性能 gate 的意义是先建立稳定的场景、采样方法和输出格式，而不是现在就对 GPU 方案下结论。SPIR-V 完整编译能力不作为这一步前置条件，当前可继续使用 GLSL、SPIR-V text、模板化 CUDA source 或简单 lowering；后续可再评估 `tvm`、`spvgen` 等项目。  
At this stage, only establish CPU baselines and do not treat performance results as pass/fail. The purpose of this gate is to lock down stable scenes, measurement methodology, and output format, not to conclude anything about the GPU path yet. Full SPIR-V compilation support is not a prerequisite; GLSL, SPIR-V text, templated CUDA source, or simple lowering remain acceptable for now, with `tvm`, `spvgen`, and similar projects deferred for later evaluation.

## Approaches / 方案比较
### Option 1: Reuse draw unit tests for timing / 复用 draw 单测计时
优点是接入快；缺点是 `draw-unittests` 更偏正确性验证，容易受 loader、文件导出和断言逻辑干扰，不适合作为持续性能基线。  
Fast to wire up, but `draw-unittests` are correctness-oriented and easily distorted by loader overhead, artifact export, and assertion logic, making them a poor long-term performance baseline.

### Option 2: Standalone window tool only / 只做独立窗口工具
优点是直观；缺点是缺乏稳定的自动统计，无法沉淀成可复用的 CPU/GPU A/B benchmark。  
Intuitive, but it lacks stable automated statistics and does not naturally become a reusable CPU/GPU A/B benchmark.

### Option 3: Dual-track validation / 双轨验证（推荐）
复用现有 `tests/VulkanBenchmarks/TriangleBenchmarks.cpp` 的 benchmark 资产做自动化性能统计，再基于同样的场景加一个 windowed FPS 观察模式。这样既保留数据化输出，也保留开发期直观观察。  
Reuse the existing `tests/VulkanBenchmarks/TriangleBenchmarks.cpp` assets for automated performance statistics, then add a windowed FPS observation mode using the same scenes. This preserves both quantitative output and direct visual feedback during development.

## Recommended Design / 推荐设计
### Automated benchmark path / 自动 benchmark 路径
建立两个 CPU baseline 用例：
- `SolidColorTriangle`：单三角形、`vec3 position`、固定纯色 fragment。它只承担最小 draw 闭环和 baseline sanity，不作为唯一性能结论。  
  A single triangle with `vec3 position` and a fixed-color fragment output. It serves as the minimal draw-loop sanity case, not the sole performance conclusion.
- `ManySolidTriangles`：同一 pipeline、同一 vertex layout、同一纯色 fragment，批量绘制大量三角形，建议至少提供 `1K`、`16K`、`64K` 三档。  
  The same pipeline, vertex layout, and fixed-color fragment shader, but with many triangles. Recommended scales are at least `1K`, `16K`, and `64K`.

自动 benchmark 输出统一包含：
- `case_name`
- `triangle_count`
- `warmup_frames`
- `measure_frames`
- `avg_ms`
- `median_ms`
- `fps`
- `backend=cpu|gpu`
- `mode=headless|windowed`

### Window-visible FPS path / 窗口可视 FPS 路径
提供一个 windowed 模式，渲染和 benchmark 场景相同，但允许实际显示窗口，并每秒打印一次：
- 当前 1 秒 FPS
- 全局平均 FPS
- 最近 1 秒平均帧时长

窗口模式用于人工观察，不作为硬门槛，也不替代自动 benchmark。

## Stage Gate / 阶段门槛
在现有 VS gate 之外，新增一个独立的性能观察 gate：
- 当前阶段只要求 CPU baseline 可稳定运行并打印结果；
- 当前阶段不要求 GPU 优于 CPU；
- 当 GPU 路径真正接管最终像素输出后，必须在完全相同的场景上复用同一套 benchmark。

届时如果 GPU 在 `ManySolidTriangles` 这类受限特性场景下没有明显优势，或反而更慢，则暂停继续扩大特性面，优先分析：
- benchmark 场景是否合理；
- codegen / compile / launch 开销是否主导；
- runtime / memory transfer / synchronization 是否吞噬预期收益；
- 当前实现是否仍处于明显未优化状态。

## Integration Points / 集成点
- 自动 benchmark 优先扩展 `tests/VulkanBenchmarks/TriangleBenchmarks.cpp`
- 窗口显示 FPS 复用 `tests/VulkanWrapper/DrawTester.*` 与 `tests/VulkanWrapper/Window.hpp`
- 主实施计划需要新增一项性能验证任务，并在 VS gate 后、进入 raster/fragment 前保留这一观察基线

## Confirmed Conclusion / 已确认结论
当前先建立 CPU draw baseline 和 windowed FPS 观察路径，不急于给 GPU 方案下性能结论；当 GPU draw 真正接管最终像素后，必须在相同 benchmark 场景上做 CPU/GPU 对比。如果 GPU 没有明显优势或更慢，则优先分析设计与实现，而不是继续盲目扩展功能。  
First establish a CPU draw baseline and a window-visible FPS observation path, without drawing premature conclusions about the GPU path. Once the GPU draw path truly owns final pixel generation, it must be compared against the CPU path on the same benchmark scenes. If the GPU path shows no clear advantage or is slower, prioritize design and implementation analysis instead of continuing blind feature expansion.
