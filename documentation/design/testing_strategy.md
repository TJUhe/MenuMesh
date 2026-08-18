# 测试体系与策略

日期：2026-08-16（更新快速套件规模、架构检查门禁和构建目标依赖）

本文说明 ManuMesh 当前的测试分层、解析 fixture 的设计理念、确定性验证方式、
快速/全量套件的命令与规模，以及新增测试的注册方式。测试目录契约见
[`source_organization.md`](source_organization.md)；新增算法模块时的测试拆分
建议见 [`adding_new_algorithm.md`](adding_new_algorithm.md) 与
[`algorithm_extension_protocol.md`](algorithm_extension_protocol.md)。

## 分层

测试按运行成本与验证目标分五层，全部由 CTest 标签或独立构建目录区分：

| 层 | 位置 | 标签/目标 | 验证什么 |
| --- | --- | --- | --- |
| unit | `tests/unit/**`（`manumesh_tests`） | `unit` | 公共 API 契约、算法行为边界、回归保护。按模块分目录：`core/`、`io/`、`common/`、`mesh_edit/`、`api/`、`feature_detection/`、`simplification/`、`perf/`。 |
| analytic | `tests/unit/feature_detection/feature_detection_analytic_tests.cpp`（8 个）、`tests/unit/simplification/simplification_analytic_tests.cpp`（5 个） | `unit`（快速套件内） | 对解析真值 fixture 的定量断言：所有误差界由微分几何推导给出，而不是录制历史输出。 |
| perf-guard | `tests/unit/perf/pipeline_perf_guard_tests.cpp`（2 个） | `unit`（快速套件内） | 粗粒度墙钟护栏：在约 1.6 万面解析球上给 `detectFeatureCurves` 与 `simplify` 设绝对时间上限，专门拦截意外混入热循环的 O(n²) 路径。 |
| external | `tests/unit/**/*_external_tests.cpp` 等（`manumesh_external_tests`，11 个） | `external` | 依赖 `tests/data/external` 大网格或运行数十秒的用例，从快速套件剥离。 |
| performance | `tests/performance/qem_dataset_tests.cpp` | `performance`（需 `MANUMESH_BUILD_PERFORMANCE_TESTS=ON` 的独立构建） | 大模型数据集吞吐与质量指标。 |

此外有 `architecture` 标签的 2 个守卫测试（`include_boundaries` 与
`include_boundary_checker_tests`）：`tests/support/check_include_boundaries.py`
按集中定义的模块依赖表拒绝越界 include，属于快速套件的一部分。

VS2019 preset 和 CI 将 `MANUMESH_REQUIRE_ARCHITECTURE_CHECKS` 显式设为 `ON`；在这些
受支持的构建中找不到 Python 3 会使 CMake 配置失败。自定义构建默认保持 `OFF`，因此
没有 Python 时仍可运行其余测试。边界检查脚本要求调用方显式传入仓库根目录，并验证
`include/`、`src/`、`apps/` 与 `examples/` 均存在，避免错误工作目录或空扫描产生假通过。

## 解析真值 fixture（`tests/support/AnalyticFixtures.{h,cpp}`）

设计理念：**测试断言应当由被测几何的闭式解推导，而不是对着一次运行结果抄数**。
每个生成器都是纯函数（无随机设备、无时钟），返回网格的同时携带解析真值访问器：

- `makeUvSphere(rings, segments, radius)`：经纬球；`analyticPrincipalCurvatures()`
  返回 κ_max = κ_min = 1/r，另有极点/内部行判断辅助。
- `makeCylinder(segments, rings, radius, height, capped)`：直圆柱；侧壁解析主曲率
  （周向 1/r、轴向 0），capped 版本带精确 90° 折痕；`groundTruthCircles()` 给出两个
  rim 圆的中心/法向/半径，`groundTruthFeatureEdges()` 给出真值特征边集。
