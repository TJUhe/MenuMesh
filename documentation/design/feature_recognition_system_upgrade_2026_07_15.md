# 特征识别系统增强说明（2026-07-15）

本文记录 ManuMesh 当前特征识别实现的系统性增强。描述以公开头文件、`src/feature_detection/`、CLI、size-aware C ABI 和测试为准；它不是未来路线图，也不把点云神经网络、B-Rep/CAD feature tree 或体网格能力写成已实现功能。

## 1. 目标与数据范围

当前目标是从三角/多边形表面网格中得到可被报告、benchmark、分区和 QEM 简化消费的显式特征结构：

- CAD/STL 硬边：boundary、non-manifold、signed dihedral。
- 弱 crease：normal-tensor 多尺度证据。
- 光滑 ridge/valley：opt-in 的多尺度 quadric curvature 证据。
- 曲线网络：endpoint、junction、逐分支切向、continuation pairing、component confidence。
- 下游结构：open chain、closed loop、circle、near-circle、ellipse、polygonal loop、surface patch adjacency。

默认配置继续偏向干净 CAD/STL。扫描类增强均为 opt-in，避免改变既有模型的检测和简化结果。

## 2. 当前管线

`FeatureDetectionPipeline::run()` 对非空网格执行九个显式阶段：

1. `EdgeEvidenceStage`：收集五类原始 evidence；需要法向时从 `FeatureDetectionCache` 取得原始或过滤后的面法向。
2. `DegenerateEvidenceFilterStage`：撤销接触退化面的伪 dihedral evidence。
3. `FeatureGraphStage`：建立完整 `FeatureGraph` 和活动 `TraceGraph`。
4. `FeatureGraphCleanupStage`：删除弱 spur，桥接兼容的 close junction 和 endpoint gap。
5. `FeatureGraphConsolidationStage`：opt-in 地恢复不同 component 之间的兼容短缺口。
6. `LoopRecoveryStage`：依次运行五类 bounded recovery，恢复 chain、loop 和 primitive。
7. `FeatureComponentSummaryStage`：计算 closure、evidence ratio、persistence、residual 和 confidence。
8. `FeatureGraphFinalizeStage`：生成 endpoint/junction/shared 标记、逐分支切向和 continuation pair。
9. `FeatureSegmentationStage`：opt-in 地用活动特征边分割 faces，输出 patch 及 patch adjacency。

法线过滤不是修改输入 mesh 的独立拓扑阶段。它在 evidence 计算前按需执行，只替换检测缓存中的面法向，顶点位置和面连接保持不变。

## 3. 新增与增强模块

| 模块 | 职责 | 关键约束 |
| --- | --- | --- |
| `FeatureNormalFilter.cpp` | area-weighted normal-domain relaxation | 保持顶点和拓扑；跨大角度边权重为零；输出迭代和角度变化诊断 |
| `FeatureGraphCompatibility.cpp` | cleanup/consolidation 共用的兼容规则 | 检查方向、强/弱 evidence source 和 ridge/valley 符号 |
| `FeatureGraphConsolidation.cpp` | component 级 gap recovery | 仅连接不同 component 的 endpoint；双端 continuation；一对一、确定性排序；512 endpoint cap |
| `FeatureGraph.cpp` | junction 分支语义 | 每条 incident branch 保存切向和符号；按对向 alignment 生成 continuation pair；报告 ambiguous junction |
| `FeatureSegmentation.cpp` | feature-induced face partition | 只把真实 mesh edge 当 barrier；非 mesh-edge recovery bridge 不切分 faces，并计入 ignored diagnostics |
| `FeatureBenchmark.cpp` | 可扩展 benchmark | edge、junction、branch-pair 和 face-patch adjacency 四类指标 |

`FeatureDetector.cpp` 只保留验证、上下文和阶段编排；算法细节下沉到职责单一的 translation unit，避免继续扩大单文件。

## 4. 法线域预处理

`FeatureNormalFilterOptions` 默认关闭：

| 字段 | 默认值 | 语义 |
| --- | ---: | --- |
| `enabled` | `false` | 是否启用 |
| `iterations` | `4` | relaxation 轮数，范围 `[0,16]` |
| `angleSigmaDeg` | `20` | 高斯角度带宽 |
| `preserveAngleDeg` | `50` | 大于该角度的相邻面不互相平滑 |
| `relaxation` | `0.8` | 原法向与邻域目标法向的混合系数 |

