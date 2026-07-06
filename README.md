# ManuMesh

ManuMesh 是一个面向增材制造的 C++17 多边形网格几何内核。当前重点是复现并扩展
`Controlling Quadric Error Simplification with Line Quadrics` 的 QEM + line
quadrics 思路，并提供可构建、可测试、可度量、可由外部程序调用的库接口。

当前产品名是 ManuMesh；源码级兼容标识仍保留 `line_quadrics_qem` 库目标、`linequadrics` CLI、`lq` namespace 和 `include/line_quadrics_qem/` 头文件路径。

当前仓库刻意不再把网页预览作为主入口。结果检查走更稳定的工业验证路径：

```text
CLI 生成 STL/CSV -> CTest/API 示例验证 -> 用 MeshLab/CAD Assistant/系统 STL 查看器目检 STL
```

## 说明网页

这些 HTML 是论文说明和 ManuMesh 当前程序结果的可浏览历史笔记，可以直接用浏览器打开：

- [QEM 与 Line Quadrics 说明](docs/generated/notes/qem-line-quadrics-notes.html)
- [Line Quadrics 数学原理展开](docs/generated/notes/line-quadrics-qem-theory-explained.html)
- [ManuMesh 程序原理说明](docs/generated/notes/line-quadrics-qem-program-principles.html)
- [ManuMesh 代码阅读手册](docs/generated/notes/line-quadrics-qem-code-manual.html)
- [凸台圆孔等圆特征实践结果](docs/generated/notes/circular-feature-practice-results.html)
- [QEM 相关开源库与 ManuMesh 当前实现对比](docs/generated/notes/qem-library-comparison.html)

## ManuMesh 工业内核边界

核心交付物：

| 路径 | 角色 |
| --- | --- |
| `include/line_quadrics_qem/` | 公共 SDK 根目录；稳定入口是 `core/`、`algorithms/` 和 `api/`。 |
| `include/line_quadrics_qem/algorithms/feature_detection/` | 平级特征检测模块，提供 `FeatureDetector`、`FeatureOptions` 和 `FeatureAnalysis`。 |
| `include/line_quadrics_qem/features/` | 旧 include 路径兼容层，新代码不要继续使用。 |
| `src/` | 库实现按职责分组：`common/`、`core/`、`feature_detection/`、`simplification/` 和 `api/`。 |
| `src/common/detail/` | 跨算法私有工具层，例如 mesh key、边-面邻接、面法向、顶点邻接和边界顶点查询；不属于 SDK。 |
| `src/<domain>/detail/` | 不安装的算法私有实现头文件。 |
| `apps/linequadrics/` | `linequadrics` CLI，作为库的应用层消费者。 |
| `tests/` | GoogleTest/CTest 回归测试，按 `support/`、`unit/`、`performance/` 和 `data/` 分类。 |
| `thirdParty/eigen/` | Eigen 头文件包；Eigen 是 header-only，不存在需要链接的动态库。 |
| `thirdParty/googletest/` | GoogleTest 预编译包，默认用于本仓库测试，不作为 SDK 运行时依赖。 |
| `examples/basic_simplify.cpp` | 模拟外部客户程序调用动态库。 |
| `examples/sdk_consumer/` | 真正按“已安装 SDK”方式验证的下游小工程。 |
| `adm/templates/` | SDK 安装辅助模板，目前用于生成 Visual Studio `.props`；普通编译不依赖它。 |
| `adm/vscode-extensions/` | 适配 VSCode 1.70.2 的 C++/CMake 离线 VSIX 插件包。 |
| `docs/design/industrial_library.md` | 动态库、安装、运行时布局和集成边界。 |
| `docs/design/industrial_validation.md` | 每项能力/性能的验证命令、CSV 指标和通过标准。 |

可生成或可选内容：

| 路径 | 说明 |
| --- | --- |
| `build*/` | CMake 构建目录，生成物。 |
| `output/` | demo/手工实验输出，属于本地生成物。 |
| `tests/data/` | 单元测试、性能测试和外部验证输入。 |
| `tests/output/` | 测试/验证生成的 STL、CSV 等输出，属于本地生成物。 |
| `docs/generated/notes/*.html`、`docs/api/` | 生成/导出的说明文档，不作为源码入口。 |

