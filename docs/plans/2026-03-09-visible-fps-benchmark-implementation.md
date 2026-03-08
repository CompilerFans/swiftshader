# Visible FPS Benchmark Implementation

## Scope

This implementation adds a visible benchmark path for a rotating interpolated-color triangle whose colors also change over time.

## Files

- `tests/VulkanWrapper/VulkanHeaders.hpp`
- `tests/VulkanWrapper/Window.hpp`
- `tests/VulkanWrapper/Window.cpp`
- `tests/VulkanWrapper/VulkanTester.cpp`
- `tests/VulkanWrapper/DrawTester.hpp`
- `tests/VulkanWrapper/DrawTester.cpp`
- `tests/VulkanWrapper/CMakeLists.txt`
- `tests/VulkanBenchmarks/CMakeLists.txt`
- `tests/VulkanBenchmarks/AnimatedTriangleBenchmark.cpp`
- `tests/VulkanBenchmarks/run-animated-triangle-benchmark.sh`
- `tests/VulkanUnitTests/DrawTests.cpp`

## Key Changes

1. Added `DrawTester::updateVertexBufferData()` so a benchmark can animate geometry without re-recording command buffers.
2. Split a `VulkanWrapperVisible` target from the existing wrapper, using `SWIFTSHADER_VULKAN_WRAPPER_FORCE_NATIVE_WINDOW=1` to bypass the default Linux headless path.
3. Implemented Linux/XCB window creation, event pumping, and title updates in `Window`.
4. Enabled `VK_KHR_XCB_SURFACE_EXTENSION_NAME` in `VulkanTester` when the visible wrapper is built.
5. Added `animated-triangle-benchmark`, which:
   - updates a position+color triangle every frame
   - rotates it over time
   - changes colors over time
   - shows FPS in stdout and the window title
6. Added `run-animated-triangle-benchmark.sh`, which selects CPU or CUDA by choosing the appropriate build directory and running the benchmark from inside that directory.

## Verification

- `DrawTest.DynamicVertexBufferUpdateChangesRenderedColor`
- `timeout 8s tests/VulkanBenchmarks/run-animated-triangle-benchmark.sh --backend=cpu --seconds=2`
- `timeout 8s tests/VulkanBenchmarks/run-animated-triangle-benchmark.sh --backend=cuda --seconds=2`

