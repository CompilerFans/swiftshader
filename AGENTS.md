# Repository Guidelines

## 项目结构与模块组织
`src/` 是核心实现，主要子目录包括 `Vulkan/`、`Device/`、`Pipeline/`、`System/`、`WSI/` 与 `Reactor/`。`include/` 放公共头文件。`tests/` 包含单元测试、基准与回归工具；常见入口有 `tests/VulkanUnitTests/`、`tests/SystemUnitTests/` 和 `tests/regres/`。`docs/` 保存架构与 dEQP 文档，`third_party/` 为外部依赖，通常不要直接改动，除非同步上游或修复明确的集成问题。

## 构建、测试与开发命令
- `cmake -S . -B build && cmake --build build --parallel`：配置并并行构建默认目标。
- `./build/vk-unittests`：运行 Vulkan 单元测试。
- `./build/system-unittests`：运行 System 相关单元测试。
- `./tests/presubmit.sh`：执行提交前检查，包括 `clang-format`、`gofmt`、构建文件校验和源码扫描。
- `tests/regres/run.sh`：运行回归测试列表；大改动前先确认所需测试集。

## 代码风格与命名约定
C++ 代码遵循根目录 `.clang-format`：4 列缩进，允许使用制表符做前导缩进，头文件与实现文件使用 `.hpp` / `.cpp` 配对。类型名、类名使用 `PascalCase`，函数与局部变量使用 `camelCase`，宏与编译开关使用全大写。测试文件倾向于按模块命名，例如 `LRUCacheTests.cpp`、`ComputeTests.cpp`。Go 工具代码放在 `tests/` 下，提交前运行 `gofmt`。

## 测试指南
单元测试基于 GoogleTest；新增功能或修复应优先补到对应模块旁边的测试目录。测试名使用 `TEST(SuiteName, CaseName)`，`SuiteName` 应体现子系统。涉及 Vulkan 行为或一致性时，补充 `vk-unittests` 或参考 `docs/dEQP.md` 的 dEQP 验证流程。

新增或扩展图形绘制相关 case 时，默认需要保存一张可检查结果的图片产物；优先复用 `DrawTester::saveFrame()`，并把产物写到构建目录下的 `draw-test-artifacts/`。除非测试本身不产生可视结果，否则不要省略图片 dump。

## 提交与评审要求
提交信息应简短、祈使句风格，可带模块前缀，如 `Vulkan:`、`Regres:`。本仓库通过 Gerrit 评审，不走 GitHub Pull Request：提交前安装 `commit-msg` hook 生成 `Change-Id`，然后使用 `git push origin HEAD:refs/for/master` 上传。变更说明应写清动机、主要实现和已运行测试；较大改动建议先在 issue tracker 沟通，并确保已签署 CLA。
