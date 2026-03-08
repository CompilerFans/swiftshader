# Custom GPU CUDA Bootstrap Design

**Goal / 目标**

Use the existing custom-backend scaffolding to execute a real `nvcc`-compatible compile and CUDA Driver API launch path, while keeping the current CPU graphics path as the correctness fallback. The first validation target remains the lightweight solid-color triangle test.  
在现有 custom backend 骨架上接入真实的 `nvcc` 兼容编译与 CUDA Driver API 执行通路，同时保留当前 CPU 图形路径作为正确性回退。第一阶段验证目标仍然是轻量级纯色三角形测试。

**Approach / 方案**

- Add a `CudaCompilerDriver` that writes generated CUDA-like source to a temporary `.cu` file and invokes `nvcc --fatbin` for the active GPU architecture.  
  新增 `CudaCompilerDriver`，将生成的 CUDA-like 源码写入临时 `.cu` 文件，并针对当前 GPU 架构调用 `nvcc --fatbin`。
- Add a `CudaRuntimeAPI` that dynamically loads `libcuda.so`, creates a primary context, loads compiled modules, allocates device memory, performs H2D/D2H copies, and launches `kernel_main`.  
  新增 `CudaRuntimeAPI`，动态加载 `libcuda.so`，创建 primary context，加载编译模块，完成显存分配、H2D/D2H 拷贝，并执行 `kernel_main`。
- Keep `FakeRuntimeAPI` as the non-CUDA fallback; custom-backend builds opt into the real path with an explicit build flag.  
  保留 `FakeRuntimeAPI` 作为非 CUDA 回退；通过显式构建开关让 custom-backend 构建进入真实路径。
- Wrap the current custom graphics backend around the CPU execution backend, but require it to perform at least one real CUDA module compile + launch during queue submit. This keeps the solid-color triangle test lightweight while proving the custom draw path is no longer a pure stub.  
  让当前 custom graphics backend 包装 CPU execution backend，但在队列提交时至少执行一次真实 CUDA 模块编译与 launch。这样既保持纯色三角形测试轻量，又能证明 custom draw path 不再只是 stub。

**Validation / 验证**

- Backend unit test: compile and launch a tiny CUDA kernel that writes one 32-bit value into device memory, then read it back on the host.  
  Backend 单测：编译并执行一个最小 CUDA kernel，向显存写入一个 32-bit 值，再回读到 host 校验。
- Vulkan compute test: verify custom-backend dispatch uses the real CUDA runtime when the CUDA build flag is enabled.  
  Vulkan compute 测试：在 CUDA 构建开关打开时，验证 custom-backend dispatch 走真实 CUDA runtime。
- Draw smoke test: keep `DrawTest.SolidColorTriangle` green and additionally assert that the custom execution path performed a real CUDA launch in the custom CUDA build.  
  Draw 烟测：保持 `DrawTest.SolidColorTriangle` 通过，并在 custom CUDA 构建中额外断言 custom execution path 发生了真实 CUDA launch。
