# ManuMesh

> 面向增材制造与三角表面网格处理的 C++17 几何内核。

ManuMesh 提供可构建、可测试、可度量并可由外部程序调用的网格处理 SDK。当前核心是由
QEM/line quadrics 候选排序、三角网格特征图和硬合法性过滤共同组成的受约束边坍缩
管线，同时提供独立特征检测、网格分析、STL/OBJ 读写、CLI 和稳定的 C ABI。

## 功能特性

- **网格基础类型**：提供 Eigen-backed `Mesh`、Eigen-free `PlainMesh`、拓扑查询、
  typed handles、校验和紧凑化能力。
- **特征检测**：组合 boundary、non-manifold、oriented dihedral、normal tensor 与可选
  smooth-curvature 证据，生成 feature graph、loop、junction、primitive、component 和
  surface patch 结果。
- **网格简化**：支持标准 QEM、line quadrics、目标比例/面数、候选 placement fallback
  和固定拓扑质量精修。
- **约束保护**：在修改拓扑前检查 link condition、边界策略、法向偏差、三角形质量、
  局部误差、局部相交、特征曲线以及可选 UV chart 约束。
- **网格分析**：提供拓扑统计、三角形质量、采样距离和特征环比较等公共分析入口。
- **文件交换**：读写 STL，读取 OBJ 多边形并执行确定性三角化，同时保留逐角 `vt`。
- **多种调用边界**：提供 C++ SDK、Eigen-free C++ 入口、size-aware C ABI、命令行工具
  和安装后 SDK consumer 示例。
- **验证与诊断**：使用 GoogleTest/CTest、外部网格 fixture、CSV 指标和可选 Debug-only
  HTML wireframe 工具形成验证闭环。

## 系统架构

ManuMesh 将公共 SDK、可复用几何基础、算法实现和应用层分开，特征检测不依赖简化，
简化可以消费预计算的 `FeatureAnalysis`：

```text
include/                         稳定的 C++ SDK 与 C ABI
   |
   +-- core / io / analysis      网格、拓扑、文件交换与公共分析
   +-- feature_detection         特征证据、图、环、原语和曲面分区
   `-- simplification            QEM/line-quadrics 与约束边坍缩入口

src/common + src/mesh_edit       跨算法私有几何基础和动态拓扑
   |
   +-- src/analysis
   +-- src/feature_detection
   `-- src/simplification
          |
          `-- apps      仅通过库接口组织 CLI 工作流
```

核心模块：

| 模块 | 职责 |
| --- | --- |
| `core` | `Mesh`、`PlainMesh`、拓扑、状态、容差和基础生成器。 |
| `common` | 私有几何谓词、邻域查询、距离索引和空间加速结构。 |
| `mesh_edit` | 活动拓扑、局部 incidence 更新以及 compact/remap。 |
| `analysis` | 网格统计、采样距离和跨网格比较。 |
| `feature_detection` | 特征证据提取、图清理/整合、追踪、原语拟合和曲面分区。 |
| `simplification` | QEM/line-quadrics 排序、placement、合法性过滤和局部拓扑修改。 |
| `api` | 不让 C++ 类型或异常穿越边界的 size-aware C ABI。 |
| `apps` | CLI 参数校验、命令分派、CSV 输出和批处理工作流。 |

详细依赖方向和公共/私有边界见
[架构设计](documentation/design/architecture.md)。

### 能力边界

ManuMesh 当前是三角表面网格内核，不是完整 CAD/B-Rep 或实体建模内核。以下能力不在
当前交付范围内：

- B-Rep 拓扑、CAD feature tree 和参数曲面语义恢复；
- 通用 Boolean、offset/thickening、补洞和完整 shape healing；
- 体网格划分和点云曲面重建；
- 全局 Hausdorff/envelope 的形式化认证；
- 不依赖输入质量与参数配置的制造公差保证。

## 系统要求

### 开发环境

| 工具或组件 | 要求 | 用途 |
| --- | --- | --- |
| C++ 编译器 | C++17；MSVC 或 GCC/MinGW | 编译库、CLI 和示例。 |
| CMake | 3.18 或更高 | 配置、构建、安装和测试。 |
| Ninja | 推荐 | 单配置快速构建；也可使用 Visual Studio generator。 |
| Eigen | 3.3 或更高 | 核心向量/矩阵计算；可使用 system、vendored 或 fetch provider。 |
| GoogleTest | 测试时需要 | 可使用 system、vendored、prebuilt 或 fetch provider。 |
| Doxygen | 可选 | 优先使用 `thirdParty/doxygen` 中 vendored 的 1.17.0；缺失时回退到系统安装。 |
| Graphviz | 可选 | 优先使用 `thirdParty/graphviz` 中 vendored 的 15.0.0 `dot` 运行时；缺失时回退到系统安装。 |
| clang-format | 可选 | 执行 `format` 与 `check-format` 目标。 |

Windows 本地开发主路径为 MinGW + Ninja，MSVC + Ninja/Visual Studio generator 作为
SDK 集成与兼容验证路径。源码与 CMake 目标不依赖 GUI 或 Node.js 运行环境。
VS Code 的 `msvc selected` 任务可在执行时选择 `v143`（MSVC 2022）或
`v142`（MSVC 2019）；v142 需要先通过 Visual Studio Installer 安装对应组件。

## 快速开始

### MinGW + Ninja

```powershell
$buildDir = "build/mingw-ninja-release"
cmake -S . -B $buildDir -G Ninja `
  -DCMAKE_MAKE_PROGRAM=ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_C_COMPILER=gcc `
  -DCMAKE_CXX_COMPILER=g++ `
  -DMANUMESH_EIGEN_PROVIDER=vendored `
  -DMANUMESH_GOOGLETEST_PROVIDER=auto