网页 viewer、Node/Vite 配置已经从源码入口中筛掉。要看形状时，直接打开 CLI
生成的 STL；要看性能和误差时，读取 CSV 指标。

## 构建

推荐 MinGW + Ninja。`--parallel` 不指定数字时，CMake 会让 Ninja 按可用核心数并行：

```powershell
cmake -S . -B build/mingw-ninja-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build/mingw-ninja-release --target linequadrics --parallel
```

MSVC + Ninja 可作为备用链路，需要从 VS Developer Command Prompt 或已经带有
`cl.exe`、`rc.exe`、`mt.exe` 的终端启动：

```powershell
cmake -S . -B build/msvc-ninja-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=cl -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build/msvc-ninja-release --target linequadrics --parallel
```

完整 ManuMesh 构建、测试、文档目标：

```powershell
cmake -S . -B build/industrial -DCMAKE_BUILD_TYPE=Release
cmake --build build/industrial --parallel
cmake -E chdir build/industrial ctest --output-on-failure
cmake --build build/industrial --target docs-api
```

查看并验证 SDK 发布目录：

```powershell
cmake -S . -B build/sdk-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DLQ_ENABLE_INSTALL=ON -DLQ_GOOGLETEST_PROVIDER=auto -DLQ_EIGEN_PROVIDER=vendored
cmake --build build/sdk-release --target sdk-consumer-test --parallel
```

该目标会安装到 `build/sdk-release/sdk/`，再用 `examples/sdk_consumer/`
作为独立下游工程，只通过这个 SDK 目录编译并运行 C++/C 两个示例。

## 核心 API

最小外部调用入口：

```cpp
#include "line_quadrics_qem/core/Mesh.h"
#include "line_quadrics_qem/algorithms/simplification/QEMSimplifier.h"

lq::SimplifyOptions options;
options.targetRatio = 0.2;
options.useLineQuadrics = true;
options.lineWeight = 1e-3;
options.preserveFeatureCurves = true;
options.featureProtectionMode = lq::FeatureProtectionMode::PrimitiveCurves;

lq::SimplifyReport report;
lq::QEMSimplifier simplifier(options);
lq::Mesh simplified = simplifier.simplify(input, &report);
```

`FeatureProtectionMode::PrimitiveCurves` 是默认特征保护策略。它会硬保护拟合出的圆、近圆和椭圆 loop，普通折线/二面角 crease 则作为软成本和合法性过滤输入。只有需要复现旧版严格锁边行为时，才使用 `AllFeatureEdges`。

特征检测也可以独立使用，不需要先运行 QEM：

```cpp
#include "line_quadrics_qem/algorithms/feature_detection/FeatureDetector.h"

lq::FeatureOptions featureOptions;
lq::FeatureDetector detector(featureOptions);
lq::FeatureAnalysis features = detector.analyze(input);
```

安装后推荐外部程序直接使用 SDK 的 `include/`、`lib/` 和 `bin/`。
如果下游本身也是 CMake 工程，并且安装 SDK 时打开了
`-DLQ_INSTALL_CMAKE_CONFIG=ON`，也可以选择使用可选的 CMake config：

```cmake
find_package(line_quadrics_qem CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE line_quadrics_qem::line_quadrics_qem)
line_quadrics_qem_copy_runtime_dependencies(my_app)
```

## Visual Studio 使用

这是推荐的工业 SDK 集成方式：先把库安装成一个 SDK 目录，再让 Visual Studio
工程引用安装产物。下游不需要依赖本仓库源码，也不需要使用 `find_package`。

```powershell
cmake -S . -B build/vs-release -G "Visual Studio 17 2022" -A x64 -DLQ_ENABLE_INSTALL=ON
cmake --build build/vs-release --config Release --parallel
cmake --install build/vs-release --config Release --prefix C:\opt\manumesh
```

安装后的关键目录：

