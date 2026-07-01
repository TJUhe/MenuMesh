# Line Quadrics QEM

这是一个面向工业库形态整理的 C++17 网格简化内核。核心目标是复现并扩展
`Controlling Quadric Error Simplification with Line Quadrics` 的 QEM + line
quadrics 思路，并提供可构建、可测试、可度量、可由外部程序调用的库接口。

当前仓库刻意不再把网页预览作为主入口。结果检查走更稳定的工业验证路径：

```text
CLI 生成 STL/CSV -> CTest/API 示例验证 -> 用 MeshLab/CAD Assistant/系统 STL 查看器目检 STL
```

## 说明网页

这些 HTML 是论文说明和当前程序结果的可浏览笔记，可以直接用浏览器打开：

- [QEM 与 Line Quadrics 说明](docs/qem-line-quadrics-notes.html)
- [Line Quadrics 数学原理展开](docs/line-quadrics-qem-theory-explained.html)
- [当前程序原理说明](docs/line-quadrics-qem-program-principles.html)
- [代码阅读手册](docs/line-quadrics-qem-code-manual.html)
- [Xu 2024 CWF 弱特征合并论文说明](docs/cwf-weak-features-notes.html)
- [凸台圆孔等圆特征实践结果](docs/circular-feature-practice-results.html)

## 工业内核边界

核心交付物：

| 路径 | 角色 |
| --- | --- |
| `include/line_quadrics_qem/` | 公共 C++ API，外部应用只依赖这里的头文件。 |
| `src/` | 算法实现、STL/OBJ I/O、指标计算和 `linequadrics` CLI。 |
| `tests/` | GoogleTest/CTest 回归测试。 |
| `thirdParty/googletest/` | 随仓库携带的 GoogleTest 源码，用于离线构建测试。 |
| `examples/basic_simplify.cpp` | 模拟外部客户程序调用动态库。 |
| `cmake/` | `find_package(line_quadrics_qem CONFIG REQUIRED)` 安装包配置。 |
| `docs/industrial_library.md` | 动态库、安装、运行时布局和集成边界。 |
| `docs/industrial_validation.md` | 每项能力/性能的验证命令、CSV 指标和通过标准。 |

可生成或可选内容：

| 路径 | 说明 |
| --- | --- |
| `build*/` | CMake 构建目录，生成物。 |
| `examples/input/*.stl` | 可复现实验输入；可由 CLI 重新生成。 |
| `examples/output/` | STL/CSV 验证输出，属于实验结果。 |
| `examples/external/` | 外部 OBJ 基准模型本地放置目录，不作为库交付物。 |
| `docs/*.html`、`docs/api/` | 生成文档，不作为源码入口。 |

网页 viewer、Node/Vite 配置已经从源码入口中筛掉。要看形状时，直接打开 CLI
生成的 STL；要看性能和误差时，读取 CSV 指标。

## 构建

推荐 MinGW + Ninja：

```powershell
cmake -S . -B build/mingw-ninja-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build/mingw-ninja-release --target linequadrics --parallel 2
```

Windows 7 / 小内存机器优先用 MinGW Makefiles：

```powershell
cmake -S . -B build/mingw-makefiles-release -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build/mingw-makefiles-release --target linequadrics --parallel 1
```

完整工业库构建、测试、文档目标：

```powershell
cmake -S . -B build/industrial -DCMAKE_BUILD_TYPE=Release
cmake --build build/industrial --parallel
cmake -E chdir build/industrial ctest --output-on-failure
cmake --build build/industrial --target docs-api
```

## 核心 API

最小外部调用入口：

```cpp
#include "line_quadrics_qem/Mesh.h"
#include "line_quadrics_qem/QEMSimplifier.h"

lq::SimplifyOptions options;
options.targetRatio = 0.2;
options.useLineQuadrics = true;
options.lineWeight = 1e-3;

lq::SimplifyReport report;
lq::Mesh simplified = lq::simplifyMesh(input, options, &report);
```

安装后外部 CMake 工程可使用：

```cmake
find_package(line_quadrics_qem CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE line_quadrics_qem::line_quadrics_qem)
line_quadrics_qem_copy_runtime_dependencies(my_app)
```

## 验证闭环

基础回归：

```powershell
cmake -E chdir build/industrial ctest --output-on-failure
```

快速生成 STL/CSV：

```powershell
.\build\mingw-ninja-release\bin\linequadrics.exe demo --quick --samples 500
```

工业特征验证：

```powershell
.\build\mingw-ninja-release\bin\linequadrics.exe validate-features --ratio 0.20 --n 96 --samples 1000
```

外部 OBJ 基准验证：

```powershell
.\build\mingw-ninja-release\bin\linequadrics.exe validate-external --ratio 0.25 --samples 800
```

每项能力、性能指标、输出文件和验收口径见
[`docs/industrial_validation.md`](docs/industrial_validation.md)。

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

STL 目检重点：

- 外形是否保持目标特征，例如孔、法兰边、轴肩、槽形轮廓。
- 是否出现明显破洞、自交、翻面或异常细长三角形。
- line/curve constrained 输出是否优于普通 QEM，而不是只看单一数值。

## 已知边界

- 当前实现是教学/研究级 QEM 内核向工业库形态收拢，不是完整 CAD/B-Rep 内核。
- STL/OBJ 输入没有 B-Rep 语义；圆孔、硬边和弱特征只能从三角网格推断。
- line quadrics 改善平面区域的切向漂移和采样均匀性，但不是去噪器。
- 曲线特征保护仍依赖当前 feature graph 检测质量；复杂 CAD 图需要更强的多环追踪。

更详细的库结构见 [`docs/industrial_library.md`](docs/industrial_library.md)，特征约束方案见
[`docs/feature_curve_constraints.md`](docs/feature_curve_constraints.md)。
