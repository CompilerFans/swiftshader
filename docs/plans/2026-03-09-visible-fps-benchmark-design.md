# Visible FPS Benchmark Design

## Goal

Insert a scriptable benchmark task that opens a visible window, renders a rotating interpolated-color triangle whose colors also change over time, and shows FPS in both the window and stdout.

## Chosen Approach

Use a dedicated `animated-triangle-benchmark` executable plus a wrapper script that selects either the CPU build or the CUDA-enabled build.

This is the shortest correct path because backend selection is still compile-time in the current repository. A single runtime flag cannot switch between CPU and CUDA honestly today.

## Architecture

1. Extend `tests/VulkanWrapper` so a benchmark target can use a native visible window on Linux/XCB instead of the current forced headless path.
2. Extend `DrawTester` with:
   - dynamic vertex-buffer updates
   - window event pumping
   - window title updates
3. Add `animated-triangle-benchmark` under `tests/VulkanBenchmarks/`.
4. Add `tests/VulkanBenchmarks/run-animated-triangle-benchmark.sh` to:
   - configure/build a CPU benchmark binary
   - configure/build a CUDA benchmark binary
   - run the selected backend with a uniform CLI

## Rendering Model

- Geometry: one triangle
- Vertex payload: position + RGB color
- Animation:
  - triangle rotates over time
  - vertex colors shift over time
- Shader model:
  - VS: position passthrough
  - FS: interpolated varying color

The benchmark updates the mapped vertex buffer every frame instead of adding push constants or a uniform path. That keeps the implementation narrow and sufficient for a first visible benchmark.

## Verification

- Red/green test for dynamic vertex-buffer updates in `draw-unittests`
- Build and run `animated-triangle-benchmark`
- Confirm:
  - visible window opens on this Linux/XCB environment
  - FPS prints to stdout
  - window title updates with backend/FPS

