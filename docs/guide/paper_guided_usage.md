# 论文视角下的当前用法

这份说明把 QEM 与 line quadrics 论文中的概念映射到 ManuMesh 当前 `manumesh.exe`、STL 输出和 CSV 指标。当前验证入口是 CLI、CTest、CSV 和外部 STL/CAD 查看器，不再依赖浏览器预览任务。更系统的数学解释见 [`../design/algorithm_essence.md`](../design/algorithm_essence.md)。

建议先在 PowerShell 中定义当前 VS Code task 一致的输出目录：

```powershell
$buildDir = "build/mingw-ninja-release"
$exe = "$buildDir/bin/manumesh.exe"
```

## 先看现象，再选参数

| 现象 | 数学/算法原因 | 优先检查 |
| --- | --- | --- |
| 大平面区域顶点分布漂移或不均匀 | 标准 QEM 在切平面方向缺约束，候选代价大量接近零。 | `--method line`、`--line-weight`、`edge_length_cv`、`solver_fallbacks`。 |
| 圆孔简化后变椭圆或顶点太少 | 特征 loop 没有形成硬保护，或最低 loop 顶点数太低。 | `--preserve-feature-curves`、`--feature-protection-mode primitive-curves`、`--min-circular-feature-loop-vertices`。 |
| 开边界被吃掉或合并 | boundary 只作为软成本，不足以阻止拓扑改变。 | `--preserve-boundary`，以及 `boundary_rejected_collapses`。 |
| 达不到目标面数，提前停止 | 目标比例和硬过滤器冲突，候选被大量拒绝。 | `termination_reason`、最高的 `*_rejected_collapses`。 |
| normal tensor 结果不稳定 | 张量特征受邻域、尺度、噪声和采样影响。 | `--normal-tensor-threshold`、`--normal-tensor-edge-alignment`、`--normal-tensor-scales`、`--normal-tensor-min-persistent-scales`、是否需要预处理。 |
| 光滑 fillet 中心线或平缓 ridge/valley 不出现在特征报告 | 二面角和 normal tensor 依赖离散法向差异，对光滑微分事件响应弱。 | 在 `feature-report` / `feature-benchmark` / `feature-compare` 中启用 `--smooth-curvature-features`，并检查 `smooth_curvature_edges`、`smooth_curvature_scored_vertices`。 |

这个阅读顺序比单纯调大某个权重更安全。QEM 和 line quadrics 是排序成本；feature graph 是曲线支撑；legality filters 才是硬安全闸。

## 先建立标准 QEM 对照

单模型标准 QEM：

```powershell
& $exe simplify input.stl output_standard.stl --method standard --ratio 0.25
```

line quadrics 对照：

```powershell
& $exe simplify input.stl output_line.stl --method line --ratio 0.25 --line-weight 1e-3 --metrics-csv metrics.csv
```

如果要比较不同简化率：

```powershell
& $exe ratio-sweep input.stl output_ratio_dir --method line --line-weight 1e-3 --ratios "0.8,0.5,0.25,0.1" --samples 512
```

如果要比较绝对目标面数：

```powershell
& $exe face-sweep input.stl output_face_dir --method line --line-weight 1e-3 --faces "2000,1000,500" --samples 512
```

## line quadrics 在当前程序中的含义

论文中的核心直觉是给 QEM 加入点到线的约束，减少欠约束区域的切向漂移。当前程序对应选项：

```powershell
--method line --line-weight 1e-3 --weight-mode uniform
```

`--line-weight` 建议从 `1e-3` 级别开始。权重过大会让均匀化压过几何保真，尤其在尖锐边、薄片、孔洞和高曲率区域要同时看 STL 外观、`edge_length_cv`、`min_triangle_quality` 和距离误差。

## 特征权重和特征保护

当前有三类特征相关能力：

- `--weight-mode dihedral`：用二面角硬边提高附近顶点 line weight，是软成本。
- `--weight-mode normal-tensor`：用带局部尺度和多尺度 persistence 的 normal tensor 给弱特征提供附加证据，是软成本。
- `--preserve-feature-curves`：启用特征环检测、曲线 quadric、placement 投影和硬保护策略。

特征分析命令另有一条 opt-in 的确定性光滑曲率证据路径（2026-07-11 落地，无任何学习成分）：

```powershell
& $exe feature-report input.stl `
  --smooth-curvature-features `
  --smooth-curvature-threshold 0.015 `
  --smooth-curvature-base-rings 2 `
  --smooth-curvature-scales 3 `
  --smooth-curvature-min-persistent-scales 2 `
  --csv features.csv
```

