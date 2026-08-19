# 构建、测试与文档

## 支持环境

仓库的受支持 preset 和安装后 package 以以下工具链基线为准；顶层 `CMakeLists.txt` 会拒绝非
MSVC/v142/x64 配置及其他 Visual Studio generator：

- Visual Studio 16 2019，MSVC v142，x64；
- CMake 3.20 或更高，C++14；
- Python 3（标准测试 preset 开启 include 边界架构检查）；
- 默认使用仓库内 Eigen；测试默认使用仓库内 GoogleTest；
- Release preset 启用仓库内 oneTBB，Debug/ASan 默认保持串行；
- 生成 Doxygen 时需要 Doxygen；优先从 `thirdParty/doxygen` 查找，Graphviz
  使用仓库内工具包时仅影响关系图。

Ninja preset 也必须从 VS2019 x64 Developer Command Prompt 运行。不要使用旧的 MinGW、VS2022
或其他编译器路径；它们会在配置阶段失败。

仓库不执行依赖联网下载。Eigen、GoogleTest 和 oneTBB 的默认 provider 都指向
`thirdParty/`；`auto` 只在仓库内置包和本机可发现的 system 包之间选择。封闭环境建议使用
全新的构建目录，并显式固定 provider：

```powershell
cmake --preset vs2019-release-sdk `
  -DMANUMESH_EIGEN_PROVIDER=vendored `
  -DMANUMESH_GOOGLETEST_PROVIDER=source `
  -DMANUMESH_ONETBB_PROVIDER=vendored
```

如果第三方目录不完整，配置会直接失败并报告缺失路径，不会尝试访问公网。

## 日常构建

```powershell
# 独立 MeshCore Debug：只有核心功能项目，不创建 CLI、测试、示例、第三方或 CMake 检查项目
cmake --preset vs2019-debug-meshcore
cmake --build --preset vs2019-debug-meshcore --parallel

# 日常 Debug：按功能模块生成核心工程和 CLI，不生成测试、示例或开发工具
cmake --preset vs2019-debug
cmake --build --preset vs2019-debug --parallel

# 完整 Debug 验证：测试、示例和开发辅助目标
cmake --preset vs2019-debug-full
cmake --build --preset vs2019-debug-full-tests --parallel
ctest --preset vs2019-debug-full-unit

# Release：正式后端和完整测试
cmake --preset vs2019-release
cmake --build --preset vs2019-release-tests --parallel
ctest --preset vs2019-release-unit
ctest --preset vs2019-release-full
```

带 CLI 的常规构建输出位于 `build/<preset>/bin/<Configuration>/`，CLI 名称为 `manumesh.exe`。
精简的 `vs2019-debug` 按 `src/` 功能目录生成核心工程，并关闭 CMake 自动重生成以移除
`ZERO_CHECK`；修改 CMake 配置或增删
源码文件后，需要重新执行 `cmake --preset vs2019-debug`。

`vs2019-debug-meshcore` 只交付
`build/vs2019-debug-meshcore/bin/Debug/MeshCore.dll` 和
`build/vs2019-debug-meshcore/lib/Debug/MeshCore.lib`。解决方案包含 `Core`、`Common`、`MeshEdit`、
`IO`、`Analysis`、`FeatureDetection`、`Simplification` 和 `ALL_BUILD`；只有 `Core` 是共享库，
其余功能项目为用于源码浏览的 CMake `OBJECT` 目标，并入 `MeshCore` 后不作为独立交付库。它保留
普通 STL/OBJ 读写、网格分析、特征检测和特征保护 QEM 简化；不包含 CLI、测试、示例、C API、MMPD、
纹理坐标简化、oneTBB 或第三方 Visual Studio 项目。该 preset 同样关闭自动重生成；修改 CMake 或
MeshCore 源文件后，重新执行 `cmake --preset vs2019-debug-meshcore`。

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

Doxygen 生成页面默认使用英文界面。需要中文界面时，在配置文档 preset 时覆盖
`MANUMESH_DOXYGEN_OUTPUT_LANGUAGE`：

```powershell
cmake --preset vs2019-release-docs -DMANUMESH_DOXYGEN_OUTPUT_LANGUAGE=Chinese
```
