# ManuMesh 算法本质、数学直觉与实现契约

本文描述 ManuMesh 当前程序真正做的事：它不是“一个 QEM 公式的翻译”，也不是完整 CAD/B-Rep kernel，而是一个面向增材制造三角网格的 **受约束边坍缩简化管线**。这份文档的目标是把现象、数学本质、论文来源、程序步骤和诊断字段放在同一张图里。

## 总体图景

ManuMesh 当前核心管线可以读成：

```text
三角网格 Mesh
  -> 构建局部几何与邻接
  -> 识别三角网格层的特征证据 FeatureAnalysis
  -> 累加 plane QEM / line quadrics / boundary quadrics / feature-curve quadrics
  -> 用优先队列按 quadric cost 选择候选边坍缩
  -> 对候选 placement 执行特征、边界、拓扑、法线、质量、局部误差、自交过滤
  -> 接受坍缩、更新动态拓扑、继续迭代
  -> 输出 Mesh / PlainMesh / C ABI 结果与 SimplifyReport、CSV 指标
```

对应源码主线：

| 步骤 | 源码 |
| --- | --- |
| 特征识别 | `src/feature_detection/FeatureDetector.cpp`、`FeatureEvidence.cpp`、`FeatureGraph.cpp`、`FeatureLoopRecovery.cpp`、`FeatureCycleRecovery.cpp`、`FeatureTraceRecovery.cpp`、`FeaturePrimitiveRecovery.cpp`、`FeatureLoopBuilder.cpp`、`FeatureCircularRecovery.cpp`、`NormalTensor.cpp`、`PrimitiveFit.cpp` |
| quadric 构造与 placement | `src/simplification/Quadrics.cpp` |
| 策略转换 | `src/simplification/SimplificationPolicies.cpp` |
| collapse 主循环 | `src/simplification/SimplificationRun.cpp` |
| 特征约束、曲线投影 | `src/simplification/FeatureConstraints.cpp` |
| 拓扑/质量/误差/自交过滤 | `src/simplification/CollapseLegality.cpp`、`GeometryPredicates.cpp`、`SpatialFaceIndex.cpp` |
| 结果压缩与报告 | `src/simplification/ResultBuilder.cpp`、`include/manumesh/algorithms/simplification/SimplificationTypes.h` |

从算法关系看，当前实现遵循的是“排序成本 + 语义支撑 + 硬过滤器”的工程结构。QEM 和 line quadrics 负责给候选排序；`FeatureAnalysis` 给出制造特征的三角网格支撑；硬过滤器负责阻止局部拓扑和几何灾难。三者不能互相替代。

## 现象一：标准 QEM 为什么在平面区域会退化

标准 QEM 来自 Garland-Heckbert 1997。对一个平面

```text
n^T x + d = 0
```

点 `x` 到该平面的平方距离可以写成齐次二次型：

```text
E_plane(x) = (n^T x + d)^2
           = [x, 1]^T (p p^T) [x, 1]
```

其中 `p = [n_x, n_y, n_z, d]^T`。一个顶点的 QEM 是相邻面的 plane quadric 累加。边坍缩时，把两个端点的 quadric 相加，求让总误差最小的新位置。

平坦区域的问题来自矩阵秩。若一片区域的三角面几乎共面，那么所有法向接近同一个 `n`，累加后的 `A` 矩阵主要只约束法向方向。点沿切平面方向移动时，`n^T x + d` 几乎不变，所以很多候选边的代价都接近零。

这会产生两个工程现象：

- 优先队列无法很好地区分平面内哪些边更该先坍缩。
- 最优 placement 线性系统病态，程序需要 fallback 到端点或中点。

当前代码中的落点：

- `planeQuadric()` 构造 `p p^T`。
- `computeInitialQuadrics()` 按三角形面积把 plane quadric 分配给三个顶点。
- `solvePlacementCandidates()` 先尝试解 `A x = -b`，若特征值条件太差，就保留端点/中点候选，并在 `solverFallbacks` 中记录退化现象。