cmake --build $buildDir --parallel
cmake -E chdir $buildDir ctest -LE "performance|external" --output-on-failure
```

在离线机器上使用固定 MinGW 工具链时，可以直接把完整工具链根目录交给 CMake，
不需要永久修改系统 `PATH`：

```powershell
$buildDir = "build/mingw-offline-release"
cmake -S . -B $buildDir -G Ninja `
  -DMANUMESH_MINGW_ROOT="D:/tools/mingw64" `
  -DCMAKE_BUILD_TYPE=Release `
  -DMANUMESH_EIGEN_PROVIDER=vendored `
  -DMANUMESH_GOOGLETEST_PROVIDER=source
```

`MANUMESH_MINGW_ROOT` 必须指向同时包含 `bin/g++.exe` 和
`libexec/gcc/.../cc1plus.exe` 的完整工具链。CMake 会在 `project()` 前固定
`gcc`、`g++` 和 `windres` 的绝对路径；如果工具链目录带有 `bin/ninja.exe`，也会使用
该 Ninja，否则继续使用生成器已找到的 Ninja。配置时还会清理 ID、版本或 C++17
feature 为空的旧 compiler state，并重新执行编译器识别。

查看 CLI 帮助：

```powershell
& "$buildDir/bin/manumesh.exe" --help
```

### MSVC + Ninja

从 Visual Studio Developer Command Prompt，或已包含 `cl.exe`、`rc.exe` 和 `mt.exe`
的终端运行：

```powershell
cmake -S . -B build/msvc-ninja-release -G Ninja `
  -DCMAKE_MAKE_PROGRAM=ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_CXX_COMPILER=cl
cmake --build build/msvc-ninja-release --parallel
```

### Visual Studio 2019 Presets

仓库根目录的 `CMakePresets.json` 仅包含 Visual Studio 2019 配置，覆盖 x64、v142
环境下的 `Visual Studio 16 2019` 和 `Ninja Multi-Config` 两种 generator。项目基础配置
仍支持 CMake 3.18，使用该 preset 文件需要 CMake 3.21 或更高版本。

```powershell
# 日常 Debug 和快速回归
cmake --preset vs2019-debug
cmake --build --preset vs2019-debug-tests
ctest --preset vs2019-debug-unit

# 开启内部 HTML wireframe 调试工具
cmake --preset vs2019-debug-debugutil
cmake --build --preset vs2019-debug-debugutil

# Release 性能测试
cmake --preset vs2019-release-performance
cmake --build --preset vs2019-release-performance
ctest --preset vs2019-release-performance

# 安装并验证独立 SDK consumer
cmake --preset vs2019-release-sdk
cmake --build --preset vs2019-release-sdk
```

Ninja preset 必须从 VS2019 x64 Developer Command Prompt（或已经执行对应
`VsDevCmd.bat` 的终端）运行，确保 `cl`、`rc` 和 `mt` 来自 v142 环境：

