# Industrial Validation Matrix

本文档把当前方案拆成可执行的验证项。验证目标不是“网页能展示”，而是证明库、
CLI、STL 输出和 CSV 指标可以覆盖算法性能、工程集成和特征保持能力。

## 0. 验证前提

推荐从干净构建目录开始：

```powershell
cmake -S . -B build/industrial -DCMAKE_BUILD_TYPE=Release
cmake --build build/industrial --parallel
```

如果需要指定 MinGW + Ninja 环境：

```powershell
cmake -S . -B build/mingw-ninja-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build/mingw-ninja-release --target linequadrics --parallel
```

`CMAKE_C_COMPILER=gcc` 和 `CMAKE_CXX_COMPILER=g++` 要成对指定。只指定
`CMAKE_CXX_COMPILER=g++` 时，某些 Windows 终端会把 C 编译器选成 MSVC
`cl.exe`，如果当前 shell 没有 `rc.exe` / `mt.exe`，配置会失败。

以下命令中的可执行文件按实际构建目录替换：

```text
build/industrial/bin/linequadrics.exe
build/mingw-ninja-release/bin/linequadrics.exe
```

Use the `bin/` path for fresh CMake builds. A root-level executable may exist in
old build trees, but it should not be used as the documented validation path.

## 1. 验证矩阵

| 验证项 | 命令 | 输出 | 通过标准 |
| --- | --- | --- | --- |
| 库可构建 | `cmake --build build/industrial --parallel` | `line_quadrics_qem` 动态库、`linequadrics` CLI | 构建无编译/链接错误。 |
| API 可被外部程序调用 | `cmake -E chdir build/industrial ctest --output-on-failure` | `linequadrics_example_basic_simplify` | 示例程序退出码为 0，输出面数小于输入面数。 |
| 单元回归 | `cmake -E chdir build/industrial ctest --output-on-failure` | `line_quadrics_qem_tests` | QEM 简化、特征检测、网格统计测试全部通过。 |
| CLI 可用性 | `linequadrics --help` | 帮助文本 | 命令列出 `simplify`、`compare`、`feature-report`、`feature-compare`、`validate-features`。 |
| 基础简化性能 | `linequadrics demo --quick --samples 500` | `output/**/metrics.csv`、STL | 目标面数下降；`metrics.csv` 有质量和距离列；STL 可正常打开。 |
| 同面数质量对比 | `linequadrics sweep input.stl out_dir --ratio 0.15 --weights "0,1e-4,1e-3"` | `out_dir/metrics.csv` | 同一目标面数下比较 `mean_triangle_quality`、`edge_length_cv`、距离误差。 |
| 不同简化率稳定性 | `linequadrics ratio-sweep input.stl out_dir --method line --line-weight 1e-3 --ratios "0.8,0.5,0.25,0.1"` | `out_dir/metrics.csv`、多档 STL | `faces` 随 ratio 下降；STL 在各档可打开。 |
| 圆/曲线特征检测 | `linequadrics feature-report input.stl --csv features.csv` | `features.csv` | 记录 feature edge、loop、circle fit 结果。 |
| 曲线特征保持 | `linequadrics validate-features --ratio 0.20 --samples 1000` | `tests/output/feature_curve_validation/*`，复制后的外部成品输入在 `tests/output/generated_inputs/*` | 对 Thingi10K spindle、NASA antenna azimuth track、Thingi10K mini pulley 和 OpenFOAM flange 输出 line/curve 两套 STL、metrics CSV 和 feature-compare CSV；可用 `--spindle-input`、`--ring-input`、`--pulley-input`、`--flange-input` 替换为下游模型。 |
| 特征误差对比 | `linequadrics feature-compare original.stl simplified.stl --csv compare.csv` | `compare.csv` | 查看 `status`、`radial_rms`、`plane_rms`、`radius_error`，曲线约束结果应减少关键圆特征漂移。 |
| 外部模型鲁棒性 | `linequadrics validate-external --ratio 0.25 --samples 800` | `tests/output/external_model_validation/external_summary.csv` | 默认读取 `tests/data/external/common_3d_test_models/` 中的 `fandisk.obj`、`rocker_arm.obj`、`beetle.obj`、`cow.obj`、`suzanne.obj`。至少找到一个模型才通过；一个都找不到时命令返回错误并说明缺失目录。 |
| STL 可视确认 | 打开输出 STL | STL 查看器画面 | 无明显破洞、翻面、异常坍塌；特征环仍可辨认。 |
| API 文档 | `cmake --build build/industrial --target docs-api` | `build/industrial/docs/api/html/index.html` | Doxygen 可生成；缺少 Doxygen 时目标给出提示而非破坏构建。 |

## 2. 性能指标如何判读

几何规模：

- `faces` 应接近目标 `ratio` 或目标面数。
- `vertices` 应随简化下降。

