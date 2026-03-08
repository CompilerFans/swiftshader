# CUDA Source Dump Design

**Goal / 目标**

Add a configurable debug feature that prints the current CUDA kernel source to the screen when the runtime compiles a module, and keep it enabled by default during the current bootstrap phase.  
增加一个可配置的调试功能：在 runtime 编译模块时把当前 CUDA kernel source 打印到屏幕上，并在当前 bootstrap 阶段默认开启。

**Approaches / 方案**

- **Always print unconditionally**  
  Simplest, but not configurable.
- **Runtime-configurable env var with default-on** **(chosen)**  
  Print in `CudaRuntimeAPI::createModule()`, default to enabled, and allow disabling through an environment variable.
- **Write only to a file**  
  Useful for archival, but it does not satisfy the request to print on screen.

**Design / 设计**

- Add one helper in `CudaRuntimeAPI.cpp` to decide whether source dumping is enabled.
- Use an environment variable `SWIFTSHADER_CUDA_DUMP_SOURCE`:
  - unset / empty → enabled
  - `0`, `false`, `off`, `no` → disabled
  - any other value → enabled
- Print to `stderr` inside `createModule()` before calling the compiler, wrapped with visible begin/end markers.
- Keep the feature local to the CUDA runtime path so CPU and fake-runtime paths remain unchanged.

**Validation / 验证**

- Default behavior should print the kernel source.
- Setting `SWIFTSHADER_CUDA_DUMP_SOURCE=0` should suppress the output.
- Existing CUDA runtime tests must continue to pass.
