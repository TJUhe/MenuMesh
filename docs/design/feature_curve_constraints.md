# QEM, Line Quadrics, and Curve Feature Constraints

本文解释当前曲线特征保护方案：为什么普通 QEM 和 normal-line quadrics 不足以
稳定保护圆孔、轴肩、圆环、椭圆孔和多段折痕，以及当前代码如何把曲线约束加入
边坍缩流程。

## 1. 基本直觉

传统 QEM 惩罚新点离开原始三角面平面：

```text
E_plane(x) = (n dot (x - p))^2
```

它主要约束法向误差。在大平面区域里，点沿切平面滑动时误差接近零，这会导致
标准 QEM 出现病态求解和不均匀采样。

line quadrics 增加的是点到原始顶点法向线的距离：

```text
E_line(x) = dist(x, normal_line)^2
```

这相当于补充切向正则化：点不要在切平面内随意漂移。

## 2. 曲线特征需要另一种自由度

圆孔、圆环和轴肩边界的合理自由度不是表面法向线，而是特征曲线切线。对这些点，
理想约束是：

```text
E_curve(x) = dist(x, tangent_line)^2
```

它惩罚两类错误：

- 半径方向漂移，导致圆孔变大、变小或变形。
- 离开特征平面，导致孔边界翘曲。

同时它允许点沿圆周或曲线切向移动，因为这是曲线简化时可接受的自由度。

## 3. 当前实现的三层保护

当前代码没有假装从 STL 中恢复完整 CAD 语义，而是采用可验证的三层保护：

```text
feature edge detection
feature graph + loop / primitive fitting
collapse legality + curve budget + projected placement
```

拓扑约束：

- 不允许 feature vertex 坍缩到 non-feature vertex。
- 不允许跨越不同 feature loop 坍缩。
- 不允许坍缩 feature junction。
- loop 顶点数低于 `--min-feature-loop-vertices` 或
  `--min-circular-feature-loop-vertices` 后停止继续缩减；如果同时设置了
  `--max-feature-curve-deviation-ratio`，圆/椭圆 loop 最低仍保留 4 个顶点，
  其他 polygonal loop 最低保留 3 个顶点。

几何约束：

- 对圆/近圆 feature loop 拟合 `center`、`radius`、`normal`。
- 对椭圆 feature loop 拟合中心、平面法向、长短轴和长短半径。
- 对非圆 polygonal loop 保存原始采样折线。
- 为 loop 顶点加入 `Q_feature_tangent_line`。
- 同一圆形 loop 内的坍缩结果会投影回拟合圆；椭圆 loop 投影回拟合椭圆；
  其他 polygonal loop 投影回原始折线最近段，退化时退回切线投影。
- 启用 `--max-feature-curve-deviation-ratio` 后，先检查原始 QEM placement
  离曲线是否已经超过 `ratio * bbox_diag`，超过则拒绝，而不是强行投影。

合法性约束：

- link-condition / duplicate-face / degenerate-face 检查防止明显拓扑破坏。
- `--max-normal-deviation-deg` 拒绝局部法向翻转或过大偏转。
- `--min-triangle-quality` 拒绝过差三角形。
- `--max-local-error` 或 `--max-local-error-ratio` 拒绝局部点位漂移过大。
- `--prevent-local-intersections` 使用局部空间索引拒绝局部三角形相交。

总误差项可以理解为：

```text
Q_total =
    Q_plane
  + w_line  * Q_normal_line
  + w_curve * Q_feature_tangent_line
```

## 4. 对应 CLI

检测特征：

```powershell
.\build\mingw-ninja-release\bin\linequadrics.exe feature-report input.stl --feature-angle-deg 25 --circle-fit-threshold 0.04 --min-feature-loop-vertices 8 --csv features.csv
```

启用曲线保护：

```powershell
.\build\mingw-ninja-release\bin\linequadrics.exe simplify input.stl output_curve.stl --method line --ratio 0.20 --line-weight 1e-3 --weight-mode dihedral --feature-boost 0.08 --feature-angle-deg 25 --preserve-feature-curves --feature-curve-weight 0.08 --circle-fit-threshold 0.04 --min-feature-loop-vertices 16
```

带曲线预算和圆环顶点预算的常用版本：

```powershell
.\build\mingw-ninja-release\bin\linequadrics.exe simplify input.stl output_curve.stl --method line --ratio 0.20 --line-weight 1e-3 --weight-mode dihedral --feature-boost 0.08 --feature-angle-deg 25 --preserve-feature-curves --feature-curve-weight 0.08 --max-feature-curve-deviation-ratio 0.05 --circle-fit-threshold 0.04 --ellipse-fit-threshold 0.05 --min-circular-feature-loop-vertices 12 --samples 1000 --metrics-csv metrics.csv
```

比较特征漂移：

```powershell
.\build\mingw-ninja-release\bin\linequadrics.exe feature-compare input.stl output_curve.stl --feature-angle-deg 25 --circle-fit-threshold 0.04 --csv compare.csv
```

批量验证：

```powershell
.\build\mingw-ninja-release\bin\linequadrics.exe validate-features --ratio 0.20 --n 96 --samples 1000
```

## 5. 验证指标

重点看 `feature-compare` 输出：

| 列 | 含义 |
| --- | --- |
| `status` | `matched`、`weak_match` 或 `missing`。 |
| `radial_rms`、`radial_max` | 半径漂移。 |
| `plane_rms`、`plane_max` | 离开拟合平面的漂移。 |
| `center_error` | 圆心漂移。 |
| `radius_error` | 半径参数漂移。 |
| `normal_angle_deg` | 圆平面法向偏转。 |

再打开对应 STL 目检：孔、轴肩、槽边是否仍可辨认，是否出现破洞、异常翻面或
局部坍塌。

同时看 `metrics.csv` 中的：

| 列 | 含义 |
| --- | --- |
| `feature_rejected_collapses` | feature policy 拦截的坍缩数。 |
| `curve_budget_rejected_collapses` | 曲线预算拦截的坍缩数。 |
| `projected_feature_placements` | placement 被投影回圆/椭圆/折线的次数。 |
| `normal_flip_rejected_collapses`、`quality_rejected_collapses`、`error_rejected_collapses` | legality filters 对输出稳定性的影响。 |

## 6. 工程边界

当前方案可以验证圆形、椭圆和 polygonal feature loop 保护是否比普通 line QEM
更稳，但它不是完整 B-Rep 特征识别系统。复杂法兰、相交孔、碎片化硬边和非圆曲线还需要：

- 更强的 feature graph 多环追踪和 loop ownership。
- primitive-aware boundary placement。
- 更严格的误差 envelope / Hausdorff filter。
- 上游 STEP/B-Rep 语义输入时，直接使用 CAD feature recognition，而不是从 STL 猜。