这解释了为什么 `solver_fallbacks` 不是坏事本身，而是提示“局部 quadric 缺约束或数值退化”。

## 现象二：line quadrics 到底补了什么

Line quadrics 来自 Liu、Rahimzadeh、Zordan 2025。它的关键不是替代 QEM，而是在标准 QEM 的零空间里加入一个温和正则。

一条过点 `c`、方向为单位向量 `u` 的直线，点 `x` 到直线的平方距离为：

```text
E_line(x) = ||(I - u u^T)(x - c)||^2
```

这里 `I - u u^T` 是到直线正交补的投影矩阵。当前实现中，普通 line quadrics 用的是“过顶点、方向为面积加权顶点法向”的直线。也就是说，在平坦区，直线方向大致是法向，点到法向线的距离正好约束切平面内的移动。

把它和 plane QEM 放在一起看：

```text
E_total = E_plane + lambda * E_line
```

当 `lambda` 很小时，曲面贴合仍由 plane QEM 主导；只有在 plane QEM 不提供切向区分时，line quadrics 才显著改变排序。这就是它适合平坦区、近平坦区、采样退化区域的原因。

当前代码中的落点：

- `lineQuadric(point, normal)` 用两个与 `normal` 垂直的平面 quadric 相加，等价表达点到直线距离。
- `useLineQuadrics` 与 `lineWeight` 控制是否加入普通 line quadric。
- `weightMode`、`featureBoost` 控制 line weight 的空间变化。
- `minAppliedLineWeight`、`maxAppliedLineWeight` 写入 `SimplifyReport`，用于判断实际权重范围。

重要边界：

- line quadrics 不是去噪器；它不区分噪声和特征。
- 权重过高会把“均匀化/正则化”放在“贴近原始曲面”之上。
- 它改善候选排序，但不能保证拓扑、安全或制造语义。

## 现象三：为什么特征检测要成为平级模块

如果把特征保护直接写进 QEM 内部，短期看方便，长期会导致交叉引用：简化器、验证器、修复器、重网格器都会想要“特征”，但特征又被绑死在 QEM 里。当前 ManuMesh 把特征检测提升为 `manumesh::feature`，原因是特征图是一个可复用中间语义，不属于某一个简化算法。

`FeatureAnalysis` 当前包含四类信息：

| 信息 | 用途 |
| --- | --- |
| `FeatureGraph` | 边、顶点、junction、shared vertex，描述显式特征图结构。 |
| `FeatureLoop` | chain/loop、闭合性、primitive 类型、半径/轴比/拟合误差。 |
| `VertexFeature` | 每个顶点的 feature ownership、切向、圆/椭圆投影数据。 |
| 计数字段 | 诊断边来源：boundary、dihedral、normal-tensor、non-manifold 等。 |

当前检测器的证据来源：

1. boundary edge：只有一个相邻面。
2. non-manifold edge：多于两个相邻面。
3. dihedral edge：两个相邻面法向夹角超过阈值。
4. normal-tensor edge：张量特征分数和边方向对齐满足阈值。

内部实现按职责拆成小 pipeline：`FeatureEvidence.cpp` 组合多种 edge evidence strategy 并维护来源计数；`FeatureGraph.cpp` 构建 `FeatureGraph` 和 trace graph；`FeatureLoopRecovery.cpp` 只编排恢复顺序；`FeatureCycleRecovery.cpp` 恢复 junction cycle 和小 cycle basis；`FeatureTraceRecovery.cpp` 追踪图上的 open chain / closed loop；`FeaturePrimitiveRecovery.cpp` 处理 primitive component 兜底；`FeatureLoopBuilder.cpp` 负责 loop id、vertex ownership、切向和圆/椭圆投影数据写回；`FeatureCircularRecovery.cpp` 只处理有界三点圆扫描 fallback。随后程序恢复 junction 处可能断裂的 cycle，对闭合 loop 做 primitive fitting。圆、近圆、椭圆和折线 loop 会被写入 `FeatureLoop::primitive`。

算法出处与影响：

