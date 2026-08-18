# 特征检测算法工程参考（当前运行时）

本文件只描述 ManuMesh 当前保留的确定性特征检测路径：boundary、non-manifold、
有向 dihedral、Normal Tensor 弱证据、FeatureGraph 清理与 primitive loop 拟合。
已移除的实验性曲面特征通道不属于当前 API、CLI、图模型或测试契约。

---

## 1. 硬证据优先

### 1.1 拓扑分类

- 只有一个相邻面的无向边是 boundary；超过两个相邻面的是 non-manifold；恰好两个相邻面的内部边才计算 dihedral。
- 拓扑证据先进入候选图，再由 trace/loop 阶段决定是否形成可保护曲线。不要把候选边数量当作 loop 数量。
- 退化面不贡献法向和 dihedral；boundary/non-manifold 仍按拓扑记录并在诊断中保留。

### 1.2 绕序协调和有向二面角

- 在共享边邻接图上先做确定性 BFS，给面分配翻转标记，使共享边方向尽量相反。
- 绕序一致时使用带符号法向点积计算完整 [0, 180] 度夹角，并用共享边方向计算凸/凹 signed kind。
- 绕序仍冲突时回退为无符号角，signed kind 置零，并增加 `inconsistentWindingEdges` 诊断。
- 角度阈值只控制 evidence 是否进入候选集；`loopTraceAngleDeg` 单独控制 trace eligibility。

### 1.3 失败模式

- 直接使用 `abs(dot)` 会折叠大于 90 度的折边，造成薄片和反折边漏检。
- 把退化面当作零法向参与点积会制造伪直角边。
- 只用全局角度阈值会对非均匀三角化和浅折痕敏感；弱证据应由 Normal Tensor 和图级验证补充。

---

## 2. Normal Tensor 弱证据

### 2.1 局部张量

- 对每个非退化面累加面积加权的 `n*n^T` 到三个顶点，再按面积归一化。
- 可选的一环平滑和多尺度推进都在共享缓存上执行；顶点对应关系不变，输出保留 `selectedScale`、`persistentScales`、`smoothingSteps` 和 `effectiveRadius`。
- 特征值谱给出 surface、crease、corner saliency；张量只产生候选支持，不直接升级为硬约束。

### 2.2 Edge materialization

- 候选内部边必须避开 boundary、dihedral 和 non-manifold 强证据；强特征 junction 允许弱边在一侧终止。
- 两个软端点都要满足持久尺度、persistent score、crease 优先和 edge/tangent alignment 门控。
- Normal Tensor 的 score、persistence 和 local scale 进入 `FeatureAnalysis`、组件 confidence 与 QEM 权重；不要在下游重新解释阈值。

### 2.3 参数和成本

- `normalTensorSmoothingIterations`、`normalTensorScaleCount` 和 `normalTensorMinPersistentScales` 有明确上限，越界立即拒绝。
- 邻接、局部平均边长、面法向和边信息由 `FeatureDetectionCache` 一次构建并在整条管线中复用。
- 高噪声输入优先启用独立 `FeatureNormalFilter`；过滤器只改分析缓存中的法向，不移动顶点或改变拓扑。

---

## 3. FeatureGraph 和恢复

### 3.1 证据与恢复边分离

- `FeatureGraphEdge` 的来源标志区分 boundary、dihedral、normal tensor、non-manifold 与 cleanup/consolidation bridge。
- bridge 不是原始 mesh edge 时只表达图连续性，不能直接当作几何投影线或面片 barrier。
- `featureEdges` 只统计原始证据边；`graph.edges` 可以额外包含恢复 bridge。

### 3.2 清理顺序

1. 删除受上限保护的短弱 spur，并重建 trace adjacency。
2. 按双端 continuation、source 和 signed kind 兼容规则合并近 junction。
3. 按局部平均边长、方向和一对一规则桥接 endpoint gap。
4. 可选地连接不同 component 的 degree-1 endpoint，并单独计数 consolidation bridge。

所有候选按固定键排序，端点或 junction 超过上限时跳过整步并写入诊断，避免组合爆炸。

### 3.3 Junction 和 patch

- junction 保存每个 incident branch 的 outward tangent、signed kind、continuation pair 和 ambiguous 标记。
- Surface patch 只把真实 mesh edge 当作 face-adjacency barrier；恢复 bridge 计入 ignored diagnostics，不切断面图。
- 组件 confidence 由强/弱证据比例、闭合率、持久性、junction/endpoint 和 primitive residual 共同决定。

---

## 4. Loop 和 primitive 拟合

- 先恢复 open chain/closed loop，再对共面闭环执行 circle、near-circle 或 ellipse 拟合；失败时保留 polygonal loop，而不是丢弃拓扑结果。
- 圆拟合使用 Taubin，病态输入确定性回退 Kåsa；椭圆使用 Halír-Flusser 直接最小二乘并检查椭圆约束。
- 拟合前用 PCA 建立局部平面并按 RMS 尺度归一化；拟合后报告 plane/radial/ellipse residual、closure 和 vertex-count 门槛。
- primitive 只是保护策略的输入，不等价于 B-Rep 或 CAD feature tree 语义。

---

## 5. 性能和确定性

1. 一次构建并复用 `FeatureDetectionCache`，禁止每个阶段重复扫描全网格。
2. Normal Tensor 多尺度使用 ping-pong 缓冲和固定顶点顺序，避免重复分配和非确定归约。
3. 便宜的拓扑/二面角判据全量运行，较贵的拟合、primitive 和恢复只在候选组件上运行。
4. unordered 容器只在内部使用；所有公开结果、CSV 和 benchmark 输出在出口按规范键排序。
5. 大网格优先采用分区 I/O、局部邻域缓存和受限诊断；跨分区 graph/halo 语义必须显式建模，不能隐式复制整网格。

---

## 6. 修改检查清单

- 新 evidence source 必须同时更新 C++ 结果类型、组件摘要、CLI/CSV、C ABI、简化消费和 benchmark。
- 新 bridge 必须声明是否为真实 mesh edge，并测试 patch segmentation 不会误切面。
- 任何新阈值都要做尺度不变性、空/退化输入、绕序冲突、并行确定性和 ABI 前缀测试。
- 诊断字段只描述当前运行时实际计算的量；删除的算法不得通过别名、profile、示例或生成文档重新出现。