```text
C:\opt\manumesh
  bin\line_quadrics_qem.dll
  include\line_quadrics_qem\...
  lib\line_quadrics_qem.lib
  share\line_quadrics_qem\thirdParty\eigen\include\Eigen\...
  share\line_quadrics_qem\msvc\line_quadrics_qem.props
```

### 可选：Visual Studio CMake 工程

只有下游工程本身采用 CMake，并且安装 SDK 时显式打开
`-DLQ_INSTALL_CMAKE_CONFIG=ON`，才需要这一段。在你的 `CMakeLists.txt` 中使用：

```cmake
find_package(line_quadrics_qem CONFIG REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE line_quadrics_qem::line_quadrics_qem)
line_quadrics_qem_copy_runtime_dependencies(my_app)
```

配置你的工程时指定安装包位置：

```powershell
cmake -S . -B build -Dline_quadrics_qem_DIR=C:\opt\manumesh\lib\cmake\line_quadrics_qem
```

### 推荐：传统 `.vcxproj` 工程

在 Visual Studio 中打开你的项目，进入：

```text
View -> Other Windows -> Property Manager
```

右键项目配置，选择 `Add Existing Property Sheet...`，添加：

```text
C:\opt\manumesh\share\line_quadrics_qem\msvc\line_quadrics_qem.props
```

这个 `.props` 会自动配置：

- `include\` 头文件目录；
- `lib\line_quadrics_qem.lib` 链接库；
- 构建后把 `bin\line_quadrics_qem.dll` 复制到你的程序输出目录。

如果使用 C++ API，例如 `line_quadrics_qem/core/Mesh.h`，属性表默认会引用
SDK 自带的 Eigen 头文件目录。需要统一公司内部 Eigen 版本时，可以覆盖
`LQEigenIncludeDir`。如果只使用 `line_quadrics_qem/api/CApi.h` 这套 C ABI，
调用方不需要包含 Eigen 头。

最小 C++ 调用：

```cpp
#include "line_quadrics_qem/core/Mesh.h"
#include "line_quadrics_qem/algorithms/simplification/QEMSimplifier.h"

