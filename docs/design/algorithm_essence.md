# ManuMesh 算法本质、数学直觉与实现契约

本文描述 ManuMesh 当前程序真正做的事：它不是“一个 QEM 公式的翻译”，也不是完整 CAD/B-Rep kernel，而是一个面向增材制造三角网格的 **受约束边坍缩简化管线**。这份文档的目标是把现象、数学本质、论文来源、程序步骤和诊断字段放在同一张图里。

## 总体图景

ManuMesh 当前核心管线可以读成：

```text
三角网格 Mesh（可携带逐角 UV faceTexCoords）
  -> 构建局部几何与邻接
  -> 识别三角网格层的特征证据 FeatureAnalysis
  -> 累加 plane QEM / line quadrics / boundary quadrics / feature-curve quadrics
  -> 用优先队列按 quadric cost 选择候选边坍缩（opt-in：叠加局部 UV 失真标量代价）
  -> 对候选 placement 执行特征、边界、拓扑、法线、质量、局部误差、自交过滤（opt-in：UV chart 与有符号 UV 面积过滤）
  -> 接受坍缩、更新动态拓扑、继续迭代
  -> 输出 Mesh / PlainMesh / C ABI 结果与 SimplifyReport、CSV 指标
```

对应源码主线：

| 步骤 | 源码 |
| --- | --- |
| 特征识别 | `src/feature_detection/FeatureDetector.cpp`、`FeatureEvidence.cpp`、`SmoothCurvature.cpp`、`FeatureGraph.cpp`、`FeatureLoopRecovery.cpp`、`FeatureCycleRecovery.cpp`、`FeatureTraceRecovery.cpp`、`FeaturePrimitiveRecovery.cpp`、`FeatureLoopBuilder.cpp`、`FeatureCircularRecovery.cpp`、`NormalTensor.cpp`、`PrimitiveFit.cpp` |
| quadric 构造与 placement | `src/simplification/Quadrics.cpp`、`Placement.cpp`（Lindstrom-Turk 边界守恒 placement） |
| 策略转换 | `src/simplification/SimplificationPolicies.cpp` |
| collapse 主循环 | `src/simplification/SimplificationRun.cpp`、`CollapseAttempt.cpp` |
| 特征约束、曲线投影 | `src/simplification/FeatureConstraints.cpp` |
| 纹理感知排序与 UV chart 保护 | `src/simplification/TextureProtection.cpp`、`detail/TextureProtection.h` |
| 拓扑/质量/误差/自交过滤 | `src/simplification/CollapseAttempt.cpp`、`CollapseTopology.cpp`、`CollapseLegality.cpp`、`SpatialFaceIndex.cpp`、`src/common/GeometryPredicates.cpp`、`src/common/MeshDistanceIndex.cpp`、`src/common/SpatialIndex.cpp` |
| 结果压缩与报告 | `src/mesh_edit/MeshCompaction.cpp`、`include/algorithms/simplification/SimplificationTypes.h` |

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
- `solvePlacementCandidates()` 实现 GH97 三级回退链：先尝试解 `A x = -b`；若特征值条件太差，退到**沿坍缩边的一维最优**（对 `h(t) = a + t(b-a)` 的标量二次问题，rank-2 quadric——直棱、边界折痕——的良定情形，分母判据用尺度不变的相对阈值）；一维也退化时才只保留端点/中点候选，并在 `solverFallbacks` 中记录退化现象。

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
- `weightMode`、`featureBoost` 控制 line weight 的空间变化。`adaptiveScale` 模式下采用 Wang 2008 的优先级解耦：`featureBoost` 不再放大 quadric 本身（旧行为会扭曲 placement 并抬高边界项），而是变成逐顶点队列优先级因子 `priorityScale = 1 + featureBoost * score`，只乘候选排序代价（`CandidateQueue` 取两端点最大值），placement 用干净的 `adaptiveBaseLineWeight` 基础 line quadric 求解。
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
| 计数字段 | 诊断边来源：boundary、dihedral、normal-tensor、non-manifold、traced/untraced 和 tensor local-scale/persistence 等。 |

