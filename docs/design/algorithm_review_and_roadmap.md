# 算法现状复核与路线图

本文按 ManuMesh 当前源码复核，而不是按早期设想描述。核对对象包括 `include/algorithms/feature_detection/FeatureDetector.h`、`include/algorithms/simplification/`、`src/feature_detection/`、`src/simplification/`、`apps/manumesh/` 和当前 CTest 发现结果（249 个测试，其中 1 个手工性能测试默认禁用；2026-07-15 复核）。算法本质和数学直觉见 [`algorithm_essence.md`](algorithm_essence.md)。2026-07-09 的特征识别升级记录见 [`feature_detection_upgrade_2026_07_09.md`](feature_detection_upgrade_2026_07_09.md)；2026-07-11 落地的确定性光滑曲率特征检测见 [`smooth_curvature_feature_detection_2026_07_11.md`](smooth_curvature_feature_detection_2026_07_11.md)；纹理感知 4×4 QEM 设计见 [`texture_aware_qem.md`](texture_aware_qem.md)。

## 当前理解框架

ManuMesh 的简化器应按四层阅读：

1. **标准 QEM**：用 plane quadric 让新顶点贴近原始局部切平面。
2. **line quadrics**：在平坦区切向零空间里加入小正则，改善候选排序和 placement 病态。
3. **feature graph**：从 boundary、non-manifold、dihedral、normal-tensor 以及 opt-in smooth-curvature 证据中恢复 loop，并拟合圆、近圆、椭圆或折线 primitive。
4. **hard filters**：用特征、边界、拓扑、法线、质量、局部误差、自交检查，以及 opt-in 的 UV chart/面积检查，决定候选能否真正执行。

这四层分别解决不同问题。把 line weight 调大不能替代特征保护，把 feature quadric 调大不能替代拓扑过滤，把 normal tensor 打开也不能替代扫描去噪。

## 当前已经实现

