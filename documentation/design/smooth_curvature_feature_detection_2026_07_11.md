# 确定性平滑曲率特征检测

日期：2026-07-11（2026-07-12 更新：三次 Monge 拟合、解析极值性、Ohtake 边零交叉判据、
Yoshizawa 组件强度过滤器；2026-07-13 更新：针对 Dupin cyclide 虚假响应的逐交叉
cyclideness 门禁，由解析环面 fixture 暴露；2026-07-15 更新：当前源码审计、纯支持尺度
投票语义、精确边接受规则、源码定位、CLI/简化器边界和当前性能证据；2026-07-15 再次更新：
可选的稳定尺度选择和稳定性诊断）

此升级仅限于三角形和多边形表面网格，不引入 B-Rep 实体、实体建模、CAD 特征树、学习式
评分、神经网络或训练数据。

## 为什么采用独立证据通道

边界、非流形和二面角边是离散网格中的不连续处；平滑 ridge 和 valley 是局部光滑曲面上的
微分事件。将它们的原始分数混合会使阈值难以解释，并让噪声直接与强拓扑证据竞争。

因此 ManuMesh 保留两条通道：

1. 强证据：边界、非流形和二面角边。
2. 弱平滑证据：法向张量和多尺度 quadric 曲率。

两条通道仅在显式的 `FeatureGraph` 处汇合；在这里可以记录来源归属、连续性、junction、
清理、组件和置信度，供下游简化及未来 remeshing 适配器使用。

## 当前源码契约

当前行为以实现为准，而不是以早期设计草图为准。重要源码位置如下：

| 职责 | 源码符号 | 当前所在位置 |
| --- | --- | --- |
| 公共控制项和逐顶点结果 | `FeatureOptions`、`SmoothCurvatureOptions`、`SmoothCurvatureVertex` | `include/algorithms/feature_detection/FeatureTypes.h` |
| 局部法向和 k-ring 收集 | `computeAreaWeightedVertexNormals`、`gatherNeighborhood` | `src/feature_detection/SmoothCurvature.cpp` |
| 三次 Monge 拟合和鲁棒求解 | `fitScale` | `src/feature_detection/SmoothCurvature.cpp` |
| 按尺度的 ridge/valley 分类 | `classifyScaleCandidate` | `src/feature_detection/SmoothCurvature.cpp` |
| 跨尺度支持和参考尺度选择 | `computeSmoothCurvatureFeaturesCached` | `src/feature_detection/SmoothCurvature.cpp` |
| 顶点证据到网格边证据 | `smoothCurvatureEdgeCandidate` | `src/feature_detection/FeatureEvidence.cpp` |
| 图清理/整合 | `cleanupTraceGraph`、`consolidateFeatureGraph` | `FeatureGraphCleanup.cpp`、`FeatureGraphCompatibility.cpp`、`FeatureGraphConsolidation.cpp` |
| CLI 选项绑定 | `parseFeatureOptions` | `apps/CliOptionBinding.cpp` |

早期版本要求最粗的请求尺度必须支持候选。由于这会压制物理宽度小于最粗邻域的真实特征，
该否决规则已移除。当前实现采用纯支持尺度投票：支持尺度不必相邻，最粗尺度也没有特殊地位。
回归测试 `FeatureDetectionAnalytic.NarrowRidgeOnDenseSheetSurvivesCoarsestScale` 保证该行为。

## 算法

对每个顶点和每个请求的拓扑尺度执行以下步骤：

1. 使用共享的 `FeatureDetectionCache` 构建 k-ring 邻域和面积加权局部法向（每次检测运行只构建一次
   邻接、边信息和平均边长，并在所有证据阶段复用）。
2. 用局部平均边长乘环数对坐标归一化。
3. 拟合三次 Monge patch（Yoshizawa，M021）

   `w = a*u^2 + b*u*v + c*v^2 + d*u + e*v
        + c0*u^3 + c1*u^2*v + c2*u*v^2 + c3*v^3`

   使用高斯距离权重和确定性的 Huber 重加权。九未知量系统通过增量累积加权正规方程（仅上三角）求解，
   取代每次鲁棒迭代中的一次稠密 QR；半径归一化使 9x9 系统保持良好条件数。可用邻居少于九个的拟合
   会因欠定而被拒绝。
4. 从二次项构造第一、第二基本形式，并求解广义自伴特征问题，得到两个带符号主曲率及其方向。
5. 对每个主方向直接从三次项计算解析极值性 `e_i = grad(kappa_i) . t_i`，其三阶方向形式为
   `e ~ 6*(c0*t1^3 + c1*t1^2*t2 + c2*t1*t2^2 + c3*t2^3)`。
   不使用相邻曲率的有限差分。
