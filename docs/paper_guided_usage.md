# Paper-Guided Usage Notes

这份说明把论文中的使用建议映射到本程序的命令、demo case 和 viewer 观察方式。它不是论文复述，而是“该怎么用这个复现程序看懂论文里的取舍”。

## 1. 先建立标准 QEM 对照

论文的主线是“在标准 quadric error simplification 上加 line quadrics”。因此每个案例都应该先跑 `w=0`：

```powershell
.\build\Release\linequadrics.exe simplify input.stl output_standard.stl --method standard --ratio 0.15
```

或者用 sweep：

```powershell
.\build\Release\linequadrics.exe sweep input.stl output_dir --ratio 0.15 --weights "0,1e-5,1e-4,1e-3,1e-2,1e-1"
```

在 viewer 里先看 `standard_w_0e_00`，再向右滑到 line quadrics 结果。平面、阶梯、薄片这些案例里，标准 QEM 的退化会更明显。

注意：这个 sweep 固定目标面数，只改变权重。因此同一案例里的 faces 会很接近。它回答的是“同样面数下哪个结果更好”，不是“面数怎么逐级变少”。

如果要观察网格简化程度本身，用：

```powershell
.\build\Release\linequadrics.exe ratio-sweep input.stl output_ratio_dir --method line --line-weight 1e-3 --ratios "0.8,0.5,0.25,0.15,0.08,0.05"
```

viewer 里对应的 demo case 包括：

```text
sine_terrain_ratio_line
ridge_ratio_line
cube_ratio_dihedral
```

## 2. 论文 Eq. 16：默认 line quadrics

论文核心式子是：

```text
Q_augmented_i = Q_i + w_i * area_i * Q_line_i
```

程序对应：

```powershell
.\build\Release\linequadrics.exe simplify input.stl output_line.stl --method line --ratio 0.15 --line-weight 1e-3
```

代码对应：

- `lineQuadric(...)`：构造两张正交平面 quadric 的和。
- `computeInitialQuadrics(...)`：把 `w_i * area_i * Q_line_i` 加到顶点 quadric。

建议从 `1e-3` 开始。论文中也建议小权重通常更稳，过大权重会让均匀性压过几何保真。

## 3. 平面退化和均匀采样

看这些案例：

```text
clustered_plane
clustered_plane_boundary
hole_plane_boundary
```

论文强调：标准 QEM 在平面上误差为零，边折叠优先级可能变得随机，线性系统也可能奇异；line quadrics 给平面区域补充点到线距离，实际效果接近在平面内做 Frechet mean，从而鼓励更均匀的顶点分布。

在 `demo_summary.csv` 里重点看：

```text
mean_triangle_quality
min_triangle_quality
edge_length_cv
non_manifold_edges
```

在 viewer 里把 `Original` 打开，再从 `standard` 切到 `line w=1e-5` 或 `1e-3`。如果只是想看论文 Fig. 3/Fig. 4 的核心现象，先看 `clustered_plane`。

## 4. 高权重和自适应控制

论文说高权重可以做 adaptive simplification，但也可能损害外形。程序里有两种观察方式。

普通权重 sweep：

```powershell
.\build\Release\linequadrics.exe sweep input.stl output_dir --ratio 0.15 --weights "0,1e-4,1e-3,1e-2,1e-1"
```

论文 Sec. 4.4.1 的“先加小 line quadrics，再缩放整个 augmented quadric”的变体，对应：

```powershell
.\build\Release\linequadrics.exe simplify input.stl output_adaptive.stl --ratio 0.15 --line-weight 1e-3 --weight-mode xband --feature-boost 5 --adaptive-scale --adaptive-base-line-weight 0.01
```

注意：这里的 `xband`、`height`、`dihedral` 是为了演示而写的启发式权重来源；论文里的 adaptive 权重可以来自用户标注、skinning weights、geodesic distance 等更有语义的信息。

## 5. 软特征保留：程序里什么实现了，什么没实现

论文有两类相关想法：

1. 顶点重要性：给重要顶点更高的 line quadric weight。
2. 边重要性：Sec. 4.4.2 讨论 dihedral plane quadric，把额外平面 quadric 加到边附近。

本程序实现的是第一类：用启发式给顶点 line weight 加权。

```powershell
.\build\Release\linequadrics.exe simplify input.stl output_feature.stl --ratio 0.15 --line-weight 1e-3 --weight-mode dihedral --feature-boost 0.08 --feature-angle-deg 30
```

这里的 `--weight-mode dihedral` 并不是论文 Eq. 19 的 dihedral plane quadric；它只是用二面角检测硬边附近的顶点，然后提高这些顶点的 line quadric weight。这个区别很重要。

适合看的案例：

```text
ridge_dihedral
terrace_dihedral
cube_dihedral
thin_fin_dihedral
```

如果你想进一步复现论文 Sec. 4.4.2，下一步应该在程序里单独实现 edge dihedral plane quadrics，而不是继续调大 `--feature-boost`。

## 6. 边界不是 line quadrics 本身

开放 STL 或有洞的模型会出现边界问题。程序提供：

```powershell
--boundary-weight 5
```

这对应 Garland-Heckbert 风格的 boundary plane quadric，不是论文 line quadric 的一部分。它用于让 demo 更容易观察“内部均匀性”和“边界保持”这两个不同问题。

适合看的案例：

```text
hole_plane_boundary
cylinder
thin_fin_uniform
```

## 7. 噪声短板

论文结论明确提到：line quadrics 尊重输入顶点法线，因此不能作为 denoiser。

看这个案例：

```text
noisy_plane
```

建议在 viewer 里轮播 `w=0, 1e-3, 1e-2, 1e-1`。你会看到三角形质量可能变好，但它不会真正恢复干净平面。遇到扫描噪声时，应先做稳健法线估计或去噪，再做简化。

## 8. Viewer 里的推荐观察顺序

启动：

```powershell
.\run_demo.ps1 -Config Release
.\collect_metrics.ps1
.\start_viewer.ps1
```

打开：

```text
http://127.0.0.1:5174/viewer/
```

推荐顺序：

1. `clustered_plane`：看标准 QEM 的平面退化，再切到 `line w=1e-5`。
2. `ridge_uniform`：看 line quadrics 改善质量，但高权重可能影响尖锐特征。
3. `ridge_dihedral`：看启发式软特征保留。
4. `noisy_plane`：看它不是去噪器。
5. `thin_fin_dihedral`：看当前教学实现的拓扑保护不足。

## 9. 最小判断准则

选择参数时，不要只看一项指标：

```text
triangle quality improves
edge_length_cv decreases
geometry distance does not jump
non_manifold_edges does not increase
important feature remains visible
```

如果 `mean_triangle_quality` 变好但外形明显变差，说明 `line_weight` 或 `feature_boost` 过大。论文里 line quadrics 是控制项，不是万能目标函数。