- CAD/STL 线特征、边界/面片边缘检测参考 `docs/papers/feature_detection/vidal_wolf_dupont_2011_robust_feature_line_extraction_cad_triangular_meshes.pdf`、`jiao_bayyana_2008_identification_c1_c2_discontinuities_surface_meshes_cad.pdf`。
- normal tensor 思路参考 `docs/papers/feature_detection/tsuchie_higashi_2014_normal_tensor_surface_feature_lines.pdf`，也和文献库 source 031、057、066、080 的 normal voting/tensor family 对齐。
- 复杂 CAD wireframe 和 junction 问题可继续参考文献库 source 026、063、067，但当前程序还不是通用 wireframe extractor。

## 现象四：normal tensor 的特征值在表达什么

当前 `computeNormalTensorFeatures()` 对每个顶点累加邻接面的法向外积：

```text
T = sum_i w_i n_i n_i^T
```

这个矩阵描述局部法向分布。若局部是一片光滑平面，法向集中，最大特征值显著，其他两个很小；若局部是折痕，法向主要落在两个方向形成的扇面里，第二个特征值变大；若局部像角点或噪声，三个方向都可能有能量。

当前代码将特征值按大到小理解为：

```text
l0 >= l1 >= l2
surfaceSaliency = l0 - l1
creaseSaliency  = l1 - l2
cornerSaliency  = l2
featureScore    = max(creaseSaliency, cornerSaliency)
```

这不是完整 tensor voting 论文系统，而是一个轻量局部法向张量。它适合给弱特征提供证据或空间变权，但受邻域、采样密度、噪声和 smoothing iteration 影响很大。

程序中的使用方式：

- 在 `FeatureEvidence.cpp` 中，normal tensor 只补充非 boundary、非 dihedral、非 non-manifold 的弱特征边。
- 在 `Quadrics.cpp` 中，`weightMode=normal-tensor` 把 `featureScore` 作为 line weight 的空间权重来源。

这解释了为什么 normal tensor 不应该被文档写成“完整特征恢复算法”。它当前更像一个弱证据通道。

## 现象五：primitive fitting 为什么重要

增材制造常见输入往往是 STL/OBJ 三角网格，没有 B-Rep 的圆孔、轴线、槽、面片边界语义。若只看二面角边，程序知道“这里有一圈边”，但不知道它应该是一条圆、一条椭圆，还是普通折线。

Primitive fitting 的作用是把离散 feature loop 提升为更可消费的曲线模型：

| primitive | 程序用途 |
| --- | --- |
| `Circle` / `NearCircle` | 圆孔、轴孔、法兰孔；可做圆投影和最低环顶点保护。 |
| `Ellipse` | 椭圆孔、斜切圆投影到网格后的椭圆环；可做椭圆投影。 |
| `PolygonalLoop` | 普通折线硬边；可在严格模式投影到折线段。 |

当前 `PrimitiveFit.cpp` 的步骤：

1. 对 loop 顶点做 PCA，拟合局部平面。
2. 在平面坐标里解最小二乘圆。
3. 从协方差估计 major/minor 轴半径。
4. 计算 radial、ellipse、plane 误差。
5. 依据相对误差和轴比分类为圆、近圆、椭圆或折线。

这种做法的本质是用低维解析模型解释离散边环。它不能恢复完整 CAD feature tree，但足以让简化器知道“这个 loop 应该像圆/椭圆一样被保护”。

## 现象六：为什么不能只靠成本函数保护特征

把特征写进 quadric 成本只能改变候选优先级，不能保证候选不会被执行。只要目标面数足够低，或者局部候选越来越少，软成本最终仍可能让重要特征被坍缩。

所以当前程序分成四层：

| 层 | 代表选项 | 类型 | 作用 |
| --- | --- | --- | --- |
| 特征邻域加权 | `weightMode`、`featureBoost` | 软 | 让特征附近候选成本更高。 |
| 曲线 quadric | `featureCurveWeight` | 软 | 让特征点靠近曲线局部模型。 |
| placement 投影和预算 | `maxFeatureCurveDeviationRatio` | 半硬 | 先限制原始 placement 偏离，再投影到圆/椭圆/折线。 |
| hard protection | `featureProtectionMode` | 硬 | 拒绝会破坏受保护 loop ownership、junction 或最低顶点数的 collapse。 |

