# 特征曲线约束说明

ManuMesh 当前特征曲线保护由独立 `FeatureDetector`/`detectFeatureCurves()`、`FeatureConstraints.cpp` 和 `SimplifyOptions` 中的 feature 相关参数共同实现。它不是完整 CAD 约束求解器，而是在三角网格上识别特征 loop，并在 QEM collapse 中加入软成本、投影和硬拒绝策略。

更完整的数学背景见 [`algorithm_essence.md`](algorithm_essence.md)。本文聚焦“特征曲线进入简化器后，每一步为什么这样做”。

## 检测输入

`FeatureOptions` 当前使用：

- 边界边。
- 非流形边。
- 有向二面角超过 `featureAngleDeg` 的硬边（绕向感知：绕向一致时用带符号法向点积，可识别 >90° 的反折边；绕向不一致的边回退无符号角并计入 `inconsistentWindingEdges` 诊断）。
- normal-tensor 弱特征证据。
- opt-in 的 smooth-curvature 弱特征证据（`useSmoothCurvatureFeatures`，默认关闭）：多尺度三次 Monge 拟合 + 解析 extremality + Ohtake 边零交叉判据产生的确定性 ridge/valley 证据，2026-07-11 落地、2026-07-12 升级，设计见 [`smooth_curvature_feature_detection_2026_07_11.md`](smooth_curvature_feature_detection_2026_07_11.md)。
- `loopTraceAngleDeg` 控制哪些已识别 edge 进入 loop tracing；默认 `-1` 表示复用 `featureAngleDeg`。
- loop tracing 后的圆、近圆、椭圆和折线 primitive 拟合（圆用 Taubin 代数拟合、椭圆用 Halíř-Flusser 直接拟合）。

`featureAngleDeg` 决定“这条边是否进入 feature evidence”，`loopTraceAngleDeg` 决定“这条二面角边是否参与 loop ownership”。因此浅特征可以被用户显式保留下来；如果用户把 trace 阈值设得更严格，报告中的 `untracedFeatureEdges` 会提示这些 edge 没有被下游曲线保护消费。

normal tensor 弱特征现在还会记录每个顶点的局部边长尺度、多尺度 persistence 和 `persistentFeatureScore`。`--normal-tensor-min-persistent-scales` 同时影响 feature edge 接受和 QEM `weight-mode=normal-tensor` 的空间权重，避免检测和简化使用两套弱特征语义。

## 简化中的约束层

| 层 | 当前实现 | 作用 |
| --- | --- | --- |
| 软 line weight | `weightMode`、`featureBoost` | 让特征附近候选成本更高。 |
| 特征曲线 quadric | `featureCurveWeight` | 沿检测到的 loop 加 tangent-line 约束。 |
| placement 投影 | 圆、近圆、椭圆、polyline | 把候选位置拉回拟合 primitive 或原始折线。 |
| 曲线预算 | `maxFeatureCurveDeviationRatio` | 原始 placement 偏离曲线太远时拒绝。 |
| 最小 loop 顶点数 | `minFeatureLoopVertices`、`minCircularFeatureLoopVertices` | 防止重要 loop 被压到过少顶点。 |
| 硬保护策略 | `featureProtectionMode` | 决定哪些 loop/边可以触发硬拒绝。 |
| trace 诊断 | `tracedFeatureEdges`、`untracedFeatureEdges` | 区分 feature evidence 和可被 loop ownership 消费的边。 |
| tensor 尺度诊断 | `normalTensorScoredVertices`、`maxNormalTensorPersistentScore`、`meanNormalTensorLocalScale`、`meanNormalTensorPersistence` | 判断弱特征证据是否有稳定多尺度支持。 |
| graph cleanup | `graphCleanupBridgedGaps`、`graphCleanupRemovedSpurs`、`graphCleanupMergedJunctions` | 在 primitive fitting 前修复短断裂、去掉 normal-tensor/smooth-curvature 弱 spur，并记录 cleanup 影响。 |
| component confidence | `FeatureComponent`、`meanFeatureComponentConfidence`、`weakFeatureComponents` | 把强/弱证据比例、闭合率、junction、tensor persistence 和 primitive residual 汇总成可被 QEM 消费的 support 置信度。 |

