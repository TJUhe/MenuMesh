# 构建、测试与文档

## 支持环境

仓库的受支持 preset 和安装后 package 以以下工具链基线为准；顶层 `CMakeLists.txt` 会拒绝非
MSVC/v142/x64 配置及其他 Visual Studio generator：

- Visual Studio 16 2019，MSVC v142，x64；
- CMake 3.20 或更高，C++14；
- Python 3（标准测试 preset 开启 include 边界架构检查）；
- 默认使用仓库内 Eigen；测试默认使用仓库内 GoogleTest；
- Release preset 启用仓库内 oneTBB，Debug/ASan 默认保持串行；
- 生成 Doxygen 时需要 Doxygen；Graphviz 仅影响关系图。

Ninja preset 也必须从 VS2019 x64 Developer Command Prompt 运行。不要使用旧的 MinGW、VS2022
或其他编译器路径；它们会在配置阶段失败。

## 日常构建

```powershell
# Debug：库、CLI、示例和测试
cmake --preset vs2019-debug
cmake --build --preset vs2019-debug-tests --parallel
ctest --preset vs2019-debug-unit

# Release：正式后端和完整测试
cmake --preset vs2019-release
cmake --build --preset vs2019-release-tests --parallel
ctest --preset vs2019-release-unit
ctest --preset vs2019-release-full
```

构建输出位于 `build/<preset>/bin/<Configuration>/`，CLI 名称为 `manumesh.exe`。

## 测试分层

```powershell
# 外部网格用例
ctest --preset vs2019-release-external

# 性能套件（独立构建目录）
cmake --preset vs2019-release-performance
cmake --build --preset vs2019-release-performance --parallel
ctest --preset vs2019-release-performance

# MSVC AddressSanitizer
cmake --preset vs2019-asan
cmake --build --preset vs2019-asan --parallel
ctest --preset vs2019-asan-unit
```

测试名称和标签由 `tests/CMakeLists.txt` 动态注册；不要在文档中固化某个测试总数。Visual
Studio 多配置构建需要明确配置；查看当前 Release 清单时运行
`ctest --test-dir build/vs2019-release -C Release -N`。`unit` 套件排除 `external` 和
`performance`，`full` 包含已构建的非性能测试。

## SDK 安装

```powershell
cmake --preset vs2019-release-sdk
cmake --build --preset vs2019-release-sdk --parallel
ctest --preset vs2019-release-sdk
```

安装验证会构建独立的 `examples/sdk_consumer/`，只使用安装前缀中的头文件、库、运行时和
CMake package。静态 SDK 使用 `vs2019-release-static-sdk`。安装目标默认关闭，避免普通开发
构建污染工作树。

## Doxygen

```powershell
cmake --preset vs2019-release-docs
cmake --build build/vs2019-release-docs --config Release --target check-src-doxygen
cmake --build --preset vs2019-release-docs --target docs-api --parallel
cmake --build --preset vs2019-release-docs --target docs-internal --parallel
```

`docs-api` 生成公开 API 和示例参考，`docs-internal` 生成 `include/` 与 `src/` 的内部源码
参考。两个目标都会先检查源码 Doxygen 注释格式；生成结果不纳入版本控制。