权重由相邻面法向夹角和面面积共同决定。该实现属于轻量、确定性的 feature-aware normal stabilization，不等同于完整扫描重建或顶点去噪；它对应文献中“先稳定法向，再分类 crease/junction”的路线（M009、M012、M013、M015），同时避免 vertex smoothing 的收缩问题。

诊断字段包括 completed iterations、changed faces、preserved edges、mean/max angular change 和 mean edge indicator。调用方应同时观察特征 recall 与角度变化，不能只以“更平滑”为成功标准。

## 5. 稳定尺度选择

smooth-curvature 原有多尺度 persistence 保留；新增 opt-in 的稳定尺度选择：

- `smoothCurvatureUseStableScaleSelection=false`：保持旧的 peak-score 参考尺度选择。
- `smoothCurvatureMinScaleStability=0.0`：默认不因稳定性拒绝 evidence。
- `SmoothCurvatureVertex::selectedScale`：实际参考尺度索引。
- `SmoothCurvatureVertex::scaleStability`：参考尺度与其他支持尺度在符号、切向和分数上的一致性。

这使“选中了哪个尺度、为什么接受”可诊断，而不是只暴露最终 score。该设计对应 multiscale crease/ridge 文献的尺度稳定性原则（M009、M011、M014、M021、M042-M044）。

## 6. 图兼容、consolidation 与 junction

cleanup 的 endpoint gap 和 close-junction bridge 现在共享兼容规则：

- 连接方向必须延续两端已有分支。
- 两侧已知 ridge/valley 符号相反时拒绝。
- strong evidence 只有存在共同 source 时兼容。
- weak evidence 可在 normal-tensor 与 smooth-curvature 之间兼容。
- recovery bridge 不伪装成 raw evidence。

close-junction 不再是纯距离判断；它使用双端 continuation、source 和 signed-kind 复核。仍保留局部贪心和一对一使用约束，因此密集、近距离的多网络场景仍需 benchmark 和可视化审查。

component consolidation 在 cleanup 之后、loop recovery 之前运行。它只连接不同 component 的 degree-1 endpoint，候选按归一化距离与 alignment penalty 确定性排序，并通过 `consolidationBridge` 与 `graphConsolidationBridges` 单独报告。该实现吸收了 M026“先整合弱支持再保护”的思想，但没有声称复现论文的完整重定位/优化方法。

junction 不再只有一个聚合切向。`FeatureGraphVertex` 现在保存：

- `branches`：edge id、neighbor vertex、outward tangent、signed kind。
- `branchPairs`：最优 continuation pair 和 alignment。
- `ambiguousJunction`：仍有未配对分支的 junction。

这为复杂曲线网络提供了可测语义，也让 benchmark 可以区分“junction 顶点找到了”和“分支连通关系找对了”。

## 7. Surface patch segmentation

`SurfacePatchOptions` 默认关闭。启用后，以活动 feature graph 中的真实 mesh edge 为 barrier，对 face adjacency 做 flood fill：

- strong barrier：boundary、dihedral、non-manifold。
- weak barrier：normal-tensor、smooth-curvature；可用 `includeWeakEvidence=false` 排除。
- cleanup/consolidation 产生但不属于原 mesh 的 bridge 不作为 barrier，避免人为线段切开面片。

输出包括 `facePatchIds`、`FeaturePatch`、`FeaturePatchAdjacency`、closed patch count 和 ignored recovery edge count。当前实现是 feature-induced connectivity partition，不包含 analytic primitive fitting、region merge 或 CAD surface classification；M024/M025 是下一步 patch 语义增强的文献锚点。

## 8. API、CLI 与 ABI

公开 C++ API 新增嵌套 options、normal filter result、junction branch、patch 和扩展 benchmark 类型。CLI 新增：

```text
--smooth-curvature-stable-scale
--smooth-curvature-min-scale-stability S
--feature-normal-filter
--feature-normal-filter-iterations N
--feature-normal-filter-angle-sigma-deg A
--feature-normal-filter-preserve-angle-deg A
--feature-normal-filter-relaxation R
--feature-graph-consolidation
--feature-graph-consolidation-gap-ratio R
--feature-graph-consolidation-alignment A
--surface-patches
--surface-patches-strong-only
```

