# 论文视角下的当前用法

这份说明把 QEM 与 line quadrics 论文中的概念映射到 ManuMesh 当前 `manumesh.exe`、STL 输出和 CSV 指标。当前验证入口是 CLI、CTest、CSV 和外部 STL/CAD 查看器，不再依赖浏览器预览任务。更系统的数学解释见 [`../design/algorithm_essence.md`](../design/algorithm_essence.md)。

建议先在 PowerShell 中定义当前 VS Code task 一致的输出目录：

```powershell
$buildDir = "build/vs2019-release"
$exe = "$buildDir/bin/Release/manumesh.exe"
```

## 先选场景档案

常见任务不需要从几十个低层阈值开始。`simplify`、`feature-report`、`feature-benchmark` 和
`feature-compare` 都接受同一组档案：先选输入来源，再只覆盖确实需要调整的字段。

| 档案 | 适用输入 | 自动选择 | 简化时的保护策略 |
| --- | --- | --- | --- |
| `default` | 现有脚本、一般 STL | 使用 FeatureOptions/profile 的检测默认（最小环 8） | 不自动开启特征曲线保护 |
| `cad` | 干净 CAD/STL、孔和棱边 | 二面角排序，关闭弱 tensor 证据 | 保护圆、近圆、椭圆等几何基元 |
| `scan` | 有中轻度法向噪声的扫描网格 | 法线滤波、3 个 tensor 尺度、2 个持久尺度 | tensor 候选参与软成本；仅硬保护已拟合的几何基元 |

最短的实用起点：

```powershell
# 机械件：保留孔和规则特征
& $exe simplify input.stl output.stl --profile cad --ratio 0.25

# 含噪扫描：先检查特征，再决定简化率
& $exe feature-report scan.stl --profile scan --print-resolved-config
& $exe simplify scan.stl scan_simplified.stl --profile scan --ratio 0.5

```

显式参数总会覆盖档案值。例如 CAD 档案上需要 tensor 证据时，可写
`--profile cad --normal-tensor-features --normal-tensor-scales 3`；需要把所有已检测边严格锁定时，
可写 `--feature-protection-mode all-feature-edges`。`scan` 默认不这样做：张量候选会继续参与曲线 quadric
的软排序，避免含噪候选让简化在达到目标面数前停滞。`--no-feature-normal-filter` 和
`--no-preserve-feature-curves` 用于明确关闭由档案或旧兼容行为启用的通道。

在第一次运行或复现结果时加入 `--print-resolved-config`。它会输出各组有效值，而不是只回显命令行；
特别适合确认 profile 和显式覆盖后的最终状态。

CLI 会在以下情况向 stderr 给出提示：`--target-faces` 与 `--ratio` 同时出现（面数目标优先）、
两种局部误差单位同时出现（为了兼容旧版本，运行时保留较宽的换算预算）、标准 QEM 下的 line-quadric
权重、未开启通道的子参数，以及仅影响报告计数的 component-confidence 阈值。提示不改变 0.x 命令的兼容性，
但应视为需要收敛为一个明确配置的信号。

`--min-feature-loop-vertices` 的特征检测/报告下限可以设为 3 或 4；简化保护阶段仍保留 5 个顶点的
内部安全下限，以避免极短环在坍缩约束中不稳定。`--print-resolved-config` 会显示该下限，CLI 也会在
简化命令给出低于 5 的值时提示这一差异。

兼容边界需要单独记住：直接构造的旧 C++ `SimplifyOptions{}`、`SimplifyConfig{}` 和扁平 C ABI
初始化仍使用最小环 16；profile、`FeatureOptions{}` 以及 CLI 显式使用 8。这样旧 SDK 不会静默改变，
新入口也不必沿用过于保守的检测阈值。

## 先看现象，再选参数

