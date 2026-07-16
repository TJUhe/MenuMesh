# 外部模型验证说明

本文说明 ManuMesh 当前外部模型验证的范围、命令和结论。验证目标是确认算法在公开模型和工业风格 STL/OBJ 上不会引入明显拓扑退化，并能输出可比较的 CSV 指标。

外部验证不是追求某个单一误差数字，而是把 [`algorithm_essence.md`](algorithm_essence.md) 中的几类失效模式落到真实数据上：平面区排序退化、特征 loop 漂移、边界改变、非流形风险、三角形质量退化和局部误差/自交过滤冲突。

## 数据位置

| 路径 | 内容 |
| --- | --- |
| `tests/data/external/common_3d_test_models/` | beetle、cow、fandisk、rocker_arm、suzanne 等 OBJ。 |
| `tests/data/external/*.stl` | Fandisk、Casting、NASA、OpenFOAM 等外部 STL。 |
| `tests/data/external/large/` | 10 个较大公开二进制 STL。 |
| `tests/data/external/thingi10k/` | 97 个 Thingi10K 子集 STL。 |

## 当前 CLI

普通外部验证：

```powershell
$exe = "build/mingw-ninja-release/bin/manumesh.exe"
& $exe validate-external `
  --input-dir tests/data/external/common_3d_test_models `
  --ratio 0.25 `
  --samples 800 `
  --output-dir tests/output/external_model_validation
```

特征验证：

```powershell
$exe = "build/mingw-ninja-release/bin/manumesh.exe"
& $exe validate-features `
  --ratio 0.20 `
  --samples 1000 `
  --output-dir tests/output/feature_curve_validation
```

## VS Code 任务

当前可用任务是：

- `run: external validation`
- `run: feature validation`
- `test: mingw+ninja release`
- `test: mingw+ninja release performance`
- `test: mingw+ninja release full`（顺序运行 Release 回归、性能测试、SDK consumer 测试和 API 文档生成）

旧文档中提到的 `run: large validation 100 stl`、`open: large validation output` 等任务当前不在 `.vscode/tasks.json` 中，不能继续作为现有入口描述。

## 需要关注的指标

- `final_faces` 是否接近目标。
- `termination_reason` 是否为 `reached-target`，或是否有合理拒绝原因。
- `boundary_edges` 和 `non_manifold_edges` 是否异常增加。
- `mean_triangle_quality`、`min_triangle_quality` 和 `edge_length_cv` 是否退化。
- 特征保护场景下 `feature_loops`、`circular_feature_loops`、`projected_feature_placements` 是否符合预期。
- `solver_fallbacks` 和 `min_line_weight` / `max_line_weight` 是否提示 QEM 欠约束或 line weight 过强。

## 当前结论

ManuMesh 当前外部验证适合作为回归和演示，不等价于工业认证。若要进入更严格使用场景，需要补充误差 envelope、属性传播、全局自交检查、更多真实 CAD STL 和宿主应用集成测试。
