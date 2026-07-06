# 论文视角下的当前用法

这份说明把 QEM 与 line quadrics 论文中的概念映射到 ManuMesh 当前 `linequadrics.exe`、STL 输出和 CSV 指标。当前验证入口是 CLI、CTest、CSV 和外部 STL/CAD 查看器，不再依赖浏览器预览任务。

## 先建立标准 QEM 对照

单模型标准 QEM：

```powershell
.\build\mingw-ninja-release\bin\linequadrics.exe simplify input.stl output_standard.stl --method standard --ratio 0.25
```

line quadrics 对照：

```powershell
.\build\mingw-ninja-release\bin\linequadrics.exe simplify input.stl output_line.stl --method line --ratio 0.25 --line-weight 1e-3 --metrics-csv metrics.csv
```

如果要比较不同简化率：

```powershell
.\build\mingw-ninja-release\bin\linequadrics.exe ratio-sweep input.stl output_ratio_dir --method line --line-weight 1e-3 --ratios "0.8,0.5,0.25,0.1" --samples 512
```

如果要比较绝对目标面数：

```powershell
.\build\mingw-ninja-release\bin\linequadrics.exe face-sweep input.stl output_face_dir --method line --line-weight 1e-3 --faces "2000,1000,500" --samples 512
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
- `--weight-mode normal-tensor`：用 normal tensor 给弱特征提供附加证据，是软成本。
- `--preserve-feature-curves`：启用特征环检测、曲线 quadric、placement 投影和硬保护策略。

曲线特征保护的推荐起点：

```powershell
.\build\mingw-ninja-release\bin\linequadrics.exe simplify input.stl output_curve.stl `
  --method line --ratio 0.25 --line-weight 1e-3 `
  --weight-mode dihedral --feature-boost 0.08 --feature-angle-deg 25 `
  --preserve-feature-curves --feature-protection-mode primitive-curves `
  --feature-curve-weight 0.08 --max-feature-curve-deviation-ratio 0.05 `
  --circle-fit-threshold 0.04 --ellipse-fit-threshold 0.05 `
  --min-circular-feature-loop-vertices 12 `
  --samples 512 --metrics-csv metrics.csv
```

默认 `primitive-curves` 只硬保护圆、近圆和椭圆等拟合 primitive；普通折线和 generic creases 主要由软成本影响。需要旧式严格锁边时使用 `--feature-protection-mode all-feature-edges` 或兼容别名 `--protect-all-feature-edges`。

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