| 现象 | 数学/算法原因 | 优先检查 |
| --- | --- | --- |
| 大平面区域顶点分布漂移或不均匀 | 标准 QEM 在切平面方向缺约束，候选代价大量接近零。 | `--method line`、`--line-weight`、`edge_length_cv`、`solver_fallbacks`。 |
| 圆孔简化后变椭圆或顶点太少 | 特征 loop 没有形成硬保护，或最低 loop 顶点数太低。 | `--preserve-feature-curves`、`--feature-protection-mode primitive-curves`、`--min-circular-feature-loop-vertices`。 |
| 开边界被吃掉或合并 | boundary weight 只作为软成本，不足以阻止拓扑改变。当前扩展 link condition 已始终拒绝"边界弦"折叠（两端点均为边界顶点的内部边），防止非流形 pinch 点；边界边自身的折叠仍需硬策略控制。 | `--preserve-boundary`，以及 `boundary_rejected_collapses`、`topology_rejected_collapses`。 |
| 达不到目标面数，提前停止 | 目标比例和硬过滤器冲突，候选被大量拒绝。 | `termination_reason`、最高的 `*_rejected_collapses`。 |
| normal tensor 结果不稳定 | 张量特征受邻域、尺度、噪声和采样影响。 | 轻中度法向噪声先试 `--feature-normal-filter` 并观察 angular-change 诊断，再调 tensor threshold/alignment/scales。 |
| 特征图碎成多个相邻 component | local cleanup 不跨 component。 | 试 `--feature-graph-consolidation`，同时限制 gap/alignment 并检查 bridge count 与 branch-pair precision。 |
| patch 分区泄漏或过分割 | feature barrier 漏检或误检。 | 用 `--surface-patches`；必要时用 `--surface-patches-strong-only` 排除弱 evidence barrier，并看 patch adjacency accuracy。 |

这个阅读顺序比单纯调大某个权重更安全。QEM 和 line quadrics 是排序成本；feature graph 是曲线支撑；legality filters 才是硬安全闸。

## 先建立标准 QEM 对照

单模型标准 QEM：

```powershell
& $exe simplify input.stl output_standard.stl --method standard --ratio 0.25
```

标准 QEM 不使用 line-quadric 权重；`sweep --method standard` 默认只运行一个基线，若显式传 `--weights` 只能使用单值 `0`。
在 `--method line` 下，`--line-weight 0` 会明确归一化为标准 QEM，并在 resolved config、文件名和 CSV 中标为 `standard`。

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

## placement 求解与 adaptive-scale 的当前实现

论文映射时注意两处与旧文档不同的实现事实：

- **三级 placement 回退链（GH97）**：全空间最优 `A x = -b` → 沿折叠边的一维最优（rank-2 直棱/边界折痕的良定情形，尺度不变相对阈值）→ 端点/中点。`solver_fallbacks` 统计的是最终落到端点/中点候选的次数。边界边折叠的 placement 采用 Lindstrom-Turk 边界守恒约束投影（`src/simplification/Placement.cpp`）。
- **`--adaptive-scale`（Wang 2008 优先级解耦）**：开启后 `--feature-boost` 不再放大 quadric，而是作为逐顶点队列优先级因子 `priorityScale = 1 + featureBoost * score` 只乘候选排序代价（取两端点 max）；placement 使用干净的 `--adaptive-base-line-weight`（默认 `1e-2`）line quadric。不开启时保持旧行为。

另外，二面角证据现在是**绕向感知**的，正典实现为 `common::computeOrientedDihedralAngle`：两邻面绕向一致时用带符号 dot，可识别超过 90° 的反折边；绕向不一致时回退无符号角，并计入 `inconsistentWindingEdges`。feature-report、simplify stdout/metrics CSV 与 C ABI simplify report 均输出对应诊断。

## 特征权重和特征保护

当前有三类特征相关能力：

- `--weight-mode dihedral`：用二面角硬边提高附近顶点 line weight，是软成本。
- `--weight-mode normal-tensor`：用带局部尺度和多尺度 persistence 的 normal tensor 给弱特征提供附加证据，是软成本。
- `--preserve-feature-curves`：启用特征环检测、曲线 quadric、placement 投影和硬保护策略。

特征分析和简化命令共享法线滤波、Normal Tensor 与图整合配置：

```powershell
& $exe feature-report input.stl `
  --feature-normal-filter `
  --feature-graph-consolidation `
  --surface-patches `
  --csv features.csv
```

直接保护简化：

```powershell
& $exe simplify input.stl output_scan.stl `
  --ratio 0.5 --feature-normal-filter `
  --feature-graph-min-weak-spur-strength 0.0 `
  --metrics-csv scan_metrics.csv