`simplify` 暴露法线过滤、稳定尺度和 consolidation，并在相应开关出现时启用 feature-curve policy；surface patch 只属于 feature-analysis 命令，不参与 QEM collapse 决策。

C ABI 只在结构体尾部追加字段，继续依赖 `struct_size`/`abi_version` 和 size-aware 写回。旧调用方提供较小结构体时不会被越界写入。

## 9. Benchmark 标签与指标

`feature-benchmark` 标签支持：

```csv
edge,a,b
junction,vertex_id
branch,junction_vertex,neighbor_a,neighbor_b
face_patch,face_id,patch_id
```

为兼容既有数据，`a,b` 仍等价于 `edge,a,b`。输出在原 edge/junction precision、recall、F1、loop closure 和 component confidence 之外，新增 branch-pair precision/recall/F1 与 face-patch adjacency accuracy。

patch benchmark 比较已标注相邻 faces 是否属于同一 patch，而不是直接比较任意 patch id；因此预测 patch id 可重新编号而不影响结果。
缺失或无效的预测 patch id 计为错误，不能因为真值恰好属于不同 patch 而获得正确计数。

## 10. 验证覆盖

`tests/unit/feature_detection/feature_detection_pipeline_upgrade_tests.cpp` 覆盖：

- noisy normals 被稳定，同时圆柱 rim 保持。
- stable scale selection 输出可解释尺度与稳定性。
- compatible components 可 consolidation。
- 相反 ridge/valley 符号禁止 consolidation。
- junction continuation branch pairing。
- junction branch 只配对从 junction 向相反方向延续的分支，不把同侧近平行分支误判为 continuation。
- 圆柱侧壁与两个端盖得到三 patch。
- 缺失 patch prediction 不会得到虚假的 adjacency 正确计数。
- 新选项的非法范围被拒绝。

此外，C ABI diagnostics/size compatibility、CLI CSV header/row 列数和既有 feature detection 回归均需一起运行。新增能力默认关闭时，既有 CAD/STL 结果应保持兼容。

## 11. 文献与开源实现对照

当前实现选择确定性几何基线：

- normal/tensor 与 noisy crease：M009、M012、M013、M015。
- curvature/ridge/valley 与 curve network：M011、M014、M021、M042-M044。
- CAD/STL graph continuity：M007、M016、M018、M019。
- weak feature consolidation：M026。
- engineering patch segmentation：M024、M025。

开源程序主要作为实现边界和测试 oracle，而不是复制 API：OpenMesh（halfedge/property/policy 分离）、CGAL PMP（显式 constrained edges）、pmp-library/libigl（curvature 与局部邻域）、MeshLib/L0Denoising/NLLR/LSD（法线域或 feature-preserving denoising 对照）。

## 12. 当前仍未实现

- 完整 variational、L0、non-local low-rank 或 segmentation-driven vertex denoising/reconstruction。
- 全局 Hough 或 winding-number feature curve recovery。
- analytic surface fitting、patch merge 和 CAD surface type classification。
- junction 的全局最优配对或 direction-field wireframe solver。
- CWF 的完整 weak-feature relocation/optimization 闭环。
- 学习式 edge/wireframe/QEM；这些只作为对照或离线研究方向。
- B-Rep、solid、CAD feature tree、Boolean 和 volumetric meshing。

## 13. 维护规则

- 新 evidence source 必须同时更新 `FeatureGraphEdge`、component summary、CLI/CSV、C ABI 和 benchmark。
- 新 recovery edge 必须声明是否为真实 mesh edge，避免 patch segmentation 误用。
- cleanup 与 consolidation 必须复用 compatibility helper，不得各自维护互相漂移的 source/sign 规则。
- 新 options 必须在 C++ validation、CLI binding、simplify mapping、C ABI size tests 和文档中保持同名语义。
- `documentation/archive/` 和论文 PDF 是历史/外部资料，不随当前源码回写；当前契约以本页、交付开发者指南和 feature-recognition pipeline HTML 为准。