默认 `primitive-curves` 只把圆、近圆和椭圆作为硬保护对象，generic crease 主要走软成本和合法性过滤。原因很实际：工业零件中大量普通折线硬边如果全部硬锁，候选空间会很快耗尽，最终 `termination_reason` 可能变成 `rejection-limit`。圆孔、椭圆槽、轴肩环这类 primitive 更值得硬保护。

`all-feature-edges` 是严格模式，不是默认工业推荐。

## 现象七：硬过滤器为什么是工业简化的核心

边坍缩的候选成本再低，也可能制造局部灾难：翻面、非流形、薄片折叠、自交、破坏边界、局部误差过大。生产型 decimator 通常都把 cost module 和 legality module 分开。当前实现也如此。

硬过滤器的本质：

| 过滤 | 报告字段 | 检查的数学/拓扑对象 |
| --- | --- | --- |
| feature policy | `feature_rejected_collapses` | feature ownership、loop id、junction、active loop vertex count。 |
| boundary policy | `boundary_rejected_collapses` | open boundary 的局部邻接和投影。 |
| link condition | `topology_rejected_collapses` | collapse 前后局部一环是否满足拓扑一致性。 |
| normal deviation | `normal_flip_rejected_collapses` | 旧三角形法向与新三角形法向点积是否低于阈值。 |
| triangle quality | `quality_rejected_collapses` | 新三角形质量是否低于 `minTriangleQuality`。 |
| local error | `error_rejected_collapses` | 旧局部采样点到新局部三角形集合的最大距离。 |
| local intersection | `self_intersection_rejected_collapses` | 新局部三角形是否和远处活动三角形相交。 |

这也解释了为什么 `SimplifyReport` 的拒绝计数很重要。它不是“失败日志”，而是参数反馈：如果 `generic_feature_rejected_collapses` 很高，说明可能锁边过度；如果 `quality_rejected_collapses` 很高，说明目标比例、质量阈值或输入三角形状态冲突；如果 `error_rejected_collapses` 很高，说明局部误差预算比目标面数更强。

算法出处：

- 边折叠、队列、placement 和 legality 的工程框架可参考 `docs/papers/edge_collapse/hoppe_1996_progressive_meshes.pdf`、`lindstrom_turk_1998_fast_memory_efficient_simplification.pdf`、`rose_2025_mesh_simplification_edge_collapse_guide.pdf`。
- 特征保持简化和 feature-sensitive metric 可参考 `docs/papers/feature_preserving_simplification/wang_2008_feature_sensitive_metric.pdf`、`hussain_2008_feature_preserving_mesh_simplification_vertex_cover.pdf`。
- 弱特征保护和先形成 feature support 的思路可参考文献库 source 088 `CWF: Consolidating Weak Features in High-quality Mesh Simplification`。

## 当前算法和论文的对应关系