6. 使用 Ohtake 边零交叉判据（M011 公式 3-4）分类 ridge/valley 支持：主方向构成线场，因此先对邻居的
   切线和极值性进行符号同步；仅当以下条件同时满足时，顶点才支持候选：(a) 边的两个端点都通过曲率
   支配性测试（ridge 为 `kappa_max > |kappa_min|`，valley 为 `kappa_min < -|kappa_max|`）；(b) 沿着大致
   遵循主方向的入射边，极值性发生符号变化；(c) 两个端点都通过一阶极大值测试（Ohtake 推荐用它实际
   替代二阶导数测试）。子顶点归属使用 Ohtake 的逆插值：仅 `|e|` 较小的端点认领交叉点，使检测带宽
   保持为一个顶点。
7. 使用 cyclideness（Yoshizawa，M021 公式 5-6）对每个接受的交叉进行门控。在 Dupin cyclide（球、圆柱、
   圆锥、环面）上，极值性场恒等为零，因此其中检测到的符号变化是离散化噪声，而非曲率极值：整个极值
   圆是 kappa 的驻集，不携带 crest。靠近真实 crest 时，`|e|` 随偏离 crest 线的距离按 `|de/dt|` 增长；
   因此端点 `|e|` 的平均值（在交叉点处对 Yoshizawa 的 cyclideness C 进行线性插值）可衡量极值显著性。
   门控条件为

   `0.5 * (|e_center| + |e_neighbor|) >= kMinCrossingCyclidenessRatio * kappa^2`

    其中 `kMinCrossingCyclidenessRatio = 0.15`。除以 `kappa^2` 后比值无量纲（e 和 `kappa^2` 都按 `1/L^2`
    缩放，并在相同的半径归一化单位中估计），并且对统一缩放严格不变：它询问沿自身曲率线的一个曲率
    半径内，kappa 是否至少变化其自身的固定比例，在 cyclide 上为零，在真实 crest 上为 O(1)。2026 年
    7 月测得的标志值：Gaussian ridge/bump fixture 的真实 crest 比值 >= 0.38（p10），中位数远高于 1；
    环面的虚假内侧 valley 带在 24-48 个小段上峰值为 0.06，因此 0.15 对两类数据都保留 2.5 倍余量。
    在此门控前，环面在工作阈值 0.008 下于 24/32/36/48 个小段产生持久虚假 valley，最大持久分数分别为
    0.097/0.038/0.026/0.011；门控后，所有测试密度下环面都完全静默（最大持久分数 0.0），Gaussian ridge
    响应逐位一致。
8. 使用尺度归一化曲率幅值、各向异性、零交叉强度（平均 `|e|` 乘切向边延伸量，以半径归一化单位计）
   和拟合残差质量为候选评分。
9. 选择参考尺度。默认保留最高分尺度以保持向后兼容。启用 `useStableScaleSelection` 后，依据跨尺度
   符号/切线/分数一致性对有效候选排序，并拒绝低于 `minScaleStability` 的候选。请求的每个尺度在以下
   条件全部满足时投一票：(a) 有效；(b) 分数至少为 `max(persistenceThreshold, 0.30 * referenceScore)`；
   (c) 带符号 ridge/valley 类型与参考一致；(d) 与参考切线的绝对点积至少为 `minTangentConsistency`。
   支持尺度不必相邻，最粗请求尺度也不必投票。实现随后计算

   `persistenceRatio = persistentScales / scaleCount`

   `persistentFeatureScore =
      (0.65 * referenceScore + 0.35 * meanSupportedScore)
      * persistenceRatio * meanSupportedAlignment`

    不支持的尺度对 `averageFeatureScore` 贡献零，其存储值为 `supportedScoreSum / scaleCount`。
10. 仅当两个端点在符号、切线、尺度支持和边对齐方面一致时，才将持久顶点转换为网格边证据。

最终分数无量纲。统一缩放网格无需重新调整曲率阈值。

## 网格边接受规则

顶点证据在通过 `smoothCurvatureEdgeCandidate` 转换为显式网格边之前，仅用于诊断：

| 门控 | 当前精确规则 |
| --- | --- |
| 强证据排除 | 如果该边已经是 boundary、dihedral 或 non-manifold，则拒绝；如果任一端点被标记为离散特征顶点也拒绝，从而在强 CAD 证据周围形成一个顶点宽度的排除区。 |
| 尺度数 | `min(endpointA.persistentScales, endpointB.persistentScales) >= smoothCurvatureMinPersistentScales`。 |
| 分数 | 两个端点的持久分数都必须达到 `smoothCurvatureFeatureThreshold`。 |
| 带符号类型 | 两个端点都必须非零且一致：ridge 对 ridge 或 valley 对 valley。 |
| 边对齐 | 边方向必须与两个端点的曲线切线对齐：`min(|d.tA|, |d.tB|) >= smoothCurvatureMinEdgeAlignment`。 |
| 端点切线一致性 | `|tA.tB| >= smoothCurvatureMinTangentConsistency`。 |