`--smooth-curvature-*` 选项族只被 `feature-report`、`feature-benchmark`、`feature-compare` 接受；`simplify` 会显式拒绝并提示它们属于 feature-analysis 选项（C++ 简化可消费预计算的 `FeatureAnalysis`）。分数尺度归一化，网格均匀缩放后不需要重调 `--smooth-curvature-threshold`。报告 stdout 与 CSV 会输出 `smooth_curvature_*` 系列诊断字段。另注意：本轮新增的纹理感知简化（`preserveTexture`）是 C++ `SimplifyOptions` 能力，CLI `simplify` 没有对应选项。

曲线特征保护的推荐起点：

```powershell
& $exe simplify input.stl output_curve.stl `
  --method line --ratio 0.25 --line-weight 1e-3 `
  --weight-mode dihedral --feature-boost 0.08 --feature-angle-deg 25 `
  --preserve-feature-curves --feature-protection-mode primitive-curves `
  --feature-curve-weight 0.08 --max-feature-curve-deviation-ratio 0.05 `
  --circle-fit-threshold 0.04 --ellipse-fit-threshold 0.05 `
  --min-circular-feature-loop-vertices 12 `
  --samples 512 --metrics-csv metrics.csv
```

默认 `primitive-curves` 只硬保护圆、近圆和椭圆等拟合 primitive；普通折线和 generic creases 主要由软成本影响。需要严格锁边时使用 `--feature-protection-mode all-feature-edges`。

## 边界和工业安全过滤

`--boundary-weight` 是 Garland-Heckbert 风格的 boundary plane quadric，影响 placement 成本；`--preserve-boundary` 是硬合法性策略，限制边界拓扑被破坏。二者不是一回事。

保守运行可以使用：

```powershell
--industrial-safe --preserve-boundary --prevent-local-intersections --max-local-error-ratio 0.02
```

这会增加拒绝折叠次数，但更适合带孔、薄壁、工业零件和需要稳定拓扑的输入。

## 推荐观察指标

不要只看一个数。合理结果通常同时满足：

```text
final_faces 接近目标
termination_reason 为 reached-target 或合理解释
mean_triangle_quality 上升或不显著下降
min_triangle_quality 没有严重退化
edge_length_cv 下降
non_manifold_edges 不新增
boundary_edges 不异常增加
feature_loops / circular_feature_loops 在特征保护场景下仍可识别
projected_feature_placements、curve_budget_rejected_collapses 与参数预期一致
```

line quadrics 和 normal tensor 都不是去噪器。扫描噪声应先做稳健法线估计、去噪或重建，再进入 ManuMesh 当前简化流程。

## 相关论文出处

| 程序概念 | 论文来源 |
| --- | --- |
| plane quadric / edge contraction cost | Garland-Heckbert 1997，`docs/papers/qem/garland_heckbert_1997_surface_simplification_qem.pdf` |
| point-to-line quadrics | Liu-Rahimzadeh-Zordan 2025，`docs/papers/line_quadrics/liu_rahimzadeh_zordan_2025_line_quadrics.pdf` |
| 纹理感知简化（`preserveTexture`，C++ API） | Garland-Heckbert 1998 属性 QEM（M003，`docs/papers/qem/garland_heckbert_1998_color_texture_qem.pdf`）为历史参照；当前实现按现代 edge-collapse 管线文献（M033）的工程拆分：4×4 几何 quadric 排序固定 3D placement，UV chart/面积合法性为显式局部策略，实现在 `src/simplification/TextureProtection.cpp`，设计见 `docs/design/texture_aware_qem.md` |
| smooth-curvature 特征证据（`--smooth-curvature-features`） | 2017–2025 确定性文献：Yamakawa-Shimada 2017/2018、Lu 2019（M044）、Romanengo 2020、Xu 2024 CWF（M026）、Cai 2025，索引见 `docs/papers/recent_deterministic_feature_detection_2026-07-11.md`；实现在 `src/feature_detection/SmoothCurvature.cpp`，设计见 `docs/design/smooth_curvature_feature_detection_2026_07_11.md` |
| edge-collapse 工程细节 | Hoppe 1996、Lindstrom-Turk 1998、Rose 2025，位于 `docs/papers/edge_collapse/` |
| CAD/STL 特征线 | Vidal-Wolf-Dupont 2011、Jiao-Bayyana 2008，位于 `docs/papers/feature_detection/` |
| normal tensor 特征评分 | Tsuchie-Higashi 2014，`docs/papers/feature_detection/tsuchie_higashi_2014_normal_tensor_surface_feature_lines.pdf` |
| 特征敏感简化 | Wang 2008、Hussain-Grahn-Persson 2008，位于 `docs/papers/feature_preserving_simplification/` |
