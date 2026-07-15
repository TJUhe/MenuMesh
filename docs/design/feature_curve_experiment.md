# 特征曲线实验记录

本文记录当前可复现实验入口。目标是验证特征曲线保护是否在目标面数、三角质量和圆/椭圆 loop 保持之间取得平衡。

特征曲线保护的算法背景见 [`algorithm_essence.md`](algorithm_essence.md) 和 [`feature_curve_constraints.md`](feature_curve_constraints.md)。实验应同时观察软成本、投影、曲线预算和硬拒绝，而不是只看输出面数。

## 推荐 fixture

| 输入 | 关注点 |
| --- | --- |
| `tests/data/feature_fixtures/coaxial_hole_plate.obj` | 多个圆孔 loop 和同轴关系。 |
| `tests/data/feature_fixtures/tilted_coaxial_hole_plate.obj` | 倾斜圆孔轴线。 |
| `tests/data/feature_fixtures/elliptical_hole_plate.obj` | 椭圆和 near-circle 分类。 |
| `tests/data/feature_fixtures/boss_pocket_plate.obj` | boss/pocket 硬边和平面结构。 |
| `tests/data/external/fandisk_2014.stl` | 大量硬边和 generic crease。 |

## 特征报告

```powershell
$exe = "build/mingw-ninja-release/bin/manumesh.exe"
& $exe feature-report `
  tests/data/feature_fixtures/coaxial_hole_plate.obj `
  --feature-angle-deg 25 `
  --circle-fit-threshold 0.04 `
  --ellipse-fit-threshold 0.05 `
  --csv output/vscode_demo/features.csv
```

## 简化实验

```powershell
$exe = "build/mingw-ninja-release/bin/manumesh.exe"
& $exe simplify `
  tests/data/feature_fixtures/coaxial_hole_plate.obj `
  output/vscode_demo/coaxial_feature_curves.stl `
  --method line --ratio 0.25 --line-weight 1e-3 `
  --weight-mode dihedral --feature-boost 0.08 --feature-angle-deg 25 `
  --preserve-feature-curves --feature-protection-mode primitive-curves `
  --feature-curve-weight 0.08 --max-feature-curve-deviation-ratio 0.05 `
  --circle-fit-threshold 0.04 --ellipse-fit-threshold 0.05 `
  --min-circular-feature-loop-vertices 12 `
  --samples 512 --metrics-csv output/vscode_demo/coaxial_feature_curves_metrics.csv
```

## 噪声、consolidation 与 patch 实验

```powershell
& $exe feature-report input_noisy.stl `
  --feature-normal-filter `
  --smooth-curvature-features --smooth-curvature-stable-scale `
  --feature-graph-consolidation `
  --surface-patches `
  --csv output/vscode_demo/noisy_feature_patches.csv
```

这组开关均为 opt-in。实验要分别比较开关前后 normal angular change、smooth scale stability、consolidation bridge、junction branch pair、surface patch/adjacency 和 ignored recovery edge；不能只比较总 feature edge 数。patch leakage 可先用 `--surface-patches-strong-only` 排除 weak barrier，再判断问题来自弱 evidence 还是硬边漏检。

## 判断标准

- `termination_reason` 优先希望为 `reached-target`。
- `feature_loops`、`circular_feature_loops` 不应异常消失。
- `projected_feature_placements` 应说明投影策略确实参与。
- `curve_budget_rejected_collapses` 过高说明曲线预算太紧。
- `generic_feature_rejected_collapses` 过高通常说明不应使用 `all-feature-edges`。
- STL 视觉检查应重点看孔边是否变成明显多边形、椭圆是否被拉圆、薄边是否翻折。
- 启用 normal filter 时，圆柱 rim 等真实大角度边必须保持；changed faces 增加本身不是成功指标。
- 启用 consolidation 时，closure 应改善且 branch-pair precision 不应明显下降。
- 启用 patches 时，比较 patch adjacency，而不是依赖 patch id 的具体编号。

这些字段对应不同算法层：`feature_loops` 来自 feature graph，`projected_feature_placements` 来自 placement 投影，`curve_budget_rejected_collapses` 来自投影前预算，`generic_feature_rejected_collapses` 来自硬保护策略。若某项异常，优先回到对应层调参。

## 当前结论

默认 `primitive-curves` 比旧式 `all-feature-edges` 更适合 ManuMesh 当前实现：它保留圆/椭圆等工业 primitive，同时避免把普通硬边全部锁死导致过早 `rejection-limit`。对 Fandisk 这类 generic crease 很多的模型，应优先依赖软成本和质量过滤，而不是全部硬保护。