int main() {
  lq::Mesh input;
  std::string error;
  if (!lq::loadMesh("input.stl", input, &error)) {
    return 1;
  }

  lq::SimplifyOptions options;
  options.targetRatio = 0.25;
  options.useLineQuadrics = true;

  lq::SimplifyReport report;
  lq::QEMSimplifier simplifier(options);
  lq::Mesh output = simplifier.simplify(input, &report);
  return output.empty() ? 1 : 0;
}
```

最小 C ABI 调用入口：

```c
#include "line_quadrics_qem/api/CApi.h"
```

## 验证闭环

基础回归：

```powershell
cmake -E chdir build/industrial ctest -LE performance --output-on-failure
```

大模型性能测试：

```powershell
cmake -E chdir build/industrial ctest -L performance --output-on-failure
```

快速生成 STL/CSV：

```powershell
.\build\mingw-ninja-release\bin\linequadrics.exe demo --quick --samples 500
```

工业特征验证：

```powershell
.\build\mingw-ninja-release\bin\linequadrics.exe validate-features --ratio 0.20 --samples 1000
```

该命令默认复制四个外部成品 STL 作为验证输入：
Thingi10K spindle、NASA antenna azimuth track、Thingi10K mini pulley 和
OpenFOAM flange。可用 `--spindle-input`、`--ring-input`、`--pulley-input`、
`--flange-input` 替换为下游自己的成品模型。

外部 OBJ 基准验证：

```powershell
.\build\mingw-ninja-release\bin\linequadrics.exe validate-external --ratio 0.25 --samples 800
```

每项能力、性能指标、输出文件和验收口径见
[`docs/design/industrial_validation.md`](docs/design/industrial_validation.md)。

## VS Code 演示入口

`.vscode/tasks.json` 只保留 MinGW+Ninja 和 MSVC+Ninja 两条链路。现场演示优先使用
`Terminal > Run Task...`：

- `demo: feature report selected mesh`：查看输入网格的 feature edge、loop、circle/ellipse 识别。
- `demo: simplify selected mesh (mingw+ninja release)`：从下拉框选择 mesh、preset、ratio、sample count。
- `demo: algorithm comparison selected mesh`：同一 mesh/ratio 下比较 `standard-qem`、`line-qem`、`dihedral-line`、`feature-curves`、`normal-tensor`。
- `demo: ratio sweep selected mesh`：固定算法预设，生成多档 ratio 的 STL 和 `metrics.csv`。
- `open: vscode demo output`：打开 `output/vscode_demo`。

最有说明力的算法选型案例：

| 案例 | 推荐 preset | 说明 |
| --- | --- | --- |
| `tests/data/feature_fixtures/coaxial_hole_plate.obj` | `feature-curves` | 4 个圆 loop 清楚，适合讲曲线预算、圆投影和 `projected_feature_placements`。 |
| `tests/data/feature_fixtures/elliptical_hole_plate.obj` | `feature-curves` | 适合观察圆/椭圆拟合、过度保护和 curve drift 之间的取舍。 |
| `tests/data/external/fandisk_2014.stl` | `dihedral-line` | 硬边明显，适合比较普通 QEM 和 line/dihedral 权重。 |
| `tests/data/external/casting_aimshape_2014.stl` | `industrial-safe` | 工业特征密集，适合展示质量 guard、局部误差 guard 和拒绝计数。 |
| `tests/data/feature_fixtures/coaxial_hole_plate.obj` | `feature-report` | 管件/孔特征直观，适合作为讲解 feature report 的起点。 |

最有说明力的参数选型案例：

| 参数 | 建议案例 | 观察点 |
| --- | --- | --- |
| `--feature-angle-deg 15/25/45` | `fandisk_2014.stl` | 二面角阈值如何改变硬边候选数量。 |
| `--max-feature-curve-deviation-ratio 0.02/0.05` | `coaxial_hole_plate.obj` | 曲线预算如何限制圆 loop 漂移。 |
| `--max-local-error-ratio 0.01/0.02` | `casting_aimshape_2014.stl` | 局部几何误差 guard 如何增加 `error_rejected_collapses`。 |
| `--weight-mode normal-tensor` | `boss_pocket_plate.obj` | normal tensor 对弱特征的补充，以及对噪声/采样的敏感性。 |

## 主要指标

常用 CSV 列：

| 指标 | 含义 |
| --- | --- |
| `faces`、`vertices` | 简化规模是否达到目标。 |
| `mean_triangle_quality`、`min_triangle_quality` | 三角形质量，越高越好。 |
| `edge_length_cv` | 边长变异系数，越低表示采样越均匀。 |
| `mean_orig_to_simp`、`max_orig_to_simp` | 采样距离误差，越低表示几何偏差越小。 |
| `non_manifold_edges` | 非流形风险提示。 |
| `radial_rms`、`plane_rms` | 圆形特征半径漂移和平面漂移。 |
| `feature_rejected_collapses` | 特征约束实际拦截了多少坍缩。 |
| `solver_fallbacks` | 当前候选 placement 求解退化到端点/中点候选集的次数，不含队列入队预排序。 |

STL 目检重点：

- 外形是否保持目标特征，例如孔、法兰边、轴肩、槽形轮廓。
- 是否出现明显破洞、自交、翻面或异常细长三角形。
- line/curve constrained 输出是否优于普通 QEM，而不是只看单一数值。

## 已知边界

- ManuMesh 当前实现是教学/研究级 QEM 内核向工业几何内核形态收拢，不是完整 CAD/B-Rep 内核。
- STL/OBJ 输入没有 B-Rep 语义；圆孔、硬边和弱特征只能从三角网格推断。
- line quadrics 改善平面区域的切向漂移和采样均匀性，但不是去噪器。
- 曲线特征保护仍依赖当前 feature graph 检测质量；复杂 CAD 图需要更强的多环追踪。

更详细的库结构见 [`docs/design/industrial_library.md`](docs/design/industrial_library.md)，特征约束方案见
[`docs/design/feature_curve_constraints.md`](docs/design/feature_curve_constraints.md)。