```powershell
# Ninja Multi-Config 日常 Debug 和快速回归
cmake --preset vs2019-ninja-debug
cmake --build --preset vs2019-ninja-debug-tests --parallel
ctest --preset vs2019-ninja-debug-unit

# Ninja Debug + HTML wireframe 调试工具
cmake --preset vs2019-ninja-debug-debugutil
cmake --build --preset vs2019-ninja-debug-debugutil --parallel
ctest --preset vs2019-ninja-debug-debugutil-unit

# Ninja Release、性能和 SDK
cmake --preset vs2019-ninja-release
cmake --build --preset vs2019-ninja-release --parallel
ctest --preset vs2019-ninja-release-unit

cmake --preset vs2019-ninja-release-performance
cmake --build --preset vs2019-ninja-release-performance --parallel
ctest --preset vs2019-ninja-release-performance

cmake --preset vs2019-ninja-release-sdk
cmake --build --preset vs2019-ninja-release-sdk --parallel
```

VS Code 的 `Terminal > Run Task...` 中提供对应的
`configure/build/test: vs2019+ninja ... preset` 任务；运行和断点调试可在 Run and Debug 中选择
`(VS2019 Ninja Preset)` 或 `(VS2019 Ninja Preset + debugUtil)` 配置。启动 VS Code
前同样需要进入 VS2019 x64 Developer Command Prompt。

此外还提供 `vs2019-release`、`vs2019-release-static` 以及 unit、external、full、
performance、SDK，以及同名的 `vs2019-ninja-*` build/test preset。每类配置使用独立
构建目录，切换 generator、debugUtil、静态/动态库、性能和安装选项时不会污染已有
CMake cache。

### 安装 SDK

```powershell
cmake -S . -B build/sdk-release -G Ninja `
  -DCMAKE_MAKE_PROGRAM=ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_C_COMPILER=gcc `
  -DCMAKE_CXX_COMPILER=g++ `
  -DMANUMESH_ENABLE_INSTALL=ON `
  -DMANUMESH_EIGEN_PROVIDER=vendored `
  -DMANUMESH_GOOGLETEST_PROVIDER=auto
cmake --build build/sdk-release --target sdk-consumer-test --parallel
```

该目标先安装到 `build/sdk-release/sdk/`，再从 `examples/sdk_consumer/` 配置独立下游
工程，只使用已安装的 `include/`、`lib/`、`bin/` 和 `share/`。Visual Studio
`.vcxproj`、`.props` 和 CMake package 的完整接入步骤见
[SDK 集成指南](documentation/guide/sdk_integration.md)。

## 使用方式

### 命令行

将 STL/OBJ 简化到原面数的 25%，并写出采样指标：

```powershell
$exe = "build/mingw-ninja-release/bin/manumesh.exe"
& $exe --version
& $exe simplify input.stl output.stl `
  --method line `
  --ratio 0.25 `
  --line-weight 1e-3 `
  --preserve-feature-curves `
  --prevent-local-intersections `
  --metrics-csv output/metrics.csv
```

独立输出特征检测结果：

```powershell
& $exe feature-report input.obj `
  --feature-angle-deg 25 `
  --smooth-curvature-features `
  --surface-patches `
  --csv output/features.csv
```

所有命令和参数以 `manumesh --help` 为准。CLI 生成的 STL/CSV 用于结果检查，不是
算法内部状态的替代接口。

### C++ API

```cpp
#include "algorithms/simplification/QEMSimplifier.h"
#include "core/Mesh.h"
#include "io/MeshIo.h"

#include <string>

