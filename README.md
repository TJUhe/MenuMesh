# ManuMesh

> 面向增材制造与三角表面网格处理的 C++14 几何内核。

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
   +-- feature_detection         特征证据、图、环、几何基元和曲面分区
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
| `feature_detection` | 特征证据提取、图清理/整合、追踪、几何基元拟合和曲面分区。 |
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
| Visual Studio | Visual Studio 16 2019 16.11，MSVC v142，x64 | 唯一受支持的编译与 SDK 消费基线。 |
| CMake | 3.20 或更高 | 配置、构建、安装和测试；兼容 VS2019 16.11 内置 CMake。 |
| Ninja | 可选 | 在 VS2019 v142 Developer Command Prompt 中使用 Ninja Multi-Config preset。 |
| Eigen | 默认 3.4.0；显式 system 为 3.3+ | 核心向量/矩阵计算；默认使用仓库内 header bundle，`system` 与 `fetch` 仅供显式覆盖。 |
| GoogleTest | 1.15.2（测试时需要） | 默认从仓库内源码构建；`system` 与 `fetch` 仅供显式覆盖。 |
| Doxygen | 文档构建时需要 | 通过 `PATH` 或 `DOXYGEN_EXECUTABLE` 提供；`MANUMESH_BUILD_DOCS=ON` 时配置阶段会验证。 |
| Graphviz | 可选 | 找到 `dot` 时生成关系图；缺少 Graphviz 时 Doxygen 仍可生成 API 和源码文档，但不包含关系图。 |
| Python 3 | 架构检查时需要 | VS2019 preset 和 CI 会开启 include 边界守卫；自定义测试配置可在没有 Python 时跳过该增强检查。 |
| clang-format | 22.x，可选 | 执行 `format` 与 `check-format` 目标；缺失或主版本不匹配时目标会明确失败。 |

仓库的 Windows 构建基线为 Visual Studio 2019、MSVC v142 和 x64。可以使用
`Visual Studio 16 2019` generator，也可以从 VS2019 x64 Developer Command Prompt
使用 Ninja Multi-Config；CMake 会在配置阶段验证编译器、工具集和目标架构。
所有仓库目标统一使用 DLL CRT：Debug、Release 和安装后的 SDK consumer 都是 `/MD`，
Debug 不使用 `/MDd`，也不允许 `/MT` 或 `/MTd` 混入同一构建。
源码与 CMake 目标不依赖 GUI 或 Node.js 运行环境。

## 快速开始

### Visual Studio 2019 Presets

仓库根目录的 `CMakePresets.json` 仅包含 Visual Studio 2019 配置，覆盖 x64、v142
环境下的 `Visual Studio 16 2019` 和 `Ninja Multi-Config` 两种 generator。项目与 preset
统一要求 CMake 3.20 或更高版本，因此可以由 Visual Studio 2019 16.11 内置的 CMake 3.20
直接读取。

```powershell
# 日常 Debug 和快速回归
cmake --preset vs2019-debug
cmake --build --preset vs2019-debug-tests
ctest --preset vs2019-debug-unit

# AddressSanitizer 内存安全回归（排除独立性能阈值和外部数据集）
cmake --preset vs2019-asan
cmake --build --preset vs2019-asan
ctest --preset vs2019-asan-unit

# Release 性能测试
cmake --preset vs2019-release-performance
cmake --build --preset vs2019-release-performance
ctest --preset vs2019-release-performance

# 安装并验证独立 SDK consumer
cmake --preset vs2019-release-sdk
cmake --build --preset vs2019-release-sdk
ctest --preset vs2019-release-sdk

# 安装并验证静态库 SDK 的 C/C++ consumer
cmake --preset vs2019-release-static-sdk
cmake --build --preset vs2019-release-static-sdk
ctest --preset vs2019-release-static-sdk
```

VS2019 v142 的 Windows AddressSanitizer 不支持 LeakSanitizer 的
`detect_leaks=1`。`vs2019-asan-unit` 因此同时运行
`ownership_lifetime_stress`：它在 64 轮 C/C++ API 生命周期中统计未释放的 C++ 分配；
ASan 则负责检查越界、释放后使用和重复释放。

Ninja preset 必须从 VS2019 x64 Developer Command Prompt（或已经执行对应
`VsDevCmd.bat` 的终端）运行，确保 `cl`、`rc` 和 `mt` 来自 v142 环境：