| 方向 | 当前状态 |
| --- | --- |
| 标准 QEM | 已实现面 plane quadric 面积加权累加、边折叠候选、placement 求解和 GH97 三级 fallback 链（全空间最优 → 沿坍缩边一维最优（rank-2 情形）→ 端点/中点）；边界边坍缩使用 Lindstrom-Turk 边界守恒 placement（`detail/Placement.{h,cpp}`）。 |
| line quadrics | 已实现 `--method line` 和 `lineWeight`，作为候选排序成本的切向正则项；`adaptiveScale` 模式下按 Wang 2008 解耦，`featureBoost` 只作为逐顶点队列优先级因子（`priorityScale`），不再进入 quadric 与 placement 求解。 |
| 权重模式 | `uniform`、`dihedral`、`normal-tensor`、`height`、`xband`。 |
| 特征检测 | 已作为 `feature_detection` 平级算法模块实现，支持边界、非流形边、二面角边、normal-tensor 弱特征、feature graph、cleanup、loop tracing，并用 `loopTraceAngleDeg` 区分 evidence 阈值和 loop ownership 阈值；二面角为绕向感知的有向角（可识别 >90° 反折边，绕向不一致时回退无符号角并计入 `inconsistentWindingEdges`）；normal tensor 已接入局部尺度归一化、多尺度 persistence 和 persistent score；全网格辅助结构（法向、边信息、邻接、局部边长）经 `FeatureDetectionCache` 构建一次全管线复用。 |
| 光滑曲率特征检测 | 2026-07-11 已以 opt-in 落地并于 2026-07-12/13 升级（`FeatureOptions::useSmoothCurvatureFeatures`，默认 `false`）：多尺度鲁棒**三次 Monge** 拟合、带符号主曲率、解析 extremality（`e_i = ∇κ_i·t_i`）、Ohtake 边零交叉极值判据、cyclideness 门控和跨尺度支持计票，全程确定性、无学习成分。当前 persistence 不要求支持尺度相邻，也不要求最粗尺度支持；每个尺度与最佳尺度比较符号、切向和分数。与硬证据只在显式 `FeatureGraph` 汇合。设计见 [`smooth_curvature_feature_detection_2026_07_11.md`](smooth_curvature_feature_detection_2026_07_11.md)。 |
| 弱毛刺清理 | 除按边数剪枝外，新增 opt-in 的 Yoshizawa 组件级无量纲强度过滤 `featureGraphMinWeakSpurStrength`（`T = (∫ds)·(∫strength ds)`，默认 0 = 旧行为，仅 C++ `FeatureOptions` 暴露）；端点 gap 桥接采用 Yoshizawa 角度三条件。 |
| 纹理感知简化 | 已以 opt-in 落地（`SimplifyOptions::preserveTexture`，默认 `false`，仅 C++ API，CLI 未暴露）：几何 quadric 保持 4×4，纹理只作为局部标量排序代价（`textureWeight`）加 UV chart 配对与有符号面积硬过滤（`textureSeamTolerance`、`minTextureAreaRatio`）；关闭时几何输出与旧路径 bit-exact。设计见 [`texture_aware_qem.md`](texture_aware_qem.md)。 |
| UV 数据模型与 IO | `Mesh::faceTexCoords` / `PlainMesh::faceTexCoords`（`PlainVec2`、`PlainFaceTexCoords`）存储角拥有的逐面逐角 UV；OBJ 读取升级为多边形网格自动三角化并保留逐角 `vt` 索引。 |
| feature component | 已实现 component-level confidence，统计强/弱证据比例、闭合率、junction/endpoint、cycle rank、tensor persistence、primitive residual，并把 confidence 写入 loop/vertex/QEM soft feature quadric。 |
| primitive 拟合 | 支持圆、近圆、椭圆、折线 loop 的报告和保护策略；圆用 Taubin 代数拟合（一阶无偏，Kåsa 保留为确定性回退），椭圆用 Halíř-Flusser 直接最小二乘拟合（保证椭圆输出，轴向来自 conic 而非 PCA）。 |
| 特征保护 | `none`、`circular-only`、`primitive-curves`、`all-feature-edges`。默认是 `primitive-curves`。 |
| 合法性过滤 | 边界、拓扑（含与 `preserveBoundary` 无关、始终生效的边界弦 pinch 拒绝）、法线偏转、三角形质量、局部误差和局部自交过滤（相交谓词使用尺度不变相对容差 `kRelativeIntersectionEps = 1e-9`）。 |
| 通用统计与比较 | 已拆出 `manumesh::analysis` 模块（`algorithms/analysis/MeshAnalysis.h`）：`MeshStats`/`computeMeshStats`、`DistanceStats`/`compareMeshesBySampledDistance`；圆环 loop 匹配 `matchCircularLoops`（`algorithms/feature_detection/FeatureComparison.h`）从 CLI 下沉为库函数。 |
| 诊断报告 | `FeatureAnalysis`、`SimplifyReport` / `ManuMeshSimplifyReport` 输出终止原因、拒绝计数、特征计数、`tracedFeatureEdges` / `untracedFeatureEdges`、graph cleanup 计数、component confidence、normal-tensor scored vertices、local scale / persistence 和权重范围；`FeatureAnalysis` 另有 smooth-curvature 系列诊断（`smoothCurvatureFeatureEdges`、`smoothCurvatureScoredVertices`、最大 raw/persistent 分数、平均 local scale / persistence），`SimplifyReport` 另有纹理诊断（`textureProtectedEdges`、`textureRejectedCollapses`）。 |
| CLI | `generate`、`simplify`、`compare`、`feature-report`、`feature-benchmark`、`feature-compare`、`sweep`、`ratio-sweep`、`face-sweep`、`demo`、`summarize-metrics`、`validate-features`、`validate-external`；feature 分析命令支持 `--smooth-curvature-*` 选项族并输出 smooth_curvature_* 报告/CSV 字段（`simplify` 显式拒绝这些 feature-analysis 选项）。 |
| SDK | Eigen-backed C++ API、Eigen-free `PlainMesh` C++ 入口和 C ABI 均可用，示例位于 `examples/`。 |