- `makeTorus(majorSegments, minorSegments, R, r)`：环面；逐顶点解析主曲率
  {1/r, cosθ/(R + r·cosθ)}，用于简化后的双向采样 Hausdorff 验证。
- `makeChamferBox(size, chamfer, divisions)`：倒角盒；`groundTruthHardEdges()`
  给出精确硬边集（24 条倒角折痕 + 两圈八边形 rim），16 个八边形角点是
  feature-graph valence 3 的真值 junction。
- `withDeterministicNoise(mesh, amplitude, seed)`：Knuth MMIX 线性同余发生器
  （乘子 6364136223846793005、增量 1442695040888963407，取高 53 位映射到
  [−1, 1]），逐坐标加有界扰动。同一 seed 在任何平台产生相同网格，噪声鲁棒性
  用例因此完全可复现。

### 推导断言界的示例

- 球简化后顶点离球面偏差 < 1% 半径：QEM 最优点落在切平面交点凸包内，
  单次 collapse 的弦高（sagitta）约 l²/(8r) ≈ 1.1e-3·r（l 为输出边长），
  1% 预算给级联 collapse 留约 9 倍余量（`SphereVerticesStayOnSphereAndTopologyIsPreserved`）。
- 环面双向采样 Hausdorff < 2% 小半径：最紧弯曲方向是小圆，弦高界
  l²/(8·r_minor) ≈ 2.5e-3，预算留约 2.4 倍余量（`TorusStaysWithinSampledHausdorffBudget`）。
- 柱面 rim 圆保真 1e-6·r：`PrimitiveCurves` 保护把 placement 投影到检测期拟合圆上，
  输入 rim 多边形精确落在解析圆上，因此该界是双精度舍入容差而非几何允差
  （`PrimitiveProtectionKeepsCylinderRimsExactlyCircular`）。
- 带 UV 圆柱的纹理保真：每个存活角点的 UV 相对其 3D 位置的解析参数化
  （u = θ/2π, v = z/h + 1/2）偏差不得超过所在面最长边对应的参数增量，
  且 UV 三角形不得翻转（`TexturedCylinderKeepsUvAlignedWithAnalyticParametrization`）。
- 解析曲面 fixture 的几何量和采样密度必须固定，避免把采样变化误判为算法改进；
  同时保留足够的 Hausdorff/primitive residual 余量来暴露回归。

## 确定性测试

- `SimplificationAnalytic.DetectAndSimplifyPipelineIsByteStableAcrossRuns`：
  detect + 特征保护 simplify 在同一 fixture 上重复三次，输出必须逐字节一致——
  无序容器遍历、浮点归约和优先队列 tie-break 都被设计要求确定性。
- `FeatureDetectionAnalytic.FeatureEdgeSetIsExactlyScaleInvariant`：网格均匀缩放后
  特征边集必须精确不变。
- 所有噪声用例经 `withDeterministicNoise` 固定 seed，不存在"偶发红"的随机源。

## 套件命令与预期规模

当前支持基线使用 Visual Studio 16 2019 / MSVC v142 presets。下列用例数量来自
2026-08-16 的 Debug 快速套件注册结果；耗时受配置、机器和数据缓存影响，不作为性能承诺：

```
# 快速套件：341 个启用用例（另有 1 个 DISABLED 计时用例）
ctest --preset vs2019-release-unit

# 全量非性能套件：352 个启用用例（快速套件 + 11 个 external 大网格用例）
ctest --preset vs2019-release-full

# 仅 external 大网格用例（需要 tests/data/external 数据）
ctest --preset vs2019-release-external

# performance 套件：独立构建目录 + -DMANUMESH_BUILD_PERFORMANCE_TESTS=ON
ctest --preset vs2019-release-performance
```

打开 `MANUMESH_ENABLE_INSTALL=ON` 且安装 CMake package config 时，会额外注册一个
`sdk_consumer_examples` 聚合测试，在独立消费工程中验证安装后的 C 与 C++ SDK。