```powershell
# Ninja Multi-Config 日常 Debug 和快速回归
cmake --preset vs2019-ninja-debug
cmake --build --preset vs2019-ninja-debug-tests --parallel
ctest --preset vs2019-ninja-debug-unit

# Ninja Release、性能和 SDK
cmake --preset vs2019-ninja-release
cmake --build --preset vs2019-ninja-release --parallel
ctest --preset vs2019-ninja-release-unit

cmake --preset vs2019-ninja-release-performance
cmake --build --preset vs2019-ninja-release-performance --parallel
ctest --preset vs2019-ninja-release-performance

cmake --preset vs2019-ninja-release-sdk
cmake --build --preset vs2019-ninja-release-sdk --parallel
ctest --preset vs2019-ninja-release-sdk

cmake --preset vs2019-ninja-release-static-sdk
cmake --build --preset vs2019-ninja-release-static-sdk --parallel
ctest --preset vs2019-ninja-release-static-sdk
```

VS Code 的 `Terminal > Run Task...` 中提供对应的 `configure/build/test: vs2019 ...`
任务；运行和断点调试可在 Run and Debug 中选择 `VS2019 Debug CLI - Feature Curves`、
`VS2019 Debug CLI - Feature Report`、`VS2019 Debug Unit Tests - Filter` 或
`VS2019 Debug C ABI Stress`。内存问题可使用
`VS2019 ASan Unit Tests - Filter` 或 `VS2019 ASan Ownership Lifetime Stress`。

此外还提供 `vs2019-release`、`vs2019-release-static`、`vs2019-release-static-sdk` 以及 unit、external、full、
performance、SDK，以及同名的 `vs2019-ninja-*` build/test preset。每类配置使用独立
构建目录，切换 generator、debugUtil、静态/动态库、性能和安装选项时不会污染已有
CMake cache。

### 安装 SDK

```powershell
cmake --preset vs2019-release-sdk
cmake --build --preset vs2019-release-sdk --parallel
ctest --preset vs2019-release-sdk
```

CTest 会先安装到 `build/vs2019-release-sdk/sdk/`，再从 `examples/sdk_consumer/` 配置独立下游
工程，只使用已安装的 `include/`、`lib/`、`bin/` 和 `share/`。Visual Studio
`.vcxproj`、`.props` 和 CMake package 的完整接入步骤见
[SDK 集成指南](documentation/guide/sdk_integration.md)。

## 使用方式

### 命令行

将 STL/OBJ 简化到原面数的 25%，并写出采样指标：

```powershell
$exe = "build/vs2019-release/bin/Release/manumesh.exe"
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
#include "algorithms/feature_detection/FeatureTypes.h"
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

    manumesh::simplification::SimplifyConfig config;
    config.target = manumesh::simplification::SimplifyTarget::ratio(0.25);
    config.cost.lineQuadrics =
        manumesh::simplification::LineQuadricConfig::uniform(1e-3);
    config.features.enabled = true;
    config.features.protectionMode =
        manumesh::simplification::FeatureProtectionMode::PrimitiveCurves;
    manumesh::feature::FeatureOptions featureOptions;
    featureOptions.featureAngleDeg = 35.0;
    config.features.detection = featureOptions;

    manumesh::simplification::SimplifyReport report;
    manumesh::simplification::QEMSimplifier simplifier;
    simplifier.setConfig(config);
    manumesh::Mesh output = simplifier.simplify(input, &report);

    return manumesh::saveBinaryStl("output.stl", output, &error)
               ? 0
               : 1;
}
```

特征数据流保持为 `Mesh -> FeatureAnalysis -> simplification`。需要跨流程复用检测结果时，
先通过 `FeatureDetector` 生成 `FeatureAnalysis`，再传给预计算分析重载；该分析会绑定输入的
精确 indexed geometry。若 `weightMode=NormalTensor`，分析中必须包含覆盖全部顶点的
`normalTensorVertexWeights`，简化器会直接复用检测时解析好的权重，不会按另一套参数重算。

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
|-- thirdParty/                # Eigen、GoogleTest 与 Graphviz 本地依赖/工具包
|-- adm/                       # 格式化、安装和 SDK 辅助目标
|-- documentation/             # 人工维护的交付文档、设计、指南和论文资料
|-- docs/                      # 仅存放生成的 Doxygen 输出；不纳入版本控制
|-- .vscode/                   # VS2019 构建、调试和验证任务
|-- CMakeLists.txt             # 顶层构建配置
|-- Doxyfile.in                # Doxygen 配置模板
|-- CHANGELOG.md               # 更新日志
`-- README.md                  # 项目入口
```