这有意比当前 normal-tensor 边规则更严格；后者只要对齐更好的端点达到阈值即可接受方向对齐。一条网格边
仍可能同时带有 normal-tensor 和 smooth-curvature 标记，因为两种弱策略在强证据门控后独立评估。

## 公共控制项

`FeatureOptions` 提供可选启用的平滑通道：

- `useSmoothCurvatureFeatures`
- `smoothCurvatureFeatureThreshold`
- `smoothCurvatureMinEdgeAlignment`
- `smoothCurvatureMinTangentConsistency`
- `smoothCurvatureBaseNeighborhoodRings`（默认 2；单环拟合对噪声过于敏感，不适合一般用途）
- `smoothCurvatureScaleCount`
- `smoothCurvatureMinPersistentScales`
- `smoothCurvatureRobustFitIterations`
- `smoothCurvatureUseStableScaleSelection`（默认 false）
- `smoothCurvatureMinScaleStability`（默认 0.0）

图清理还通过 C++ `FeatureOptions`、C++ `SimplifyOptions`、两个 CLI 选项表（`--feature-graph-min-weak-spur-strength`）
以及带大小信息的 C ABI 尾部暴露 `featureGraphMinWeakSpurStrength`（默认 0.0）。启用正值时，悬空的弱证据链
按无量纲的 Yoshizawa 曲线强度 `T = (integral ds) * (integral strength ds)` 判断：`ds` 使用局部平均边长单位，
每条边的强度为持久分数除以匹配的通道阈值，不再使用旧的边数上限。因此较长但微弱的平滑 ridge 会在清理后
保留，而短但强烈的噪声尖峰会被剪除。默认值完全保持旧行为。

该通道默认关闭，因为干净 CAD/STL 的硬边检测与噪声/自由形状的平滑特征检测需要不同的验证集。除非启用平滑
通道，或调用方提供包含平滑特征的预计算分析，否则现有简化行为保持不变。

CLI 将相同控制项提供给 `feature-report`、`feature-benchmark`、`feature-compare` 和 `simplify`。在 `simplify` 中，
`--smooth-curvature-features` 启用平滑曲率证据；在 `simplify` 中它沿用 0.x 的自动特征保护行为，因此检测到的图会被保护策略消费，而不是计算后
丢弃。稳定尺度控制项以
`--smooth-curvature-stable-scale` and
`--smooth-curvature-min-scale-stability`.

## 诊断

`FeatureAnalysis` 现在报告：

- 平滑曲率评分顶点和图边；
- 最大原始分数和持久分数；
- 平均局部尺度、持久性和尺度稳定性；
- 每个顶点的 `selectedScale` 和 `scaleStability`；
- 每条边的 `smoothCurvature` 归属；
- 每个组件的平滑边数量和平均曲率持久性。

组件置信度将 normal-tensor 和 smooth-curvature 证据视为分离的弱支持，强证据仍占主导。

按来源统计的边计数是证据通道计数。由于一条图边可能同时带有两种弱标志，
`normalTensorFeatureEdges + smoothCurvatureFeatureEdges` 不保证等于唯一弱图边的数量。同样，清理阶段添加的
桥接边会附加到 `FeatureGraph`，但不计入原始 `featureEdges` 证据数。

`featureComponentMinConfidence` 仅是报告阈值：它控制 `highConfidenceFeatureComponents` 计数，不会删除组件，
也不会开启/关闭硬保护。简化在 `src/simplification/Quadrics.cpp` 中持续将特征曲线软 quadric 按
`0.35 + 0.65 * confidence` 缩放。

## 开源实现经验

- libigl：使用局部切线框架、k-ring 或半径邻域、quadric 拟合和显式主方向。ManuMesh 增加鲁棒重加权、尺度归一化、
  方向极值和持久性。
- pmp-library：将曲率作为具有显式边界和平滑策略的网格属性。ManuMesh 将评分与图归属分离。
- CGAL PMP：在 patch 或 remeshing 操作消费特征边之前，将其具体化为显式约束边图。`FeatureGraphEdge` 是 ManuMesh 的等价物。
- OpenMesh：将拓扑/状态/属性存储与特征策略分离。检测器依赖核心网格查询，不拥有编辑操作。

## 近期确定性文献

实现以局部 Ohtake（M011）和 Yoshizawa（M021）的 crest-line 方法（三次拟合、解析极值性、边零交叉、曲线强度
过滤）为基础，并结合 M014/M042-M044 曲率和 quadric 参考，再与近期非 AI 工作进行核对：

