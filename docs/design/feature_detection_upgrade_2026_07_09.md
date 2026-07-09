# 特征识别升级记录 2026-07-09

本文记录 2026-07-09 针对特征识别、QEM 特征保护和 common 几何查询的联动改动。目标是先把 CAD/STL 三角网格上的确定性 feature graph、弱特征 tensor 证据和简化器可消费诊断做稳，再推进 neural/wireframe 或更重的全局优化路线。

## 已完成改动

- `loopTraceAngleDeg` 独立于 `featureAngleDeg`：默认 `-1` 表示复用用户的二面角阈值，不再把 loop tracing 阈值硬抬到 40 度。`feature-report` 和 `simplify` 都会报告 `tracedFeatureEdges` / `untracedFeatureEdges`。
- primitive recovery 和 circular fallback 只在 primitive 验证通过后分配 loop id，避免无效 primitive 造成非连续 id，进而漏建简化约束。
- small cycle basis 和 circular vertex-cluster fallback 改成按 trace connected component 判定 tensor 影响；一个 normal-tensor ridge 不再全局阻塞其他干净圆孔的 fallback。
- common 层新增 `computeVertexAverageEdgeLength`，提供每个顶点的局部采样尺度；孤立顶点使用全局平均边长 fallback。
- normal tensor 平滑从简单一环平均改为按局部边长归一化的距离权重平滑，降低非均匀 STL tessellation 对弱特征分数的影响。
- normal tensor 多尺度结果增加 `averageFeatureScore`、`persistentScales`、`persistentFeatureScore` 和 `localScale`。
- `normalTensorMinPersistentScales` / `--normal-tensor-min-persistent-scales` 接入 FeatureOptions、SimplifyOptions、CLI、C ABI 和 VS Code 调试配置；默认 1，调试配置使用 2。
- QEM 的 `WeightMode::NormalTensor` 改用同一套 `persistentFeatureScore` 和最小 persistent scales 门槛，避免 feature detection 与 line-quadric 权重使用两套弱特征判据。
- `FeatureAnalysis`、`SimplifyReport`、C ABI report、feature-report CSV 和 simplify metrics CSV 增加 normal-tensor scored vertices、最大 persistent score、平均 local scale、平均 persistence 诊断。

## 新增保护测试

- `ManuMesh.MeshQueriesComputeLocalVertexEdgeScale`：保护 common 局部边长尺度和孤立点 fallback。
- `ManuMesh.NormalTensorReportsLocalScaleAndPersistentScore`：保护 normal tensor 的 local scale、persistence 和 persistent score 输出。
- `ManuMesh.NormalTensorAddsFeatureEdgesWhenDihedralThresholdIsStrict`：在二面角通道基本关闭时，验证多尺度 tensor 仍能补弱 ridge。
- `ManuMesh.NormalTensorWeightModeAppliesSpatiallyVaryingWeights`：保护 QEM normal-tensor weight mode 使用 persistent score 后仍产生空间变化权重。
- `CApiTest.ExposesNormalTensorOptionsAndDiagnostics`：保护 C ABI 能读写最小 persistent scales 和新增 tensor 诊断。
- `FeatureDetection.BenchmarksDetectedEdgesAgainstGroundTruthLabels`：保护 vertex-index edge-label benchmark 的 precision/recall/F1 计算。
- `ManuMesh.FeatureProtectionReportsComponentConfidenceDiagnostics`：保护 feature component confidence 进入 `SimplifyReport`。
- 既有浅二面角 trace、untraced 诊断和 component-level circular fallback 回归测试继续保留。

## 继续完成的算法改进

- Feature graph cleanup 已接入：`cleanupFeatureGraph` 默认开启，在 loop recovery 前删除短 tensor-only spur，并按局部平均边长桥接短 endpoint gap 和近 junction gap。CLI 参数为 `--feature-graph-gap-ratio`、`--feature-graph-max-weak-spur-edges` 和 `--no-feature-graph-cleanup`。
- Component-level confidence 已接入：`FeatureComponent` 汇总强/弱证据比例、闭合率、junction/endpoint、cycle rank、tensor persistence、primitive residual 和 confidence；loop/vertex 记录 component id、confidence 和 weak-feature 标记。
- QEM 联动已接入：feature-curve soft quadric 使用 `0.35 + 0.65 * confidence` 做温和缩放，强 CAD loop 接近原权重，弱证据 component 先作为软 support 消费。
- 定量 benchmark 已接入第一版：`feature-benchmark input.stl labels.csv` 和 `benchmarkFeatureEdges()` 支持 edge precision/recall/F1、junction correctness、loop closure rate 和 mean component confidence。
- 新诊断已贯通 `FeatureAnalysis`、`SimplifyReport`、C ABI、CLI stdout、feature-report CSV、simplify metrics CSV、gtest 和 VS Code demo/debug 配置。

## 文献对应

本次仍属于 deterministic CAD/STL + normal/tensor voting + feature-preserving QEM 路线。

- CAD/STL 特征线和 C1/C2 discontinuity：M007、M016、M018、M019。
- normal voting / normal tensor 弱特征证据：057、066、M012、M013、M015。
- 弱特征在简化前 consolidation 的必要性：088 / M026。
- QEM/line quadrics 和 edge-collapse 工程边界：082、085、087、M001-M004、M033。

这些论文给出的共同提示是：feature edge score 只是输入，真正能被简化器稳定消费的是带尺度、连续性、闭合性和 component confidence 的 feature support。因此本次优先修 trace、loop id、component fallback、局部尺度、多尺度 persistence 和诊断，而不是直接上 neural/wireframe。

## 仍待推进

1. 弱特征 consolidation：参考 CWF/M026，把多个低置信弱 component 合并成更稳定的 feature support，再决定软成本、硬保护或 post-relocation。
2. Benchmark 第二版：加入弱特征保留率、简化前后 feature drift / Hausdorff envelope，并把 `feature-benchmark` 的 label 格式扩展到 loop/junction/weak feature group。
3. QEM 二阶段优化：在 benchmark 稳定后评估 two-round relocation/refinement，优先约束 protected support 的漂移和局部三角形质量。
4. Edge dihedral plane quadrics 与 line weight 调度：比较 component confidence、dihedral plane quadrics、line quadrics 在浅特征/平面漂移上的收益。
5. Learned saliency / neural QEM：只在 deterministic baseline 对弱、浅、非均匀采样特征失效时作为对比项接入，且不得替代拓扑、法向、局部误差和 feature drift 硬过滤。
