# 当前算法管线

## 特征检测

`FeatureDetector` 将三角表面网格转换为可报告、可 benchmark、可分区并可被简化器消费的
`FeatureAnalysis`。当前阶段顺序由 `src/feature_detection/FeatureDetector.cpp` 编排：

1. 构建拓扑、面法向、局部尺度和证据缓存；
2. 可选 `FeatureNormalFilter` 稳定面法向（不改输入顶点）；
3. 收集 boundary、non-manifold、oriented dihedral、normal-tensor 和可选 smooth-curvature 证据；
4. 形成有来源标志的 feature graph；
5. 清理弱 spur、按兼容方向/来源整合分量和桥接小间隙；
6. 恢复 trace/cycle/loop，并在可行时拟合 circle、near-circle、ellipse 或 polygon；
7. 汇总 component confidence、junction branch pair 和恢复诊断；
8. 可选按真实 mesh-edge 屏障划分 surface patches。

normal tensor 是局部、多尺度、确定性的弱证据通道，不是通用曲率估计器。SmoothCurvature
则通过局部 quadric 拟合，在多个拓扑尺度上计算无量纲脊/谷分数、切线一致性和持久尺度，
再将通过阈值的候选映射到已有 mesh edge；该分数不承诺归一化到 `[0,1]`。两类弱证据都会
进入同一套 graph cleanup、trace/cycle/loop、component confidence 和 surface-patch 流程；
SmoothCurvature 默认关闭，`FeatureProfile::SmoothSurface` 会启用它并关闭 normal tensor。
它不是全局曲线重建，也不会凭空创建非输入边；Recovery bridge 可以出现在 graph 中，但不是
输入 mesh edge，不会自动变成 surface-patch 屏障。

## 简化

`QEMSimplifier` 的单次运行由以下职责组成：

1. `Quadrics.cpp` 累积面平面 quadric，并按配置加入 boundary、feature curve 和可选 line
   quadric；line quadric 是平坦区候选排序/placement 的正则项，不替代几何 QEM。
2. `Placement.cpp` 计算最优位置，并按秩提供端点/中点回退；边界 placement 遵守边界守恒策略。
3. `CandidateQueue.cpp` 按 cost 和稳定 tie-break 排序，版本号使拓扑变化后的旧候选失效。
4. `CollapseLegality.cpp`/`CollapseAttempt.cpp` 在提交前检查 link condition、boundary、
   feature curve、normal、triangle quality、local error、local intersection 和 texture。
5. `CollapseTopology.cpp` 更新动态邻接和活动面；`QualityRefinement.cpp` 可在固定拓扑上优化质量。

QEM/line quadrics 负责“选哪个候选、放在哪里”；硬过滤器负责“能不能改变拓扑”。因此输出
可能达不到比例或目标面数，`SimplifyReport::terminationReason` 和拒绝计数是预期诊断，不是
算法失效的单一证据。

特征指导复用 `FeatureAnalysis` 中的统一图和曲线切线。默认的 `PrimitiveCurves` 策略只硬保护
已拟合的圆、近圆和椭圆；smooth ridge/valley 候选通过统一特征图的曲线 line quadric
参与软约束，不会因为局部噪声或密集候选锁死整张图。需要严格保护所有检测到的
输入特征边时，应显式选择 `AllFeatureEdges`。

除 `normal-tensor` 外，简化器还提供独立的 `smooth-curvature` 权重模式。该模式消费
`FeatureAnalysis::smoothCurvatureVertexWeights` 中按持久尺度筛选的逐顶点分数，并将其用于
line-quadric 的特征增益；它与 SmoothCurvature 候选诱导的图约束和几何基元保护相互独立。`SmoothSurface`
profile 在简化入口中默认选择 `smooth-curvature`，显式 `weight-mode` 可以覆盖该 profile 值；
`normal-tensor` 权重模式仍只消费 normal-tensor 顶点权重。

## 纹理保护

纹理保护只在 `SimplifyConfig::texture.preserveTexture` 显式打开时启用。UV 采用逐面逐角
存储，局部 chart 配对、有符号 UV 面积和标量失真参与候选判断；几何 quadric 始终是 4x4，
placement 求解不扩维。CLI 当前不提供纹理开关，默认保持关闭。

## 分析与 I/O

`manumesh::analysis` 提供 `MeshStats` 和确定性双向采样距离；统计会跳过不可用面，不修改
输入。STL 导出要求严格有效三角形；OBJ 读取保留逐角 UV，凸面使用确定性 fan，凹面使用
投影 ear clipping，并拒绝重复、退化和自交 polygon。

## 明确限制

当前实现不承诺全局无自交、全局 Hausdorff/envelope 认证、制造公差合规、封闭修复、Boolean、
offset/thickening、体网格或完整 CAD 语义。任何更强的保证都必须先新增独立模块、契约和测试，
不能从现有局部过滤器推导出来。