int main() {
    manumesh::Mesh input;
    std::string error;
    if (!manumesh::loadMesh("input.stl", input, &error)) {
        return 1;
    }

    manumesh::simplification::SimplifyOptions options;
    options.targetRatio = 0.25;
    options.useLineQuadrics = true;
    options.lineWeight = 1e-3;
    options.preserveFeatureCurves = true;
    options.featureProtectionMode =
        manumesh::simplification::FeatureProtectionMode::PrimitiveCurves;

    manumesh::simplification::SimplifyReport report;
    manumesh::simplification::QEMSimplifier simplifier(options);
    manumesh::Mesh output = simplifier.simplify(input, &report);

    return manumesh::saveBinaryStl("output.stl", output, &error)
               ? 0
               : 1;
}
```

安装 CMake package 后，下游工程可以使用：

```cmake
find_package(ManuMesh CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE ManuMesh::manumesh)
manumesh_copy_runtime_dependencies(my_app)
```

需要避免在宿主交换类型中暴露 Eigen 时，使用
`algorithms/simplification/PlainSimplifier.h` 和 `manumesh::PlainMesh`。跨语言或要求
稳定二进制边界时，使用 `api/CApi.h`；输入 options 必须先调用对应初始化函数，输出
report/stats 应使用当前 size-aware wrapper 或显式 `*_with_size` 入口。

## 文档

| 主题 | 说明 |
| --- | --- |
| [文档总入口](documentation/README.md) | 人工维护的交付文档、设计、指南、论文资料和历史笔记索引。 |
| [开发者指南](documentation/delivery/manumesh_kernel_developer_guide.html) | 面向 SDK 集成、算法开发和交付评审的综合手册。 |
| [架构设计](documentation/design/architecture.md) | 模块职责、依赖方向、公开/私有边界和数据策略。 |
| [算法本质](documentation/design/algorithm_essence.md) | QEM/line quadrics、特征图和合法性过滤之间的关系。 |
| [特征识别管线](documentation/generated/notes/manumesh-feature-recognition-pipeline.html) | 特征证据、graph、loop、primitive、junction 和 patch 的实现级说明。 |
| [特征识别调试学习计划](documentation/generated/notes/manumesh-feature-recognition-gtest-debug-learning-plan.html) | 按 GTest 案例学习特征识别九阶段流程与 debugUtil。 |
| [网格简化调试学习计划](documentation/generated/notes/manumesh-simplification-gtest-debug-learning-plan.html) | 按 GTest 案例学习 QEM、edge collapse、保护策略和质量验证。 |
| [基础与 IO 调试学习计划](documentation/generated/notes/manumesh-core-io-analysis-gtest-debug-learning-plan.html) | 学习 Mesh/Topology、动态编辑、空间查询、OBJ/STL 和分析模块。 |
| [API 与 SDK 调试学习计划](documentation/generated/notes/manumesh-api-sdk-gtest-debug-learning-plan.html) | 学习 C++/C ABI、CLI、安装布局和独立 consumer 验证。 |
| [SDK 集成指南](documentation/guide/sdk_integration.md) | 安装目录、C++/C ABI、CMake 和 Visual Studio 接入。 |
| [工业验证](documentation/design/industrial_validation.md) | 能力、数据集、命令、CSV 指标和验收口径。 |
| [测试策略](documentation/design/testing_strategy.md) | 测试分层、解析 fixture、标签、确定性和性能用例。 |
| [调试工具](documentation/guide/debug_util_usage.md) | Debug-only HTML wireframe 工具的启用方式和使用边界。 |
| [算法扩展协议](documentation/design/algorithm_extension_protocol.md) | 新增平级算法模块的职责、API、诊断和测试约定。 |
| [更新日志](CHANGELOG.md) | 版本功能、修复和已验证命令。 |

`documentation/` 是人工维护文档目录。`docs/` 仅用于 Doxygen 生成结果，不存放设计
记录、指南、论文或手工 HTML。

## 项目结构

```text
ManuMesh/
|-- include/                   # 可安装的 C++ SDK 与 C ABI 头文件
|-- src/                       # core/common/mesh_edit/analysis/feature/simplification 实现
|-- apps/             # manumesh 命令行工具
|-- examples/                  # C++、C ABI 和安装后 SDK consumer 示例
|-- tests/                     # unit、external、performance、support 和测试数据
|-- thirdParty/                # Eigen、GoogleTest、Doxygen 与 Graphviz 本地依赖/工具包
|-- adm/                       # 格式化、安装和 SDK 辅助目标
|-- documentation/             # 人工维护的交付文档、设计、指南和论文资料
|-- docs/                      # 仅存放生成的 Doxygen 输出；不纳入版本控制
|-- .vscode/                   # MinGW/MSVC 构建、调试和验证任务
|-- CMakeLists.txt             # 顶层构建配置
|-- Doxyfile.in                # Doxygen 配置模板
|-- CHANGELOG.md               # 更新日志
`-- README.md                  # 项目入口
```

本地生成内容进入 `build*/`、`output/`、`tests/output/` 或 `docs/doxygen/`，不作为源码
或人工文档入口。

## 依赖与构建选项

Eigen 是库的 header-only 编译依赖。GoogleTest 只用于仓库测试，不是 ManuMesh SDK
运行时依赖。安装型 Windows SDK 会把当前工具链所需的运行时 DLL 放入 `bin/`，并在
`sdk-consumer-test` 中使用隔离 `PATH` 验证可启动性。CMake 会根据 provider 选择
system、仓库内依赖或网络拉取版本。