| 年份 | 工作 | 本处采用的工程经验 |
| --- | --- | --- |
| 2017/2018 | Yamakawa and Shimada, *Feature Edge Extraction Via Angle-Based Edge Collapsing and Recovery*, DOI `10.1115/1.4037227` | 多尺度简化可以暴露小型圆角中心；单一局部角度阈值并不足够。 |
| 2019 | Lu et al., *Feature Curve Network Extraction via Quadric Surface Fitting*, M044 | Quadric 拟合必须服务于曲线连续性和 junction/网络推理。 |
| 2020 | Romanengo et al., *HT-Based Identification of 3D Feature Curves and Their Insertion into 3D Meshes*, DOI `10.1016/j.cag.2020.05.012` | 检测到的曲线应成为显式网格约束，而不是停留为分离的采样点。 |
| 2024 | Xu et al., *CWF: Consolidating Weak Features in High-quality Mesh Simplification*, M026 | 弱证据在用于下游硬保护前需要整合和置信度。 |
| 2025 | Cai et al., *Feature Line Extraction Based on Winding Number*, DOI `10.1016/j.gmod.2025.101296` | 当局部微分证据碎片化时，全局曲线证据是有用的未来补充。 |

此实现不使用也不推荐神经或学习式方法。

## 测试与剩余限制

测试覆盖精确平面拒绝、平滑 bump 响应、统一尺度不变性、多尺度持久性、最终特征图阶段的噪声响应抑制、图来源
归属、参数校验以及现有硬特征回归套件。原始曲率候选仍是诊断输入；图约束决定哪些响应会成为可复用的特征边。
带标签的 Gaussian ridge/valley fixture 以单个局部边的曲线漂移容差测量精度和片段召回率，这与 remeshing 约束
消费离散近似曲线而非精确解析曲线的方式一致。

自 2026-07 起，该通道还使用解析真实值 fixture（`tests/support/AnalyticFixtures.{h,cpp}`、
`tests/unit/feature_detection/feature_detection_analytic_tests.cpp`）进行验证：球、圆柱和环面都是极值性恒等
为零的 Dupin cyclide，必须完全静默；Gaussian ridge sheet 用于检查 Monge 拟合的定量 crest 曲率精度
（`|f''(0)| = 2hs`，中位相对偏差上限 15%，由邻域平均偏差推导）。环面 fixture 暴露了上文通过 cyclideness
门控修复的虚假内侧 valley 带。

剩余限制：

- 每个顶点只存储一个主导平滑切线，因此非常紧凑的平滑多分支 junction 仍然难以处理；
- 检测器尚未执行全局 Hough 或 winding-number 曲线恢复；
- 尚未实现 split/collapse 后的增量邻域更新；
- 在默认启用该通道前，仍需要带标签 ridge/valley 曲线的扫描专用基准 fixture；
- 由于成本高度依赖请求的环数、尺度数量、鲁棒迭代次数和局部价数，该通道有意排除在强制 16k 面快速套件的
  墙钟保护之外。禁用的手动 Release 基准（`FeatureDetectionPerf.DISABLED_AnalyzeTiming`）在 2026-07-15
  开发机上的当前 8192 面 bump fixture 中测得三尺度/两次鲁棒迭代平滑阶段约 92 ms。该数字是本地观测值，
  不是 API 性能保证；强制测试仍在解析 fixture 上保持功能覆盖，并为默认 dihedral + normal-tensor 通道提供
  独立快速保护。

## 简化边界

`SimplifyOptions` 镜像平滑曲率控制项，包括稳定尺度选择和最小稳定性，以及 `featureGraphMinWeakSpurStrength`；
`featureOptionsFromSimplifyOptions` 在不改变阈值的情况下完成映射。C++ 调用方同时启用
`preserveFeatureCurves = true` 和 `useSmoothCurvatureFeatures = true`。在 `simplify` 中，CLI 会沿用 0.x 语义，在出现
`--smooth-curvature-features`、`--feature-normal-filter` 或 `--feature-graph-consolidation` 时自动打开 feature-curve policy；`feature-report` 则可以单独分析这些通道。`SimplifyReport`、简化标准输出/指标 CSV 和带大小信息的 C ABI 报告，携带平滑边、评分顶点
及持久性诊断，并与 winding、清理上限和圆形恢复诊断一起输出。

当检测结果在 repair/remeshing/validation 之间共享时，仍支持预计算 `FeatureAnalysis` 的重载。两条路径都汇合
到 `buildFeatureGuidanceFromAnalysis`；因此特征策略保持在拓扑编辑循环之外，符合 CGAL PMP 和 OpenMesh 所代表的
约束边/数据适配器实践。
