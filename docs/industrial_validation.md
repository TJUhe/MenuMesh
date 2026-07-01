# Industrial Validation Matrix

本文档把当前方案拆成可执行的验证项。验证目标不是“网页能展示”，而是证明库、
CLI、STL 输出和 CSV 指标可以覆盖算法性能、工程集成和特征保持能力。

## 0. 验证前提

推荐从干净构建目录开始：

```powershell
cmake -S . -B build/industrial -DCMAKE_BUILD_TYPE=Release
cmake --build build/industrial --parallel
```

如果使用 preset：

```powershell
cmake --preset mingw-ninja-release
cmake --build --preset mingw-ninja-release --target linequadrics --parallel 2
```

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
| API 可被外部程序调用 | `ctest --test-dir build/industrial --output-on-failure` | `linequadrics_example_basic_simplify` | 示例程序退出码为 0，输出面数小于输入面数。 |
| 单元回归 | `ctest --test-dir build/industrial --output-on-failure` | `line_quadrics_qem_tests` | QEM 简化、特征检测、网格统计测试全部通过。 |
| CLI 可用性 | `linequadrics --help` | 帮助文本 | 命令列出 `simplify`、`compare`、`feature-report`、`feature-compare`、`validate-features`。 |
| 基础简化性能 | `linequadrics demo --quick --samples 500` | `examples/output/**/metrics.csv`、STL | 目标面数下降；`metrics.csv` 有质量和距离列；STL 可正常打开。 |
| 同面数质量对比 | `linequadrics sweep input.stl out_dir --ratio 0.15 --weights "0,1e-4,1e-3"` | `out_dir/metrics.csv` | 同一目标面数下比较 `mean_triangle_quality`、`edge_length_cv`、距离误差。 |
| 不同简化率稳定性 | `linequadrics ratio-sweep input.stl out_dir --method line --line-weight 1e-3 --ratios "0.8,0.5,0.25,0.1"` | `out_dir/metrics.csv`、多档 STL | `faces` 随 ratio 下降；STL 在各档可打开。 |
| 圆/曲线特征检测 | `linequadrics feature-report input.stl --csv features.csv` | `features.csv` | 记录 feature edge、loop、circle fit 结果。 |
| 曲线特征保持 | `linequadrics validate-features --ratio 0.20 --n 96 --samples 1000` | `examples/output/feature_curve_validation/*` | 对 stepped shaft、pipe coupling、pulley、flange 输出 line/curve 两套 STL 和 compare CSV。 |
| 特征误差对比 | `linequadrics feature-compare original.stl simplified.stl --csv compare.csv` | `compare.csv` | 查看 `status`、`radial_rms`、`plane_rms`、`radius_error`，曲线约束结果应减少关键圆特征漂移。 |
| 外部模型鲁棒性 | `linequadrics validate-external --ratio 0.25 --samples 800` | `examples/output/external_model_validation/external_summary.csv` | 有模型则输出汇总；缺少模型时应给出明确跳过/缺失提示，不影响核心库构建。 |
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
- `non_manifold_edges` 非零时要打开 STL 复核；当前算法不是完整拓扑保护内核。

曲线特征：

- `matched` / `weak_match` / `missing` 表示原始特征是否在简化结果中被重新识别。
- `radial_rms` 和 `radial_max` 衡量半径漂移。
- `plane_rms` 和 `plane_max` 衡量孔/环是否离开拟合平面。
- `center_error`、`radius_error`、`normal_angle_deg` 衡量解析圆参数漂移。

## 3. 建议的验收顺序

1. 先跑 CMake 构建和 CTest，证明库、CLI、示例和单元测试闭环成立。
2. 跑 `demo --quick`，证明基础 STL/CSV 输出可复现。
3. 跑 `validate-features`，证明工业风格圆/轴肩/槽特征可被度量。
4. 打开关键 STL：`pipe_coupling_line.stl`、`pipe_coupling_curve.stl`、
   `pulley_line.stl`、`pulley_curve.stl`。
5. 对照 feature compare CSV，检查曲线约束是否降低关键圆环的半径漂移和平面漂移。
6. 如需真实模型，再放入外部 OBJ 并跑 `validate-external`，用于发现过保护、误检和复杂特征图问题。

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

这些边界也是下一阶段工业化路线的输入：半边拓扑、collapse legality filter、
自交检测、强边界/特征约束、多环 feature graph tracing、以及可选 C ABI。
