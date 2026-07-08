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
- 既有浅二面角 trace、untraced 诊断和 component-level circular fallback 回归测试继续保留。

## 文献对应

本次仍属于 deterministic CAD/STL + normal/tensor voting + feature-preserving QEM 路线。

- CAD/STL 特征线和 C1/C2 discontinuity：M007、M016、M018、M019。
- normal voting / normal tensor 弱特征证据：057、066、M012、M013、M015。
- 弱特征在简化前 consolidation 的必要性：088 / M026。
- QEM/line quadrics 和 edge-collapse 工程边界：082、085、087、M001-M004、M033。

这些论文给出的共同提示是：feature edge score 只是输入，真正能被简化器稳定消费的是带尺度、连续性、闭合性和 component confidence 的 feature support。因此本次优先修 trace、loop id、component fallback、局部尺度、多尺度 persistence 和诊断，而不是直接上 neural/wireframe。

## 仍待推进

1. Component-level confidence：为每个 trace connected component 计算强/弱证据比例、闭合率、junction 数、primitive residual 和 tensor persistence 均值，用于 fallback 排序和 QEM 策略。
2. Graph cleanup 与 gap closure：在 primitive 拟合前做短 gap 桥接、spurious spur 删除和 junction 合并，提高圆孔、椭圆孔和浅折线 loop 的闭合率。
3. 弱特征 consolidation：参考 CWF/M026，在 decimation 前把弱特征合并成可保护 support，再决定软成本、硬保护或 post-relocation。
4. 定量 benchmark：增加带 ground-truth edge labels 的 precision/recall、loop closure rate、junction correctness、弱特征保留率，以及简化前后 feature drift / Hausdorff envelope。
5. QEM 二阶段优化：在 benchmark 稳定后评估 two-round relocation/refinement，优先约束 protected support 的漂移和局部三角形质量。
6. Learned saliency / neural QEM：只在 deterministic baseline 对弱、浅、非均匀采样特征失效时作为对比项接入，且不得替代拓扑、法向、局部误差和 feature drift 硬过滤。