## 每一层的由来

### 软 line weight

二面角、normal tensor 和高度/空间带权重本质上都是“候选排序偏置”。它们不会禁止坍缩，只是让特征附近的 line quadric 权重更高，使优先队列更倾向于先删掉非特征区域。

这对应特征保持简化中的常见思路：不要一开始就把所有特征锁死，而是先让代价函数表达局部重要性。相关来源包括 feature-sensitive metric、显著性加权简化和 QEM 变体。

### 特征曲线 quadric

普通 line quadric 使用顶点法向线，主要解决平坦区切向漂移；feature curve quadric 使用的是特征曲线切向。直观上，它惩罚点离开曲线附近的二维正交补，使 feature 顶点更愿意沿曲线方向移动，而不是横向漂移。

对圆/椭圆 loop，这个切向来自 primitive 拟合；对折线 loop，则来自相邻特征边方向。这是“曲线支撑”而不是完整 CAD 约束。

small cycle basis 和 circular fallback 现在按 trace connected component 运行。normal-tensor 产生的弱 ridge 只会阻止同一 component 的圆形 fallback，不会全局阻止其他干净圆孔或孔边界的补救恢复。

### component confidence 与 graph cleanup

`FeatureAnalysis::components` 是 raw edge evidence 与 QEM 之间的中间层。每个 component 记录 edge count、boundary/dihedral/tensor/smooth-curvature/non-manifold/cleanup bridge 来源、endpoint 数、junction 数、cycle rank、closure rate、tensor persistence、curvature persistence、primitive residual 和 confidence。component confidence 把 normal-tensor 与 smooth-curvature 视为相互独立的弱支持，硬证据（boundary/dihedral/non-manifold）始终占主导。`FeatureLoop` 与 `VertexFeature` 会记录 `componentId`、`confidence` 和 `weakFeature`。

Graph cleanup 默认开启，使用局部平均边长归一化阈值：短 endpoint gap 的连接段必须同时延续两端链切向；close junction 当前只按局部尺度距离桥接，没有切向/法向/source-kind 复核，因此存在误合并邻近曲线网络的风险；短 weak-evidence spur 覆盖 normal-tensor 与 smooth-curvature 两通道，并要求没有 boundary/dihedral/non-manifold/cleanup-bridge 支持。除按边数剪枝（`featureGraphMaxWeakSpurEdges`）外，`featureGraphMinWeakSpurStrength`（默认 0 = 旧行为；C++ feature/simplify options、CLI 与 C ABI 均已暴露）为正时改按 Yoshizawa 组件级无量纲强度 `T = (∫ds)·(∫strength ds)` 裁决。可用 `--no-feature-graph-cleanup` 关闭，或用 `--feature-graph-gap-ratio`、`--feature-graph-max-weak-spur-edges`、`--feature-graph-min-weak-spur-strength` 调参。cleanup 新增的桥接边不会伪装成 raw feature evidence，而是通过 `graph_cleanup_*` 诊断单独报告。源码：`FeatureGraphCleanup.cpp:53-223, 247-413, 493-505`。

QEM 的 feature-curve soft quadric 会按 component confidence 温和缩放。强 CAD loop 接近原始 `featureCurveWeight`，弱 tensor support 会先作为较软成本进入排序；是否硬保护仍由 `featureProtectionMode` 决定。

### placement 投影

即使 quadric cost 偏好曲线，新位置仍可能落在曲线附近但不在曲线上。圆孔这类制造特征对径向漂移敏感，所以当前实现会把同一受保护 loop 内的 collapse placement 投影回：

- 圆：固定圆心、法向和半径。
- 椭圆：固定中心、法向、主/次轴和主/次半径。
- 折线：严格模式下投影到原始折线段或局部切线。

投影的数学含义是把无约束局部最优点映射到低维曲线模型上。它改善特征形状，但仍要经过后续拓扑、质量、法线和自交过滤。实现上，不少于 64 段的折线 loop 使用 `PolylineSegmentIndex` AABB 树做最近段查询（O(log L)），短 loop 保留常数更小的线性扫描。