当前检测器的证据来源：

1. boundary edge：只有一个相邻面。
2. non-manifold edge：多于两个相邻面。
3. dihedral edge：两个相邻面的**有向二面角**超过阈值。检测器先按共享边在两面中的遍历方向做绕向一致性判断：绕向一致时用带符号法向点积（可区分浅折痕与 >90° 的反折刀边，旧的 `|dot|` 会把 120° 法向夹角读成 60° 而漏检）；绕向不一致的边回退无符号角，并计入 `FeatureAnalysis::inconsistentWindingEdges` 诊断。
4. normal-tensor edge：张量 persistent feature score、最小支持尺度数和边方向对齐满足阈值。
5. smooth-curvature edge（opt-in，`useSmoothCurvatureFeatures`）：多尺度三次 Monge 拟合产生的确定性 ridge/valley 证据，两端点在符号、切向、尺度支持和边对齐上一致时才转成 edge 证据（见现象四之二）。

其中 1–3 是离散网格上的“硬证据”（不连续现象），4–5 是光滑表面上的“弱证据”（微分现象）。两路只在显式 `FeatureGraph` 汇合；component 置信度把 normal-tensor 与 smooth-curvature 视为相互独立的弱支持，硬证据始终占主导。

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
persistentFeatureScore = f(featureScore, averageFeatureScore, persistentScales)
```

这不是完整 tensor voting 论文系统，而是一个轻量局部法向张量。当前实现会用 common 层局部平均边长做距离权重平滑；只有单尺度 saliency 达到 `normalTensorFeatureThreshold` 时，该尺度才计入 `persistentScales`，`persistentFeatureScore` 再按有效尺度比例衰减。它适合给弱特征提供证据或空间变权，但仍受邻域、采样密度、噪声和 smoothing iteration 影响很大。

程序中的使用方式：

- 在 `FeatureEvidence.cpp` 中，normal tensor 只补充非 boundary、非 dihedral、非 non-manifold 的弱特征边。
- 在 `Quadrics.cpp` 中，`weightMode=normal-tensor` 把 `persistentFeatureScore` 作为 line weight 的空间权重来源，并受 `normalTensorMinPersistentScales` 约束。

这解释了为什么 normal tensor 不应该被文档写成“完整特征恢复算法”。它当前更像一个弱证据通道。

## 现象四之二：光滑曲率证据在表达什么

二面角和 normal tensor 都依赖相邻面法向的离散差异，对光滑过渡的 ridge/valley（例如 fillet 中心线、扫描件上的平缓折痕）响应很弱。2026-07-11 落地的确定性光滑曲率通道（`src/feature_detection/SmoothCurvature.cpp`，设计见 [`smooth_curvature_feature_detection_2026_07_11.md`](smooth_curvature_feature_detection_2026_07_11.md)）把这类微分事件补成独立弱证据，opt-in 开关是 `FeatureOptions::useSmoothCurvatureFeatures`（默认 `false`）。

对每个顶点、每个拓扑尺度，算法执行：

1. 构建 k-ring 邻域和面积加权局部法线（邻接、边信息、局部平均边长由 `FeatureDetectionCache` 全管线构建一次、传引用复用）；
2. 用局部平均边长 × ring 数做坐标归一化；
3. 带距离权与确定性 Huber 重加权拟合**三次 Monge patch** `w = a u² + b uv + c v² + d u + e v + c₀u³ + c₁u²v + c₂uv² + c₃v³`（9 未知量，增量累加加权正规方程求解，少于 9 个可用邻居判为欠定拒绝）；
4. 由二次块的第一/第二基本形式解广义自伴特征问题，得到两条带符号主曲率和方向；
5. 由三次块**解析求出 extremality** `e_i = ∇κ_i · t_i`（三阶方向型 `e ∝ c₀t₁³ + c₁t₁²t₂ + c₂t₁t₂² + c₃t₂³`），不再对邻居曲率做差分；
6. 用 Ohtake 边零交叉判据分类 ridge/valley：主方向是 line field，先对邻居的切向与 extremality 做符号同步，再要求（a）边两端都通过曲率支配性测试（ridge 要求 `κ_max > |κ_min|`，valley 对偶），（b）extremality 沿近似跟随主方向的入射边变号，（c）两端一阶极大测试成立；零交叉点用反比插值归属到 |e| 较小的端点，使检测带保持一个顶点宽；
7. 打分融合尺度归一化曲率幅值、各向异性、零交叉强度（|e| 均值乘切向边跨度）和拟合残差质量；
8. 以最佳尺度为参照，在全部请求尺度中统计“分数达到绝对阈值与最佳分数 30% 中较大者、符号一致、曲线切向一致”的支持票数；当前实现是纯支持尺度计票，不要求支持尺度相邻，也不要求最粗尺度必须支持；
9. 两端点在符号、切向、尺度支持和边对齐上一致时，才把顶点证据转成 mesh-edge 证据。

分数无量纲，网格均匀缩放不需要重新调曲率阈值。默认 `smoothCurvatureBaseNeighborhoodRings = 2`，因为 one-ring 拟合对噪声过敏。该路径 opt-in 的原因是 CAD/STL 硬边与扫描/自由曲面两种场景需要不同阈值和验证集；不启用时既有硬特征行为完全不变。诊断字段包括 `FeatureAnalysis::smoothCurvatureFeatureEdges`、`smoothCurvatureScoredVertices`、`maxSmoothCurvatureFeatureScore`、`maxSmoothCurvaturePersistentScore`、`meanSmoothCurvatureLocalScale`、`meanSmoothCurvaturePersistence`，graph edge 与 component 分别记录 `smoothCurvature` 来源和 `smoothCurvatureEdges`、`meanCurvaturePersistence`。整条链路是确定性数值几何，不含任何神经/学习成分。

## 现象五：primitive fitting 为什么重要

增材制造常见输入往往是 STL/OBJ 三角网格，没有 B-Rep 的圆孔、轴线、槽、面片边界语义。若只看二面角边，程序知道“这里有一圈边”，但不知道它应该是一条圆、一条椭圆，还是普通折线。

Primitive fitting 的作用是把离散 feature loop 提升为更可消费的曲线模型：

| primitive | 程序用途 |
| --- | --- |
| `Circle` / `NearCircle` | 圆孔、轴孔、法兰孔；可做圆投影和最低环顶点保护。 |
| `Ellipse` | 椭圆孔、斜切圆投影到网格后的椭圆环；可做椭圆投影。 |
| `PolygonalLoop` | 普通折线硬边；可在严格模式投影到折线段。 |

当前 `PrimitiveFit.cpp` 的步骤：

1. 对 loop 顶点做 PCA，拟合局部平面（PCA 只用于平面法向估计，不再决定椭圆轴向）。
2. 在平面坐标里用 **Taubin 代数拟合**解圆（一阶无偏，部分弧/噪声下不再像旧 Kåsa 正规方程那样系统性低估半径；Taubin 特征分解失败时回退 Kåsa 作确定性兜底）。
3. 用 **Halíř-Flusser 直接最小二乘椭圆拟合**求 major/minor 轴（Fitzgibbon 约束 `4ac − b² = 1` 的 3×3 缩减系统，保证输出为椭圆；轴向来自 conic 转角而非 PCA/二阶矩）。
4. 计算 radial、ellipse、plane 误差。
5. 依据相对误差和轴比分类为圆、近圆、椭圆或折线。

这种做法的本质是用低维解析模型解释离散边环。它不能恢复完整 CAD feature tree，但足以让简化器知道“这个 loop 应该像圆/椭圆一样被保护”。

## 现象六：为什么不能只靠成本函数保护特征

把特征写进 quadric 成本只能改变候选优先级，不能保证候选不会被执行。只要目标面数足够低，或者局部候选越来越少，软成本最终仍可能让重要特征被坍缩。

所以当前程序分成四层：

| 层 | 代表选项 | 类型 | 作用 |
| --- | --- | --- | --- |
| 特征邻域加权 | `weightMode`、`featureBoost` | 软 | 让特征附近候选成本更高。 |
| component confidence | `FeatureComponent`、`meanFeatureComponentConfidence` | 软诊断 | 把强/弱证据、闭合率、junction、primitive residual 和 tensor persistence 汇总成 support 可信度。 |
| 曲线 quadric | `featureCurveWeight` | 软 | 让特征点靠近曲线局部模型，并按 component confidence 温和缩放。 |
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
| boundary policy | `boundary_rejected_collapses` | open boundary 的局部邻接；边界边坍缩的 placement 使用 Lindstrom-Turk 边界守恒约束（投影到最小化边界有向面积变化的直线，见 `detail/Placement.{h,cpp}`）。 |
| link condition | `topology_rejected_collapses` | 按单纯复形检查 `Lk(u) ∩ Lk(v) = Lk(uv)`：不仅比较共同邻点，也比较 vertex link 中的共同对边，因此四面体边这类会生成重复面的坍缩会被拒绝；另含"虚拟顶点"边界扩展判据，拒绝两端都在开边界上的内部弦，以及会让二维分量降维消失的 isolated open triangle。上述判据都与 `preserveBoundary` 无关、始终生效。 |
| normal deviation | `normal_flip_rejected_collapses` | 旧三角形法向与新三角形法向点积是否低于阈值。 |
| triangle quality | `quality_rejected_collapses` | 新三角形质量是否低于 `minTriangleQuality`。 |
| local error | `error_rejected_collapses` | 旧局部采样点到新局部三角形集合的最大距离。 |
| local intersection | `self_intersection_rejected_collapses` | 新局部三角形是否和远处活动三角形相交；相交谓词使用无量纲相对容差 `kRelativeIntersectionEps = 1e-9`，网格均匀缩放不改变判定。 |
| texture policy（opt-in） | `SimplifyReport::textureRejectedCollapses`、`textureProtectedEdges`（仅 C++ 报告，CLI metrics CSV 未包含） | 坍缩两端点的局部 UV chart 能否一一配对、存活 UV 三角形是否翻转定向或有符号面积低于 `minTextureAreaRatio`。仅在 `preserveTexture=true` 且输入带 UV 时生效。 |

这也解释了为什么 `SimplifyReport` 的拒绝计数很重要。它不是“失败日志”，而是参数反馈：如果 `generic_feature_rejected_collapses` 很高，说明可能锁边过度；如果 `quality_rejected_collapses` 很高，说明目标比例、质量阈值或输入三角形状态冲突；如果 `error_rejected_collapses` 很高，说明局部误差预算比目标面数更强。

当前实现中，`CollapseAttempt.cpp` 是这些硬过滤器的组合点：它先询问 feature policy 和 boundary policy，再对候选 placement 执行曲线预算、投影和 legality checks，最后返回统一的接受/拒绝结果。`SimplificationRun.cpp` 只根据结果应用 collapse 或记录报告计数。

算法出处：

- 边折叠、队列、placement 和 legality 的工程框架可参考 `docs/papers/edge_collapse/hoppe_1996_progressive_meshes.pdf`、`lindstrom_turk_1998_fast_memory_efficient_simplification.pdf`、`rose_2025_mesh_simplification_edge_collapse_guide.pdf`。
- 特征保持简化和 feature-sensitive metric 可参考 `docs/papers/feature_preserving_simplification/wang_2008_feature_sensitive_metric.pdf`、`hussain_2008_feature_preserving_mesh_simplification_vertex_cover.pdf`。
- 弱特征保护和先形成 feature support 的思路可参考文献库 source 088 `CWF: Consolidating Weak Features in High-quality Mesh Simplification`。

## 现象七之二：纹理感知简化为什么不给 quadric 扩维

Garland-Heckbert 1998 的属性 QEM 把颜色/UV 追加进齐次向量，让 quadric 矩阵随属性维度增长。ManuMesh 采用现代 edge-collapse 管线文献（M033）建议的工程拆分：几何 quadric 保持 `Mat4`（齐次 `(x, y, z, 1)`），placement 候选仍是端点、中点和稳定 3D QEM 最优点；纹理只以两种显式局部策略参与（实现在 `src/simplification/TextureProtection.cpp`，权威设计见 [`texture_aware_qem.md`](texture_aware_qem.md)）：

1. **排序代价**：`E_total(p) = E_geometry_4x4(p) + textureWeight * E_uv_local(p)`。其中 `E_uv_local = edgeLength² * Σ(faceArea * cornerUvDisplacement²) / meanLocalUvEdgeLength²` 只在坍缩触及的面上计算，与面积加权几何 QEM 同长度量纲，并对 UV atlas 均匀缩放不变。`textureWeight` 只缩放这个标量，绝不改变 quadric 维度或 placement 求解。
2. **硬过滤**：对坍缩两端点用容差网格哈希（`textureSeamTolerance`）把入射角 UV 分组成局部 chart，由坍缩边入射面建立两端 chart 的一一配对。chart 无对应、配对歧义、合并不相关 chart、存活 UV 三角形定向翻转或有符号面积低于 `minTextureAreaRatio` 时拒绝坍缩。双侧 seam 上两侧 chart 都一致配对时允许坍缩；跨 seam 合并 chart 归属的坍缩被过滤。

数据模型上，UV 存储为 `Mesh::faceTexCoords` 的“角拥有”逐面逐角坐标（一个几何顶点可属于多个 UV chart，接缝才可表达），OBJ 读取按逐角 `vt` 索引保留。整套检查是局部 O(k)（k 为 one-ring 规模），无全局参数化或属性空间矩阵分解，edge-collapse 渐近复杂度不变。

`preserveTexture` 默认 `false`：关闭时几何输出与旧无纹理路径完全一致（bit-exact），UV 仍会传播但无失真/接缝保证。启用纹理保护时，可选的固定拓扑质量精修轮会被暂时跳过，因为该顶点重定位阶段尚未约束 UV 失真。实现上纹理工作分三段：`evaluate()` 只为排序/否决打分、不物化 UV 重写；被接受的 placement 由 `buildPlan()` 构建一次 `TextureUpdatePlan`（具体的逐面角 UV 重写），`apply()` 直接应用，避免 `applyCollapse` 内重建同一计划。诊断字段为 `textureProtectedEdges`（初始即无合法中点纹理坍缩的边数）、`textureRejectedCollapses`（placement 评估后被纹理检查否决的队列候选数）和 `textureApplyFailures`（已接受坍缩的预建计划无法重放的内部一致性计数，应保持为零）。该能力目前只在 C++ `SimplifyOptions` 暴露，CLI `simplify` 未提供纹理选项。

## 当前算法和论文的对应关系

| 论文/方向 | 文档位置 | 当前落地状态 |
| --- | --- | --- |
| Garland-Heckbert QEM 1997 | `docs/papers/qem/garland_heckbert_1997_surface_simplification_qem.pdf` | 已实现 plane quadric、vertex quadric 累加、edge collapse cost、placement solve/fallback。 |
| Garland-Heckbert 属性扩展 1998 | `docs/papers/qem/garland_heckbert_1998_color_texture_qem.pdf`（M003） | 作为历史参照保留。ManuMesh 未采用属性扩维路线，而是按 M033 的工程拆分实现了 opt-in 纹理感知简化：4×4 几何 quadric + 局部标量 UV 失真代价 + chart/面积硬过滤，见 [`texture_aware_qem.md`](texture_aware_qem.md)。 |
| Line Quadrics 2025 | `docs/papers/line_quadrics/liu_rahimzadeh_zordan_2025_line_quadrics.pdf` | 已实现普通 line quadric，并扩展到空间变权和 feature curve tangent quadric。 |
| CAD/STL feature line extraction | `docs/papers/feature_detection/*.pdf` | 已实现 boundary/dihedral/non-manifold/normal-tensor feature graph、loop tracing、primitive fitting 的工程子集。 |
| 确定性光滑曲率特征检测（2017–2025） | `docs/papers/recent_deterministic_feature_detection_2026-07-11.md`（Yamakawa-Shimada 2017/2018、Lu 2019/M044、Romanengo 2020、Xu 2024 CWF/M026、Cai 2025） | 已实现 opt-in 多尺度鲁棒 quadric 拟合、带符号方向极值和 persistence 的 smooth-curvature 证据通道（`src/feature_detection/SmoothCurvature.cpp`），见 [`smooth_curvature_feature_detection_2026_07_11.md`](smooth_curvature_feature_detection_2026_07_11.md)；全局 Hough/winding-number 曲线恢复仍是路线图项。 |
| Feature-sensitive simplification | `docs/papers/feature_preserving_simplification/*.pdf` | 已实现特征软成本、primitive 硬保护、投影和拒绝计数；未实现完整 feature-sensitive metric 系列。 |
| Edge-collapse engineering | `docs/papers/edge_collapse/*.pdf` | 已实现队列、动态拓扑、placement fallback、若干 legality filters。 |
| Two-round QEM / post optimization | `docs/papers/qem/chang_2025_two_round_optimization_qem.pdf` | 已实现可选的固定拓扑切向 refinement：回溯线搜索只接受局部最差质量提升且平均质量不下降的移动，并复用边界、硬特征、法向、误差包络和自交约束。 |
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
8. `qualityRefinementIterations`：edge collapse 后的固定拓扑质量优化轮数；默认 `0` 保持单轮行为。启用纹理保护时该阶段暂时跳过。
9. `preserveTexture` / `textureWeight` / `textureSeamTolerance` / `minTextureAreaRatio`：输入带 UV 时的纹理排序权重和 chart/面积硬保护；默认关闭，关闭时几何结果与旧路径完全一致。
10. `SimplifyReport` / metrics CSV：解释运行结果，而不是只看最后面数。

一个结果没有达到目标面数时，第一反应不应是“QEM 坏了”，而要看：

- `termination_reason` 是 `no-candidates` 还是 `rejection-limit`。
- 哪类 `*_rejected_collapses` 最高。
- `featureLoops`、`circularFeatureLoops` 是否符合预期。
- `minAppliedLineWeight`、`maxAppliedLineWeight` 是否因权重模式变得过强。
- 目标比例是否已经与质量/误差/特征保护冲突。

## 现有边界与未来扩展

当前没有承诺：

- 从任意 STL 自动恢复完整 CAD feature tree。
- 对高噪扫描输入自动完成去噪和法线重估（opt-in 的 smooth-curvature 通道提供确定性 ridge/valley 证据，但不承担去噪预处理）。
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
- 讲 smooth-curvature 时必须写明它是 opt-in、确定性、无学习成分的弱证据通道，只在显式 `FeatureGraph` 与硬证据汇合，不改变默认检测行为。
- 讲纹理感知简化时必须区分“标量排序代价”和“chart/有符号面积硬过滤”，不得说几何 quadric 被扩维，也不得在 CLI 文档里描述不存在的纹理选项。
- 讲工业安全时必须绑定具体过滤器、测试数据和报告字段。
- 新增能力应进入 `include/algorithms/<domain>/` 下的平级模块，而不是反向塞进 simplification。