本地生成内容进入 `build*/`、`output/`、`tests/output/` 或 `docs/doxygen/`，不作为源码
或人工文档入口。

## 依赖与构建选项

Eigen 是库的 header-only 编译依赖。GoogleTest 只用于仓库测试，不是 ManuMesh SDK
运行时依赖。默认配置固定使用仓库内 Eigen 3.4.0 与 GoogleTest 1.15.2 源码，避免
开发机安装的包改变构建结果；显式 `system` 路径接受 Eigen 3.3 及更高版本，`auto`、
`system` 和 `fetch` 都是显式覆盖路径。安装型 Windows
SDK 会把当前工具链所需的运行时 DLL 放入 `bin/`，并在 `sdk-consumer-test` 中使用隔离
`PATH` 验证可启动性。

| 选项 | 默认值 | 作用 |
| --- | --- | --- |
| `MANUMESH_BUILD_SHARED_LIBRARY` | `ON` | 构建共享库；关闭后构建静态库。 |
| `MANUMESH_BUILD_CLI` | `ON` | 构建 `manumesh` CLI。 |
| `MANUMESH_BUILD_EXAMPLES` | `ON` | 构建 C/C++ 示例。 |
| `MANUMESH_BUILD_TESTS` | `ON` | 注册 GoogleTest/CTest 测试。 |
| `MANUMESH_REQUIRE_ARCHITECTURE_CHECKS` | `OFF` | 测试构建时要求 Python 3 并注册 include 边界守卫；VS2019 preset 和 CI 显式设为 `ON`。 |
| `MANUMESH_BUILD_PERFORMANCE_TESTS` | `OFF` | 构建独立的大模型性能套件。 |
| `MANUMESH_ENABLE_INSTALL` | `OFF` | 启用 SDK 安装和 consumer 验证目标。 |
| `MANUMESH_ENABLE_DEBUG_UTIL` | `OFF` | 在 Debug 构建中启用内部 HTML 线框工具。 |
| `MANUMESH_EIGEN_PROVIDER` | `vendored` | 默认仓库内 Eigen 3.4.0；显式 `system` 支持 3.3+，也可设为 `auto` 或 `fetch`。 |
| `MANUMESH_GOOGLETEST_PROVIDER` | `source` | 默认仓库内 GoogleTest 1.15.2 源码；可显式设为 `auto`、`system` 或 `fetch`。 |
| `MANUMESH_BUILD_DOCS` | `OFF` | 开启后要求 Doxygen 可用，并创建 `docs-api` 和 `docs-internal` 目标。 |
| `MANUMESH_DOXYGEN_ENABLE_GRAPHS` | `ON` | 找到 vendored 或系统 Graphviz `dot` 时生成关系图；缺少 `dot` 时继续生成无图文档。 |

## 测试

快速回归排除性能和外部大模型用例，适合日常开发：

```powershell
ctest --preset vs2019-release-unit
```

运行全部非性能测试：

```powershell
ctest --preset vs2019-release-full
```

只运行外部模型用例：

```powershell
ctest --preset vs2019-release-external
```

性能套件使用独立构建目录，避免改变日常测试配置：

```powershell
cmake --preset vs2019-release-performance
cmake --build --preset vs2019-release-performance --parallel
ctest --preset vs2019-release-performance
```

测试结果之外，工业验证还要求检查 CLI 生成的 STL/CSV：拓扑、采样距离、三角形
质量、feature component、拒绝计数和求解 fallback 应结合输入模型与目标比例解释。

## Doxygen 文档

文档 preset 会在配置阶段要求 Doxygen 可用，然后可分别生成公开/API 参考和内部源码参考：

```powershell
cmake --preset vs2019-release-docs
cmake --build build/vs2019-release-docs --config Release --target check-src-doxygen
cmake --build --preset vs2019-release-docs --parallel
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