三角形质量：

- `mean_triangle_quality` 越高越接近均匀三角形。
- `min_triangle_quality` 很低时，优先打开 STL 目检是否有退化细长面。
- `edge_length_cv` 越低，边长分布越均匀。

几何误差：

- `mean_orig_to_simp` 和 `mean_simp_to_orig` 用于估计双向平均距离。
- `max_orig_to_simp` 和 `max_simp_to_orig` 用于发现局部大偏差。
- line quadrics 通常改善采样均匀性，但权重过大可能增加几何误差。

拓扑风险：

- `boundary_edges` 用于判断边界是否意外变化。
- `non_manifold_edges` 非零时要打开 STL 复核；当前算法有 link-condition、
  normal-deviation、triangle-quality、local-error 和可选 local self-intersection
  guards，但仍不是完整 CAD/B-Rep 拓扑证明。

曲线特征：

- `matched` / `weak_match` / `missing` 表示原始特征是否在简化结果中被重新识别。
- `radial_rms` 和 `radial_max` 衡量半径漂移。
- `plane_rms` 和 `plane_max` 衡量孔/环是否离开拟合平面。
- `center_error`、`radius_error`、`normal_angle_deg` 衡量解析圆参数漂移。

拒绝计数：

- `feature_rejected_collapses`：feature vertex 与非 feature vertex、跨 loop、
  junction 或低于 loop 顶点预算的坍缩被拒绝。
- `boundary_rejected_collapses`：`--preserve-boundary` 下非法边界坍缩被拒绝。
- `topology_rejected_collapses`、`normal_flip_rejected_collapses`、
  `quality_rejected_collapses`、`self_intersection_rejected_collapses`、
  `error_rejected_collapses` 分别对应 collapse legality filters。
- `curve_budget_rejected_collapses`：启用
  `--max-feature-curve-deviation-ratio` 后，原始 placement 离特征曲线太远。
- `projected_feature_placements`：同一特征 loop 内的 placement 被投影回圆、
  椭圆或原始 polyline。

## 3. 建议的验收顺序

1. 先跑 CMake 构建和 CTest，证明库、CLI、示例和单元测试闭环成立。
2. 跑 `demo --quick`，证明基础 STL/CSV 输出可复现。
3. 跑 `validate-features`，证明外部工业风格圆/轴肩/槽特征可被度量。
4. 打开关键 STL：`external_spindle_line.stl`、`external_ring_track_line.stl`、
   `external_pulley_line.stl`、`external_flange_line.stl`，并对照各自的
   `*_curve.stl`。
5. 对照 feature compare CSV，检查曲线约束是否降低关键圆环的半径漂移和平面漂移。
6. 跑 `validate-external`，用于发现过保护、误检和复杂特征图问题；若使用
   自己的 OBJ 集，传入 `--input-dir your_dir`。

新增外部模型探针和第一阶段策略落地结果记录在
[`feature_protection_roadmap.md`](feature_protection_roadmap.md)。当前
`--preserve-feature-curves` 默认使用 `--feature-protection-mode primitive-curves`：
只对 circle / near-circle / ellipse 做硬保护，generic polygonal / dihedral crease
默认作为软代价和 normal/quality/local-error filter 的输入。旧的强保护行为仍可用
`--feature-protection-mode all-feature-edges` 或 `--protect-all-feature-edges` 重现。

这次验证中，Mars wheel 从 `all-feature-edges` 的 10974 faces / `rejection-limit` /
468702 feature rejections 改善为 `primitive-curves` 的 9066 faces /
`reached-target` / 31 feature rejections；differential gear 从 2662 faces /
`rejection-limit` / 68993 feature rejections 改善为 1236 faces /
`reached-target` / 0 feature rejections。Fandisk 两种模式都能达到目标，但 generic
feature rejections 从 513 降为 0。这说明算法提升已经生效：generic crease 不再把
简化队列硬锁死。

## 4. 当前方案能验证什么

可以验证：

- 动态库和 CLI 能构建、运行、安装并被外部 CMake 工程链接。
- QEM/line quadrics 的同面数质量差异。
- 简化率变化下的面数、质量和距离指标。
- 圆形 CAD/STL 特征的检测、保护和误差对比。
- 输出 STL 是否能被通用 STL 工具打开并目检。

不能完全证明：

- 任意工业 STL 都能保持所有语义特征。
- 输出严格无自交、严格流形或满足 CAD/B-Rep 级拓扑约束。
- line quadrics 可以替代去噪、重网格化或 B-Rep 特征识别。

这些边界也是下一阶段工业化路线的输入：更强的 editable topology / 半边结构、
严格误差 envelope、primitive-aware 边界/特征曲线、复杂多环 feature graph
ownership，以及持续扩展 C ABI。
