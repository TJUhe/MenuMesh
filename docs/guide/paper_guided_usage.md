# Paper-Guided Usage Notes

这份说明把 line quadrics 论文里的观察方式映射到当前 CLI、STL 输出和 CSV
指标。网页预览不再是验证入口；需要看形状时直接打开生成的 STL。

## 1. 先建立标准 QEM 对照

单个模型：

```powershell
.\build\mingw-ninja-release\bin\linequadrics.exe simplify input.stl output_standard.stl --method standard --ratio 0.15
```

同目标面数下比较不同权重：

```powershell
.\build\mingw-ninja-release\bin\linequadrics.exe sweep input.stl output_dir --ratio 0.15 --weights "0,1e-5,1e-4,1e-3,1e-2,1e-1"
```

查看 `output_dir/metrics.csv`，再打开 `standard_w_0e_00.stl` 和若干
`line_w_*.stl`。这个 sweep 固定目标面数，只改变权重，因此回答的是“同样面数
下哪个结果更好”，不是“面数逐级下降时会怎样”。

如果要观察不同简化率：

```powershell
.\build\mingw-ninja-release\bin\linequadrics.exe ratio-sweep input.stl output_ratio_dir --method line --line-weight 1e-3 --ratios "0.8,0.5,0.25,0.15,0.08,0.05"
```

## 2. Eq. 16：默认 line quadrics

论文核心形式：

```text
Q_augmented_i = Q_i + w_i * area_i * Q_line_i
```

CLI 对应：

```powershell
.\build\mingw-ninja-release\bin\linequadrics.exe simplify input.stl output_line.stl --method line --ratio 0.15 --line-weight 1e-3
```

建议从 `1e-3` 开始。权重过大会让均匀性压过几何保真，尤其在尖锐特征和高曲率
区域要同时看 STL 外观和距离误差。

## 3. 平面退化和均匀采样

推荐案例：

```text
clustered_plane
clustered_plane_boundary
hole_plane_boundary
```

重点看：

```text
mean_triangle_quality
min_triangle_quality
edge_length_cv
non_manifold_edges
```

标准 QEM 在大平面区域可能出现退化和不均匀采样；line quadrics 会补充切向正则
化，通常降低 `edge_length_cv` 并提高三角形质量。

## 4. 高权重和自适应控制

普通权重 sweep：

```powershell
.\build\mingw-ninja-release\bin\linequadrics.exe sweep input.stl output_dir --ratio 0.15 --weights "0,1e-4,1e-3,1e-2,1e-1"
```

启发式自适应权重示例：

```powershell
.\build\mingw-ninja-release\bin\linequadrics.exe simplify input.stl output_adaptive.stl --ratio 0.15 --line-weight 1e-3 --weight-mode xband --feature-boost 5 --adaptive-scale --adaptive-base-line-weight 0.01
```

`xband`、`height`、`dihedral` 是演示用权重来源；工业输入更理想的权重来源是
用户标注、工艺语义、skinning weights、geodesic distance 或上游 CAD 特征。

## 5. 软特征保持

当前程序有两层特征相关能力。第一层是“给疑似重要顶点提高 normal-line
quadric 权重”：

```powershell
.\build\mingw-ninja-release\bin\linequadrics.exe simplify input.stl output_feature.stl --ratio 0.15 --line-weight 1e-3 --weight-mode dihedral --feature-boost 0.08 --feature-angle-deg 30
```

`--weight-mode dihedral` 不是论文 Sec. 4.4.2 的 edge dihedral plane quadric；
它只是用二面角检测硬边附近顶点并提高 line weight。

第二层是当前已经落地的曲线特征保护。它会先运行 `detectFeatureCurves()`，
恢复 feature graph / loop，拟合圆、近圆或椭圆，并把同一 loop 内的 placement
投影回圆、椭圆或原始 polyline：

```powershell
.\build\mingw-ninja-release\bin\linequadrics.exe simplify input.stl output_curve.stl --method line --ratio 0.20 --line-weight 1e-3 --weight-mode dihedral --feature-boost 0.08 --feature-angle-deg 25 --preserve-feature-curves --feature-curve-weight 0.08 --max-feature-curve-deviation-ratio 0.05 --circle-fit-threshold 0.04 --ellipse-fit-threshold 0.05 --min-circular-feature-loop-vertices 12 --samples 1000 --metrics-csv metrics.csv
```

这仍然不是完整复现论文中的 edge dihedral plane quadrics。若要更完整复现论文中
的边特征项，下一步应单独实现 edge dihedral plane quadrics，或把当前 feature
graph 作为 constrained edges 输入给独立的 legality / placement policy。

对弱特征，可以把权重来源切到 normal tensor：

```powershell
.\build\mingw-ninja-release\bin\linequadrics.exe simplify input.stl output_nt.stl --ratio 0.25 --line-weight 1e-3 --weight-mode normal-tensor --feature-boost 0.08 --normal-tensor-threshold 0.06 --normal-tensor-edge-alignment 0.2 --normal-tensor-scales 3
```

推荐案例：

```text
ridge_dihedral
terrace_dihedral
cube_dihedral
thin_fin_dihedral
tests/data/feature_fixtures/coaxial_hole_plate.obj
tests/data/feature_fixtures/elliptical_hole_plate.obj
tests/data/feature_fixtures/boss_pocket_plate.obj
```

## 6. 边界不是 line quadrics 本身

开放 STL 或有孔模型会有边界保持问题。当前 CLI 提供：

```powershell
--boundary-weight 5
```

这对应 Garland-Heckbert 风格的 boundary plane quadric，不是 line quadrics 的一
部分。当前程序还提供 `--preserve-boundary`，这会启用 boundary legality
policy：允许同一当前边界边上的合法坍缩，拒绝边界到内部或跨边界环的坍缩。
边界项用于改 placement 代价，`--preserve-boundary` 用于改可否坍缩，二者不要混淆。

## 7. 噪声短板

line quadrics 尊重输入顶点法线，因此不是 denoiser。推荐看：

```text
noisy_plane
```

可比较 `w=0`、`1e-3`、`1e-2`、`1e-1` 的 STL 和 CSV。三角形质量可能变好，
但它不会自动恢复干净平面；扫描噪声应先做稳健法线估计或去噪。

## 8. 最小判断准则

不要只看单一指标。合理结果应同时满足：

```text
triangle quality improves
edge_length_cv decreases
geometry distance does not jump
non_manifold_edges does not increase unexpectedly
important feature remains visible in STL
```

如果 `mean_triangle_quality` 变好但外形明显变差，说明 `line_weight` 或
`feature_boost` 过大。line quadrics 是控制项，不是万能目标函数。