| 论文/方向 | 文档位置 | 当前落地状态 |
| --- | --- | --- |
| Garland-Heckbert QEM 1997 | `docs/papers/qem/garland_heckbert_1997_surface_simplification_qem.pdf` | 已实现 plane quadric、vertex quadric 累加、edge collapse cost、placement solve/fallback。 |
| Garland-Heckbert 属性扩展 1998 | `docs/papers/qem/garland_heckbert_1998_color_texture_qem.pdf` | 作为“约束可并入 quadric”的思想参考；当前未实现颜色/UV 属性传播。 |
| Line Quadrics 2025 | `docs/papers/line_quadrics/liu_rahimzadeh_zordan_2025_line_quadrics.pdf` | 已实现普通 line quadric，并扩展到空间变权和 feature curve tangent quadric。 |
| CAD/STL feature line extraction | `docs/papers/feature_detection/*.pdf` | 已实现 boundary/dihedral/non-manifold/normal-tensor feature graph、loop tracing、primitive fitting 的工程子集。 |
| Feature-sensitive simplification | `docs/papers/feature_preserving_simplification/*.pdf` | 已实现特征软成本、primitive 硬保护、投影和拒绝计数；未实现完整 feature-sensitive metric 系列。 |
| Edge-collapse engineering | `docs/papers/edge_collapse/*.pdf` | 已实现队列、动态拓扑、placement fallback、若干 legality filters。 |
| Two-round QEM / post optimization | `docs/papers/qem/chang_2025_two_round_optimization_qem.pdf` | 当前未实现二轮优化，可作为后续质量修复方向。 |
| Neural / saliency QEM | `docs/papers/neural_and_temporal_qem/`、`feature_preserving_simplification/ha_2025...pdf` | 当前未实现学习模型。 |
| Temporal QEM | `docs/papers/neural_and_temporal_qem/yokota_2024_tracked_qem_temporal_consistency.pdf` | 当前未实现时间序列一致性。 |

## 参数理解顺序

调参时不要先问“哪个参数更大更好”，而要按约束层阅读：

1. `targetFaces` / `targetRatio`：你要求程序删到哪里。
2. `useLineQuadrics` / `lineWeight`：平坦区排序和 placement 正则化强度。
3. `weightMode` / `featureBoost`：哪些空间区域获得更高 line weight。
4. `preserveFeatureCurves`：是否先构建 `FeatureAnalysis` 并启用曲线保护层。
5. `featureProtectionMode`：哪些 feature 从软成本升级为硬拒绝。
6. `featureCurveWeight` / `maxFeatureCurveDeviationRatio`：曲线靠附和漂移预算。
7. `preserveBoundary` / `minTriangleQuality` / `maxNormalDeviationDeg` / `maxLocalErrorRatio` / `preventLocalIntersections`：几何安全闸。
8. `SimplifyReport` / metrics CSV：解释运行结果，而不是只看最后面数。

一个结果没有达到目标面数时，第一反应不应是“QEM 坏了”，而要看：

- `termination_reason` 是 `no-candidates` 还是 `rejection-limit`。
- 哪类 `*_rejected_collapses` 最高。
- `featureLoops`、`circularFeatureLoops` 是否符合预期。
- `minAppliedLineWeight`、`maxAppliedLineWeight` 是否因权重模式变得过强。
- 目标比例是否已经与质量/误差/特征保护冲突。

## 现有边界与未来扩展

当前没有承诺：

- 从任意 STL 自动恢复完整 CAD feature tree。
- 对高噪扫描输入自动完成去噪、法线重估和曲率 ridge 提取。
- 提供全局 Hausdorff/envelope 证明。
- 保证输出直接满足制造公差。
- 提供布尔、offset/thickening、修复、补洞或完整 manifold repair。

如果后续扩展，应尽量保持同样分层：

- `repair` 处理拓扑和几何修复，不反向依赖 simplification。
- `remesh` 消费 `FeatureAnalysis`，但不把 feature 检测复制一份。
- `boolean` 和 `offset` 需要自己的拓扑/容差内核，不能靠当前 edge collapse 硬凑。
- 全局 envelope / Hausdorff filter 应成为独立验证或 legality 层，而不是塞进 line weight。

## 文档维护规则

后续写文档时请保持这些判断：

- 讲 QEM 时必须区分“候选排序成本”和“硬合法性过滤”。
- 讲特征识别时必须区分“三角网格 feature graph”与“完整 CAD/B-Rep 语义”。
- 讲 line quadrics 时不要说它能去噪；它当前解决的是平坦区切向欠约束和候选排序退化。
- 讲 normal tensor 时不要写成万能特征恢复；它是弱特征证据和空间变权来源，受邻域、尺度和噪声影响。
- 讲工业安全时必须绑定具体过滤器、测试数据和报告字段。
- 新增能力应进入 `include/manumesh/algorithms/<domain>/` 下的平级模块，而不是反向塞进 simplification。