| 选项 | 默认值 | 作用 |
| --- | --- | --- |
| `MANUMESH_BUILD_SHARED_LIBRARY` | `ON` | 构建共享库；关闭后构建静态库。 |
| `MANUMESH_BUILD_CLI` | `ON` | 构建 `manumesh` CLI。 |
| `MANUMESH_BUILD_EXAMPLES` | `ON` | 构建 C/C++ 示例。 |
| `MANUMESH_BUILD_TESTS` | `ON` | 注册 GoogleTest/CTest 测试。 |
| `MANUMESH_BUILD_PERFORMANCE_TESTS` | `OFF` | 构建独立的大模型性能套件。 |
| `MANUMESH_ENABLE_INSTALL` | `OFF` | 启用 SDK 安装和 consumer 验证目标。 |
| `MANUMESH_ENABLE_DEBUG_UTIL` | `OFF` | 在 Debug 构建中启用内部 HTML 线框工具。 |
| `MANUMESH_MINGW_ROOT` | 空 | 可选的完整 MinGW 根目录；在 `project()` 前固定编译器绝对路径。 |
| `MANUMESH_RECOVER_INVALID_COMPILER_STATE` | `ON` | 自动删除 ID、版本或 C++17 feature 不完整的 CMake compiler state。 |
| `MANUMESH_EIGEN_PROVIDER` | `auto` | `auto`、`system`、`vendored` 或 `fetch`。 |
| `MANUMESH_GOOGLETEST_PROVIDER` | `auto` | `auto`、`source`、`prebuilt`、`system` 或 `fetch`。 |
| `MANUMESH_BUILD_DOCS` | `ON` | 在可用时创建 Doxygen `docs-api` 和 `docs-internal` 目标。 |
| `MANUMESH_DOXYGEN_ENABLE_GRAPHS` | `ON` | 检测到 vendored 或系统 Graphviz `dot` 时生成关系图。 |

## 测试

快速回归排除性能和外部大模型用例，适合日常开发：

```powershell
cmake -E chdir build/mingw-ninja-release `
  ctest -LE "performance|external" --output-on-failure
```

运行全部非性能测试：

```powershell
cmake -E chdir build/mingw-ninja-release `
  ctest -LE performance --output-on-failure
```

只运行外部模型用例：

```powershell
cmake -E chdir build/mingw-ninja-release `
  ctest -L external --output-on-failure
```

性能套件使用独立构建目录，避免改变日常测试配置：

```powershell
$perfBuildDir = "build/mingw-ninja-release-performance"
cmake -S . -B $perfBuildDir -G Ninja `
  -DCMAKE_MAKE_PROGRAM=ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_C_COMPILER=gcc `
  -DCMAKE_CXX_COMPILER=g++ `
  -DMANUMESH_BUILD_PERFORMANCE_TESTS=ON `
  -DMANUMESH_EIGEN_PROVIDER=vendored `
  -DMANUMESH_GOOGLETEST_PROVIDER=auto
cmake --build $perfBuildDir --target performance-tests --parallel
```

测试结果之外，工业验证还要求检查 CLI 生成的 STL/CSV：拓扑、采样距离、三角形
质量、feature component、拒绝计数和求解 fallback 应结合输入模型与目标比例解释。

## Doxygen 文档

配置阶段检测到 Doxygen 后，可分别生成公开/API 参考和内部源码参考：

```powershell
cmake --build build/mingw-ninja-release --target check-src-doxygen
cmake --build build/mingw-ninja-release --target docs-api --parallel
cmake --build build/mingw-ninja-release --target docs-internal --parallel
```

生成首页分别位于：

```text
docs/doxygen/html/index.html
docs/internal/html/index.html
```

`docs-api` 输入包括 `documentation/doxygen/` 页面、`include/`、`src/`、`apps/` 和
`examples/`。`docs-internal` 专注于 `include/` 与 `src/`，启用 private/static/local/anonymous-namespace
符号、内联源码、caller/callee 关系和 `@internal` 内容。`src/` 统一使用显式
`/** ... */` Doxygen 块：每个文件都有 `@file`、`@brief`、`@ingroup`，内部类型和头文件
接口有独立说明，局部实现推导继续使用普通 `//`。`check-src-doxygen` 会拒绝混用
`///`、`//!`、`///<`；两个文档目标生成前都会自动执行该检查。内部站点仍使用
`EXTRACT_ALL=YES` 以完整展示短小局部辅助实现。`thirdParty/` 不参与文档抽取；Graphviz
不可用时只会省略关系图。

## 版本记录

当前项目版本为 `0.2.0`，由顶层 CMake 定义。各批次的功能、兼容性调整、算法修复和验证记录见
[CHANGELOG.md](CHANGELOG.md)。