### 曲线预算

`maxFeatureCurveDeviationRatio` 在投影前检查原始 placement 到曲线的距离。如果原始最优点离曲线太远，直接投影可能把点拉到一个局部几何完全不同的位置，造成瘦三角形或局部折叠。预算检查相当于问：“这个候选本来是否已经接近曲线约束？”如果答案是否，就拒绝而不是强行投影。

### 最小 loop 顶点数

圆孔或椭圆孔即使形状还在，如果被压到极少顶点，也会失去可制造和可识别性。`minCircularFeatureLoopVertices` 和 `minFeatureLoopVertices` 控制的是 feature support 的离散分辨率，不是几何距离误差。它与 `featureCurveWeight`、曲线预算共同决定“保形”和“可删减”的折中。

## 保护模式

| 模式 | 含义 |
| --- | --- |
| `none` | 不启用硬特征保护，只保留软成本。 |
| `circular-only` | 只硬保护圆和近圆 loop。 |
| `primitive-curves` | 默认模式，硬保护圆、近圆和椭圆，generic crease 保持较软。 |
| `all-feature-edges` | 严格模式，所有检测到的特征边都硬保护。 |



## 适用场景

适合：干净 CAD/STL 三角网格、孔洞边界、圆孔、近圆孔、椭圆孔、明显硬边和规则工业件。

不适合直接承诺：高噪扫描件、缺失拓扑的点云重建、B-Rep 语义特征、严格尺寸公差证明和全局几何约束。

## 调参建议

- 普通圆孔：`--preserve-feature-curves --feature-protection-mode primitive-curves --min-circular-feature-loop-vertices 12`。
- 泛硬边太多导致停滞：避免 `all-feature-edges`，使用默认 `primitive-curves`。
- 浅二面角需要进入 loop：降低 `--feature-angle-deg`，并让 `--loop-trace-angle-deg -1` 复用同一阈值。
- 只想观察浅边但暂不让它保护简化：把 `--loop-trace-angle-deg` 设得高于 `--feature-angle-deg`，再检查 `untraced_edges`。
- 弱特征不明显：尝试 `--weight-mode normal-tensor --normal-tensor-threshold 0.06 --normal-tensor-edge-alignment 0.2 --normal-tensor-scales 3 --normal-tensor-min-persistent-scales 2`。
- 光滑 fillet 中心线或平缓 ridge/valley 完全不出现在 feature graph：先用 `feature-report --smooth-curvature-features` 校准阈值；需要直接保护简化时在 `simplify` 使用同一组选项。CLI 会自动打开 feature-curve policy；C++ 调用需同时设置 `preserveFeatureCurves` 与 `useSmoothCurvatureFeatures`，也可继续传入预计算 `FeatureAnalysis`。
- 输出偏离曲线：增大 `featureCurveWeight` 或减小 `maxFeatureCurveDeviationRatio`，同时检查拒绝计数是否过高。

## 相关算法出处

| 主题 | 参考 |
| --- | --- |
| CAD/STL 特征线和边界提取 | `docs/papers/feature_detection/vidal_wolf_dupont_2011_robust_feature_line_extraction_cad_triangular_meshes.pdf`、`jiao_bayyana_2008_identification_c1_c2_discontinuities_surface_meshes_cad.pdf` |
| normal tensor / voting | `docs/papers/feature_detection/tsuchie_higashi_2014_normal_tensor_surface_feature_lines.pdf`，以及文献库 source 031、057、066、080 |
| 特征保持简化 | `docs/papers/feature_preserving_simplification/wang_2008_feature_sensitive_metric.pdf`、`hussain_2008_feature_preserving_mesh_simplification_vertex_cover.pdf` |
| 弱特征支撑 | 文献库 source 088 `CWF: Consolidating Weak Features in High-quality Mesh Simplification` |
| 边折叠合法性 | `docs/papers/edge_collapse/rose_2025_mesh_simplification_edge_collapse_guide.pdf` |

本次实现细节和后续算法计划见 [`feature_detection_upgrade_2026_07_09.md`](feature_detection_upgrade_2026_07_09.md)。