## 当前没有实现

- 论文中的完整 edge dihedral plane quadrics。
- 通用扫描去噪、曲率重建或法线重估计流水线（opt-in smooth-curvature 通道提供确定性 ridge/valley 证据，但不承担去噪预处理）。
- 全局 Hough / winding-number 曲线恢复（RFD002/RFD003 仍是路线图参考）。
- 通用 CAD/B-Rep 特征识别。
- 布尔运算、offset/thickening、孔洞修复和全自动 manifold repair。
- 学习式显著特征评分、时间序列一致性简化或神经 QEM 表示。
- C ABI 层的纹理字段与纹理选项（纹理保护当前只在 C++ `SimplifyOptions` 暴露）。

## 主要技术风险

1. line quadrics 是软约束，权重过高会牺牲几何保真。
2. 特征检测目前以 CAD/STL 风格网格为主，噪声扫描件需要更稳健的法线估计、去噪或重建预处理。
3. normal tensor 是弱特征证据和空间变权来源，不是通用 ridge/valley 提取器；当前已把局部尺度、多尺度 persistence 和 persistent score 用于弱特征接受准则与 QEM normal-tensor 权重。
4. `all-feature-edges` 会锁住太多 generic crease，可能导致 `rejection-limit`，默认 `primitive-curves` 更平衡。
5. 局部自交和局部误差过滤改善安全性，但不是全局 Hausdorff/envelope 证明。
6. C ABI 输入结构体依赖 `struct_size` 和 `abi_version`，外部调用必须初始化；纯输出 report/stats 由 size-aware 入口按显式容量初始化。同一 ABI 版本内允许旧尾部尺寸，缺失字段使用默认值。
7. smooth-curvature 通道默认关闭：CAD/STL 硬边与扫描/自由曲面需要不同阈值和验证集；在带标注扫描 benchmark 就绪前不应默认开启。
8. 纹理保护启用时固定拓扑质量精修轮暂时跳过（该顶点重定位阶段尚未约束 UV 失真）；`textureWeight` 只影响候选排序，不提供全局 UV 失真上界。
9. `featureComponentMinConfidence` 目前只决定 `highConfidenceFeatureComponents` 的报告计数，不会过滤 component、loop 或 hard protection；真正进入 feature-curve soft quadric 的连续缩放是 `0.35 + 0.65 * confidence`。
10. graph cleanup 的 endpoint gap bridge 有双端切向规则；close-junction bridge 当前只看局部尺度距离并采用一对一贪心匹配，密集且彼此靠近的独立特征网络仍有误连风险。

## 短期路线

- 保持 `build: mingw+ninja release all`、`test: mingw+ninja release` 和 `test: mingw+ninja release full` 稳定可跑。
- 扩展特征报告 CSV，继续区分 circular、near-circle、ellipse、polygonal loop，并持续跟踪 traced/untraced feature edges。
- 用更多工业件验证 `primitive-curves` 默认策略，避免 generic crease 过度硬锁。
- 扩展 `feature-benchmark` label schema，加入 weak feature group、loop id、简化前后 feature drift / Hausdorff envelope。
- 为 smooth-curvature 路径补充带标注 ridge/valley 曲线的扫描类 benchmark fixture，评估其默认开启条件。
- 补充 CLI 子命令级帮助或明确保持顶层帮助模式。
- 对 C API 增加更多端到端示例和 ABI 兼容测试。

## 中期路线