```

在 `simplify` 中，`--feature-normal-filter` 和 `--feature-graph-consolidation` 会沿用 0.x 的自动特征保护行为，等价于同时启用 `--preserve-feature-curves`。`feature-report` 可以单独使用这些检测选项。默认保护模式仍是 `primitive-curves`。normal filter 只改检测法向，surface patches 只属于 feature analysis。报告包含 normal-filter、consolidation、junction pair 和 patch 诊断。纹理感知简化通过 C++ `SimplifyConfig::texture` 显式开启，CLI `simplify` 没有对应选项。

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

line quadrics 和 normal tensor 都不是去噪器。ManuMesh 的 opt-in normal filter 可先稳定面法向 evidence，但不会移动顶点；强位置噪声仍应先做稳健去噪或重建。

## 相关论文出处

| 程序概念 | 论文来源 |
| --- | --- |
| plane quadric / edge contraction cost | Garland-Heckbert 1997，`documentation/papers/qem/garland_heckbert_1997_surface_simplification_qem.pdf` |
| placement 三级回退链（全空间 → 沿边一维 → 端点/中点） | Garland-Heckbert 1997 的求解与回退框架；实现在 `src/simplification/Quadrics.cpp` |
| 边界边 placement（边界守恒约束） | Lindstrom-Turk 1998，`documentation/papers/edge_collapse/lindstrom_turk_1998_fast_memory_efficient_simplification.pdf`；实现在 `src/simplification/Placement.cpp` |
| `--adaptive-scale` 下 featureBoost 的队列优先级解耦 | Wang 2008 feature-sensitive metric，`documentation/papers/feature_preserving_simplification/wang_2008_feature_sensitive_metric.pdf` |
| 圆拟合（Taubin，一阶无偏；Kåsa 保留为回退）与椭圆拟合（Halíř-Flusser 直接最小二乘，保证椭圆输出） | Taubin 1991、Halíř-Flusser 1998；实现在 `src/feature_detection/PrimitiveFit.cpp` |
| point-to-line quadrics | Liu-Rahimzadeh-Zordan 2025，`documentation/papers/line_quadrics/liu_rahimzadeh_zordan_2025_line_quadrics.pdf` |
| 纹理感知简化（`preserveTexture`，C++/C API） | Garland-Heckbert 1998 属性 QEM（M003，`documentation/papers/qem/garland_heckbert_1998_color_texture_qem.pdf`）为历史参照；当前实现按现代 edge-collapse 管线文献（M033）的工程拆分：4×4 几何 quadric 排序固定 3D placement，UV chart/面积合法性为显式局部策略，实现在 `src/simplification/TextureProtection.cpp`，设计见 `documentation/design/texture_aware_qem.md` |
| edge-collapse 工程细节 | Hoppe 1996、Lindstrom-Turk 1998、Rose 2025，位于 `documentation/papers/edge_collapse/` |
| CAD/STL 特征线 | Vidal-Wolf-Dupont 2011、Jiao-Bayyana 2008，位于 `documentation/papers/feature_detection/` |
| normal tensor 特征评分 | Tsuchie-Higashi 2014，`documentation/papers/feature_detection/tsuchie_higashi_2014_normal_tensor_surface_feature_lines.pdf` |
| normal-domain feature stabilization | M009/M012/M013/M015 的 multiscale crease、normal voting/tensor 路线；工程对照包括 MeshLib、L0Denoising、NLLR、LSD |
| weak component consolidation | CWF/M026 的“先整合弱支持再保护”原则；当前只实现受方向/source/sign 门控的局部 endpoint recovery |
| feature-induced patch segmentation | M024/M025；当前实现 connectivity partition 与 adjacency，不含 analytic surface fitting/merge |
| 特征敏感简化 | Wang 2008、Hussain-Grahn-Persson 2008，位于 `documentation/papers/feature_preserving_simplification/` |
| feature graph 弱 spur 强度裁决 T=(∫ds)·(∫strength ds) 与 gap 桥接角度规则 | Yoshizawa（M021）；`featureGraphMinWeakSpurStrength`（默认 `0.0` 即旧行为）已映射到 C++ feature/simplify options、`--feature-graph-min-weak-spur-strength` 与 C ABI 尾字段，实现在 `src/feature_detection/FeatureGraphCleanup.cpp` |