对应的构建目标：`unit-tests`（等价于快速套件）、`external-tests`、
`performance-tests`（仅性能构建）。`unit-tests` 只依赖快速套件需要的测试程序、CLI
与三个示例，不再先构建 `manumesh_external_tests`；外部大网格程序由
`external-tests` 单独构建和运行。

## Visual Studio 2019 / v142 支持门禁

`.github/workflows/msvc-v142.yml` 使用显式 CMake 命令，本地验证使用等价的
`vs2019-*` presets。两条路径都由 `Visual Studio 16 2019` generator、x64 和
`v142` toolset 生成；Debug 运行快速套件，Release 额外覆盖 external 和安装后
C/C++ SDK consumer。当前 CI 不构建性能测试、静态库矩阵或 Doxygen 文档。依赖固定
使用仓库内 Eigen 和源码 GoogleTest，确保库、测试与消费程序都由 v142 编译。

`vs2019-ninja-*` presets 只是同一 VS2019 v142 Developer PowerShell 下的可选
Ninja Multi-Config 前端，不是另一套编译器支持面。两种生成器不得复用同一构建目录。

性能护栏的上限设计（`tests/unit/perf/pipeline_perf_guard_tests.cpp` 注释中有
机器基准）：上限取实测值的 ≥3 倍，慢一些的 CI 机器仍能通过；而这一规模下的
O(n²) 回归会超过 10 倍，必然被拦截。历史依据：`SimplificationRun::tryCollapse`
曾在每次 collapse 尝试里重算输入包围盒对角线（O(V) 扫描），16k 面球的 simplify
因此从约 2 秒劣化到约 15 秒——正是该护栏针对的缺陷类别；现在对角线在
`initializeBudget` 缓存为 `meshDiagonal_` 一次性计算。

2026-07-15 新增的
`tests/unit/feature_detection/feature_detection_pipeline_upgrade_tests.cpp`
专门保护法线域过滤、兼容 component consolidation、junction branch pairing、三 patch 分区和新 options
校验。benchmark 标签格式同时覆盖 `edge`、`junction`、`branch`、`face_patch`；
patch 指标比较相邻 faces 的同/异 patch 关系，避免依赖任意 patch id 编号。

## 测试组织约定

- 通用 core/topology/io 用例位于 `tests/unit/core/core_tests.cpp` 与
  `tests/unit/io/mesh_io_tests.cpp`（2026-07 从 `simplification_core_tests.cpp`
  迁出；后者只保留 simplification 入口相关用例）。
- 共享测试辅助放 `tests/support/`（`TestSupport`、`AnalyticFixtures`），
  模块内共享辅助放各自目录（如 `FeatureDetectionTestSupport`）。
- 测试名按行为命名（`FeatureDetectionAnalytic.CappedCylinderRimsRecoverAsTwoExactCircles`），
  不按实现类命名。

## 新增测试的注册方式

1. 新 `TEST()` 加入既有已注册源文件时**不需要改任何 CMake**：
   `gtest_discover_tests(... DISCOVERY_MODE PRE_TEST)` 会在运行前自动发现。
2. 新增测试源文件时，把它登记进 `tests/CMakeLists.txt` 的
   `MANUMESH_UNIT_TEST_SOURCES`（快速套件）；依赖
   `tests/data/external` 数据或单用例运行超过约 30 秒的文件登记进
   `MANUMESH_EXTERNAL_TEST_SOURCES`（`external` 标签），保持快速套件秒级。
3. 新增源码模块的测试目录（如 `tests/unit/repair/`）无需单独 target；
   但源码模块本身要先在 `tests/support/check_include_boundaries.py` 的
   `MODULE_DEPENDENCIES` 登记允许依赖，否则 `include_boundaries` 直接拒绝。
4. 性能用例进 `tests/performance/`，由 `MANUMESH_BUILD_PERFORMANCE_TESTS`
   开关的独立可执行文件承载，标签 `performance` 且 `RUN_SERIAL`。