- 实现独立 edge dihedral plane quadrics，和现有 feature graph 保护策略对比。
- 将弱特征 support consolidation 做到 component 之间，参考 CWF/M026，把多个低置信弱 component 合并成更稳定的保护支撑。
- 将当前双向局部采样和原始曲面 BVH 包络扩展为自适应采样或可证明的全局 Hausdorff 上界。
- 将已落地的质量型二轮 refinement 扩展到高置信 feature support 的受约束重投影和属性感知 relocation（包括让精修轮在纹理保护下约束 UV 失真，而不是整体跳过）。
- 扩展属性传播策略：UV 已通过 `faceTexCoords` 与 `preserveTexture` 落地第一步，法线、颜色、source face id 仍待。
- 继续沿 `algorithms/<domain>` 组织新增模块，建立更清晰的 `repair`、`remesh` 和 `boolean` 边界。
- 将本地自交过滤扩展成更完整的空间查询和 envelope 检查。

## 长期路线

ManuMesh 的长期目标是一个可嵌入的增材制造多边形网格 SDK。QEM/line quadrics 是其中的 decimation 模块，不应承担所有修复、识别和布尔任务。未来模块应保持公共 API、实现、测试和文档边界清楚。

## 论文来源对应

| 方向 | 主要参考 | 当前取用方式 |
| --- | --- | --- |
| 标准 QEM | Garland-Heckbert 1997，`docs/papers/qem/garland_heckbert_1997_surface_simplification_qem.pdf` | plane quadric、顶点 quadric、边坍缩代价。 |
| Line quadrics | Liu-Rahimzadeh-Zordan 2025，`docs/papers/line_quadrics/liu_rahimzadeh_zordan_2025_line_quadrics.pdf` | 平坦区切向正则和候选排序增强。 |
| 边折叠工程 | Hoppe 1996、Lindstrom-Turk 1998、Rose 2025 | 队列、placement、GH97 三级 fallback（含沿边一维最优）、LT 边界守恒 placement、扩展 link condition（边界弦 pinch 拒绝）、合法性过滤和大模型验证习惯。 |
| CAD/STL 特征线 | Vidal-Wolf-Dupont 2011、Jiao-Bayyana 2008、Tsuchie-Higashi 2014 | boundary/dihedral/normal-tensor 证据、有向二面角（Jiao 的取向教训）、feature graph 和 primitive loop。 |
| crest line / 光滑特征 | Ohtake 2004（M011）、Yoshizawa 2005（M021） | 三次拟合解析 extremality、边零交叉判据、组件级曲线强度 `T` 过滤与 gap 桥接角度规则。 |
| 圆/椭圆拟合 | Taubin 1991、Halíř-Flusser 1998、Fitzgibbon 1999、Chernov 2010（经典文献，无本地 PDF） | Taubin 圆拟合替代 Kåsa 正规方程（保留回退），Halíř-Flusser 直接椭圆拟合替代 PCA/二阶矩轴长。 |
| 确定性光滑特征（2017–2025） | Yamakawa-Shimada 2017/2018、Lu 2019（M044）、Romanengo 2020、Xu 2024 CWF（M026）、Cai 2025，索引见 `docs/papers/recent_deterministic_feature_detection_2026-07-11.md` | opt-in smooth-curvature 证据通道的多尺度、鲁棒拟合、consolidation/confidence 依据；全局曲线恢复仍是路线图。 |
| 属性/纹理感知简化 | Garland-Heckbert 1998（M003，历史参照）、现代 edge-collapse 管线综述（M033）、4×4 line-quadric 主干（M004/085） | opt-in 纹理保护采用 M033 工程拆分：几何 QEM 排序固定 3D placement，chart/UV 面积合法性为显式局部策略，不做属性扩维。 |
| 特征保持简化 | Wang 2008、Hussain-Grahn-Persson 2008、CWF 2024 | 软成本、硬保护、弱特征 support、保护/质量冲突的理解，以及 Wang 2008 的"blow-up 权重只进队列优先级、不进 placement 求解"解耦模式（`adaptiveScale` 下的 `priorityScale`）。 |
