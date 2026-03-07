# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 构建

```bash
# 配置并构建（在 build/ 目录下）
cd build
cmake ..
cmake --build . --parallel

# 或者一步完成
cmake -S . -B build && cmake --build build --parallel
```

构建产物包括：
- `build/libvk_swiftshader.so` / `.dll` — Vulkan ICD 库（主要产物）
- `build/vk-unittests` — Vulkan 单元测试
- `build/system-unittests` — System 单元测试
- `build/ReactorUnitTests` — Reactor 单元测试

## 测试

```bash
# 运行 Vulkan 单元测试
./build/vk-unittests

# 运行指定测试用例（GoogleTest 过滤）
./build/vk-unittests --gtest_filter=SuiteName.CaseName

# 运行 system 和 reactor 测试
./build/system-unittests
./build/ReactorUnitTests

# 提交前检查（clang-format、gofmt、构建文件校验）
./tests/presubmit.sh

# dEQP 回归测试（见 docs/dEQP.md）
# tests/regres/ 下有完整的回归测试工具
```

## 架构分层

SwiftShader 分为四层（从上到下）：

```
Vulkan API 层 (src/Vulkan/)
    ↓  实现 Vulkan 1.3，管理对象生命周期和 API 状态
Device/Pipeline 层 (src/Device/, src/Pipeline/)
    ↓  渲染器：生成专用处理例程，协调任务执行
       Device/: Context, PixelProcessor, Blitter, 纹理解码
       Pipeline/: PixelRoutine, SamplerCore, ShaderCore, SPIR-V 处理
Reactor 层 (src/Reactor/)
    ↓  C++ 嵌入式 DSL，用于运行时动态代码生成
       Reactor 类型（Float、Int4 等）通过运算符重载记录操作
JIT 层 (third_party/llvm-16.0/, third_party/subzero/)
       将 Reactor 的中间表示编译为可执行机器码
```

Vulkan API 调用从 `src/Vulkan/libVulkan.cpp` 进入，分发到各 `Vk*.cpp` 对象（`VkCommandBuffer`、`VkPipeline` 等），最终由 `src/Pipeline/` 中的着色器例程执行。

### 关键子系统

- **src/Reactor/**: 核心 JIT 框架，`LLVMReactor.cpp` 是 LLVM 后端实现
- **src/Pipeline/SamplerCore.cpp**: 纹理采样
- **src/Pipeline/ShaderCore.cpp**: SPIR-V 着色器指令执行
- **src/WSI/**: 窗口系统集成（平台相关的 surface 支持）
- **src/System/**: 系统抽象层（内存、线程、CPUID）

## 代码风格

遵循根目录 `.clang-format`（基于 Google 风格，4 列缩进）：
- 类型名、类名：`PascalCase`
- 函数、局部变量：`camelCase`
- 宏和编译开关：`UPPER_CASE`

## 提交规范

本项目使用 Gerrit（不是 GitHub PR）：
- 提交信息格式：`模块: 简短描述`，如 `Vulkan: fix fence reset` 或 `Reactor: add SIMD intrinsic`
- 上传审查：`git push origin HEAD:refs/for/master`
- 需要安装 `commit-msg` hook 生成 `Change-Id`（见 README.md）
