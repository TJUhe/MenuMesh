/**
 * @file include/algorithms/feature_detection/FeatureTypes.h
 * @brief 声明 ManuMesh 特征检测模块的特征类型设施。
 * @ingroup manumesh_feature_detection
 *
 * @details 此文件属于确定性的三角表面特征管线。局部证据与图清理、追踪、基本体恢复和分区相互分离，使每个阶段都有明确契约。
 */

#pragma once

#include "core/Mesh.h"

#include <utility>
#include <vector>

namespace manumesh::feature {

/// validateFeatureOptions 接受的迭代/尺度参数上限。实现不会计算超过这些上限的环数、尺度或迭代次数，
/// 因此越界请求会立即拒绝，而不是静默截断。
inline constexpr int kMaxNormalTensorSmoothingIterations = 8;
inline constexpr int kMaxNormalTensorScaleCount = 8;
inline constexpr int kMaxSmoothCurvatureBaseNeighborhoodRings = 4;
inline constexpr int kMaxSmoothCurvatureScaleCount = 6;
inline constexpr int kMaxSmoothCurvatureRobustFitIterations = 4;
inline constexpr int kMaxFeatureNormalFilterIterations = 16;

/// 面向含噪三角网格的可选法向域预处理。
///
/// 过滤器保持输入拓扑和顶点位置不变。它交替计算边指标并按面积加权松弛面法向，
/// 使特征证据可以使用稳定后的法向，而不会静默替换调用方的网格。
struct FeatureNormalFilterOptions {
    bool enabled = false;           ///< 在完整检测器中启用预处理。
    int iterations = 4;             ///< 松弛次数，范围为 [0, kMaxFeatureNormalFilterIterations]。
    double angleSigmaDeg = 20.0;    ///< 双边角度带宽，单位为度。
    double preserveAngleDeg = 50.0; ///< 大于等于此角度的不连续处会被冻结。
    double relaxation = 0.8;        ///< 向兼容邻域均值混合的比例，范围为 [0,1]。
};

/// 局部特征图清理后的组件级恢复。
struct FeatureGraphConsolidationOptions {
    bool enabled = false;           ///< 启用清理后的组件级恢复。
    double maxGapLengthRatio = 3.0; ///< 局部边长单位下允许的最大端点间隙。
    double minAlignment = 0.75;     ///< 延续切线点积绝对值的最小值。
};

/// 由活动特征图边诱导的可选面分区。
struct SurfacePatchOptions {
    bool enabled = false;            ///< 在图恢复后启用面分割。
    bool includeWeakEvidence = true; ///< 将仅由张量/曲率产生的边视为分区边界。
};

/// 一个检测特征环的拟合基本体类型。
enum class FeaturePrimitiveType {
    Unknown,
    Circle,
    NearCircle,
    Ellipse,
    PolygonalLoop,
};

/// 折痕、边界和特征环检测参数。
///
/// 检测器首先针对 CAD/STL 风格网格调校：显式边界和二面角证据构成特征图，张量证据作为弱折痕的次要信号。
/// 因此应结合网格尺度和数据来源选择阈值，不应在扫描/含噪输入与干净 CAD 输入之间盲目复用。
struct FeatureOptions {
    /// 硬特征边的二面角阈值，单位为度。
    double featureAngleDeg = 40.0;
    /// 将特征边追踪为环归属时使用的二面角。
    /// 负值表示“复用 featureAngleDeg”。
    double loopTraceAngleDeg = -1.0;
    /// 校验圆环时使用的相对径向容差。
    double circleFitRelativeThreshold = 0.05;
    /// 校验椭圆环时使用的相对残差容差。
    double ellipseFitRelativeThreshold = 0.05;
    /// 低于此轴比容差的椭圆视为近圆。
    double nearCircleAxisRatioTolerance = 0.08;
    /// 恢复环接受和基本体拟合所需的最少顶点数。
    /// 直接追踪的开放链和闭合追踪结果即使低于此阈值仍会报告，只是不会通过该路径获得基本体拟合。
    int minFeatureLoopVertices = 8;
    /// 除图边外，启用由张量推导的弱特征候选。
    bool useNormalTensorFeatures = true;
    /// 弱特征分类所需的最小张量显著性分数。
    double normalTensorFeatureThreshold = 0.16;
    /// 接受张量推导边证据所需的最小边/切线对齐度。
    double normalTensorMinEdgeAlignment = 0.45;
    /// 张量评分前的一环法向平滑次数。
    /// 有效范围：[0, kMaxNormalTensorSmoothingIterations]。
    int normalTensorSmoothingIterations = 0;
    /// 弱特征评分采样的张量尺度数量。
    /// 有效范围：[1, kMaxNormalTensorScaleCount]。
    int normalTensorScaleCount = 1;
    /// 支持张量边候选的最小尺度数量。
    /// 有效范围：[1, normalTensorScaleCount]。
    int normalTensorMinPersistentScales = 1;
    /// 启用由局部 quadric 拟合得到的确定性平滑脊/谷证据。
    /// 保持为可选功能，因为 CAD/STL 硬特征与扫描/自由曲面场景需要不同的阈值和校验数据。
    bool useSmoothCurvatureFeatures = false;
    /// 最小尺度归一化平滑特征分数。
    double smoothCurvatureFeatureThreshold = 0.015;
    /// 网格边与恢复曲线切线之间的最小对齐度。
    double smoothCurvatureMinEdgeAlignment = 0.55;
    /// 最小跨尺度及端点切线一致性。
    double smoothCurvatureMinTangentConsistency = 0.65;
    /// 局部 quadric 拟合使用的基础拓扑半径。
    /// 有效范围：[1, kMaxSmoothCurvatureBaseNeighborhoodRings]。
    int smoothCurvatureBaseNeighborhoodRings = 2;
    /// 逐步增大的 quadric 拟合邻域数量。
    /// 有效范围：[1, kMaxSmoothCurvatureScaleCount]。
    int smoothCurvatureScaleCount = 3;
    /// 支持平滑特征候选的最小尺度数量。
    /// 有效范围：[1, smoothCurvatureScaleCount]。
    int smoothCurvatureMinPersistentScales = 2;
    /// 局部 quadric 拟合的确定性稳健重加权次数。
    /// 有效范围：[0, kMaxSmoothCurvatureRobustFitIterations]。
    int smoothCurvatureRobustFitIterations = 2;
    /// 根据跨尺度稳定性而不是单独的原始峰值分数选择参考拟合尺度。
    bool smoothCurvatureUseStableScaleSelection = false;
    /// 选定平滑曲率尺度可接受的最小稳定性。
    double smoothCurvatureMinScaleStability = 0.0;
    /// 在环恢复前启用局部特征图清理。
    bool cleanupFeatureGraph = true;
    /// 清理阶段可桥接的最大端点间隙，单位为局部平均边长。
    double featureGraphGapLengthRatio = 1.25;
    /// 旧版按边数清理规则会移除的最大弱证据（法向张量或平滑曲率）spur 长度。
    int featureGraphMaxWeakSpurEdges = 2;
    /// 报告高置信度组件时使用的置信度阈值。
    double featureComponentMinConfidence = 0.35;
    /// 用于移除弱 spur 的无量纲 Yoshizawa 风格强度阈值。
    ///
    /// 当该值为正时，仅当悬挂弱证据链的曲线强度 T = (积分 ds) * (积分 strength ds) 低于该值时才移除。
    /// 其中 ds 以局部平均边长为单位，每条边的强度为持久分数除以对应通道阈值。这样长但微弱的链会保留，
    /// 短但强烈的噪声尖峰会被裁剪，且长度超过 featureGraphMaxWeakSpurEdges 的链也可被裁剪。
    /// 默认值 0 保留旧行为：移除边数不超过 featureGraphMaxWeakSpurEdges 的所有弱 spur。
    double featureGraphMinWeakSpurStrength = 0.0;

    /// 可选的含噪输入预处理、组件恢复和面分割阶段。
    /// 将它们分组可以保持主要选项界面可读，同时保留值语义。
    FeatureNormalFilterOptions normalFilter;
    FeatureGraphConsolidationOptions graphConsolidation;
    SurfacePatchOptions surfacePatches;
};

/// 一次法向域预处理运行的诊断信息。
struct FeatureNormalFilterReport {
    int iterationsCompleted = 0;       ///< 实际执行的松弛次数。
    int changedFaces = 0;              ///< 输出法向不同于原始法向的面数。
    int preservedEdges = 0;            ///< 由保留门限冻结的强边数。
    double meanAngularChangeDeg = 0.0; ///< 原始到过滤后法向夹角的平均值。
    double maxAngularChangeDeg = 0.0;  ///< 原始到过滤后法向夹角的最大值。
    double meanEdgeIndicator = 0.0;    ///< 最终双边边指标的平均值，范围为 [0,1]。
};

/// 过滤后的面法向及定量预处理诊断信息。
struct FeatureNormalFilterResult {
    std::vector<Vec3> faceNormals;
    FeatureNormalFilterReport report;
};

/// Tsuchie-Higashi 风格法向张量特征评分参数。
struct NormalTensorOptions {
    int smoothingIterations = 0; ///< 张量投票前的面法向平滑次数。
    int scaleCount = 1;          ///< 逐步增大的拓扑尺度数量。
};

/// 每个顶点的法向张量分解和特征显著性。
struct NormalTensorVertex {
    Vec3 normal = Vec3(0.0, 0.0, 1.0);        ///< 主导张量特征向量。
    Vec3 creaseTangent = Vec3(1.0, 0.0, 0.0); ///< 从中间特征方向推断的切线。
    double surfaceSaliency = 0.0;             ///< 局部平面支持度。
    double creaseSaliency = 0.0;              ///< 类曲线法向变化。
    double cornerSaliency = 0.0;              ///< 各向同性多方向变化。
    double featureScore = 0.0;                ///< 接受的单尺度最大特征分数。
    /// 所有采样尺度上的每尺度特征分数平均值，不论该尺度是否支持获胜候选
    /// （所有尺度分数之和除以尺度数量）。
    double averageFeatureScore = 0.0;
    double persistentFeatureScore = 0.0; ///< 跨尺度持久性门限后的分数。
    double localScale = 0.0;             ///< 选定邻域半径，单位为模型单位。
    int persistentScales = 0;            ///< 提供支持的尺度数量。
};

/// 稳健尺度归一化局部 quadric 拟合参数。
struct SmoothCurvatureOptions {
    int baseNeighborhoodRings = 2;        ///< 最细拟合的拓扑半径。
    int scaleCount = 3;                   ///< 逐步增大的拟合数量。
    int robustFitIterations = 2;          ///< 确定性的残差重加权次数。
    double minTangentConsistency = 0.65;  ///< 跨尺度切线点积绝对值门限。
    bool useStableScaleSelection = false; ///< 相比原始峰值分数优先选择稳定尺度支持。
    double minScaleStability = 0.0;       ///< 可接受的最小尺度稳定性分数。
};

/// 通过多尺度 quadric 拟合得到的每个顶点平滑脊/谷证据。
///
/// 曲率和分数按拟合邻域半径归一化，因此在网格统一缩放时阈值保持稳定。
struct SmoothCurvatureVertex {
    Vec3 normal = Vec3(0.0, 0.0, 1.0);            ///< 拟合 Monge 框架的曲面法向。
    Vec3 curveTangent = Vec3(1.0, 0.0, 0.0);      ///< 沿脊/谷曲线的方向。
    Vec3 extremumDirection = Vec3(0.0, 1.0, 0.0); ///< 跨曲线的主方向。
    double principalCurvature = 0.0;              ///< 用于极值测试的带符号曲率。
    double secondaryCurvature = 0.0;              ///< 带符号的正交主曲率。
    double anisotropy = 0.0;                      ///< 无量纲主曲率分离度。
    double extremumStrength = 0.0;                ///< 双侧方向极值强度。
    double featureScore = 0.0;                    ///< 接受的最大尺度归一化分数。
    /// 仅对支持获胜候选的尺度（符号持久且切线一致）取分数平均值；不支持尺度贡献为零。
    /// 这有意区别于对每个尺度无条件取平均的 NormalTensorVertex::averageFeatureScore。
    double averageFeatureScore = 0.0;
    double persistentFeatureScore = 0.0; ///< 符号/切线持久性门限后的分数。
    double fitResidual = 0.0;            ///< 归一化稳健 quadric 残差。
    double localScale = 0.0;             ///< 选定拟合半径，单位为模型单位。
    int persistentScales = 0;            ///< 提供支持的尺度数量。
    int selectedScale = -1;              ///< 从零开始的参考尺度；无效时为 -1。
    double scaleStability = 0.0;         ///< 相邻尺度拟合的一致性，范围为 [0,1]。
    /// 正值表示脊，负值表示谷，零表示未分类。
    int signedKind = 0;
};

/// 网格中检测到的一条连通特征曲线或环。
struct FeatureLoop {
    /// @name 恢复的拓扑和归属
    /// @{
    int id = -1;
    int componentId = -1;
    std::vector<int> vertices;
    int edgeCount = 0;
    bool closed = false;
    bool circular = false;
    bool mostlyBoundary = false;
    bool weakFeature = false;
    double componentConfidence = 0.0;
    double primitiveResidual = 0.0;
    /// @}

    /// @name 基本体拟合
    /// 圆和近圆使用 `radius`；椭圆使用长轴/短轴参数对。
    /// @{
    FeaturePrimitiveType primitive = FeaturePrimitiveType::Unknown;
    Vec3 center = Vec3::Zero();
    Vec3 normal = Vec3(0.0, 0.0, 1.0);
    Vec3 majorAxis = Vec3(1.0, 0.0, 0.0);
    Vec3 minorAxis = Vec3(0.0, 1.0, 0.0);
    double radius = 0.0;
    double majorRadius = 0.0;
    double minorRadius = 0.0;
    double axisRatio = 0.0;
    double rmsRadialError = 0.0;
    double maxRadialError = 0.0;
    double rmsEllipseError = 0.0;
    double maxEllipseError = 0.0;
    double rmsPlaneError = 0.0;
    double maxPlaneError = 0.0;
    /// @}

    /// @name 带符号二面角摘要
    /// @{
    int convexEdges = 0;
    int concaveEdges = 0;
    int unknownSignedEdges = 0;
    /// @}
};

/// 面向特征保护简化使用的逐顶点特征分类。
struct VertexFeature {
    /// @name 归属和图角色
    /// @{
    bool isFeature = false;
    bool circular = false;
    bool junction = false;
    bool weakFeature = false;
    FeaturePrimitiveType primitive = FeaturePrimitiveType::Unknown;
    int loopId = -1;
    int componentId = -1;
    double confidence = 0.0;
    Vec3 tangent = Vec3::Zero();
    /// @}

    /// @name 圆投影数据
    /// @{
    Vec3 circleCenter = Vec3::Zero();
    Vec3 circleNormal = Vec3(0.0, 0.0, 1.0);
    double circleRadius = 0.0;
    /// @}

    /// @name 椭圆投影数据
    /// @{
    Vec3 ellipseCenter = Vec3::Zero();
    Vec3 ellipseNormal = Vec3(0.0, 0.0, 1.0);
    Vec3 ellipseMajorAxis = Vec3(1.0, 0.0, 0.0);
    Vec3 ellipseMinorAxis = Vec3(0.0, 1.0, 0.0);
    double ellipseMajorRadius = 0.0;
    double ellipseMinorRadius = 0.0;
    /// @}
};

/// 显式特征图中的一条边。
struct FeatureGraphEdge {
    int a = -1;
    int b = -1;
    bool boundary = false;
    bool dihedral = false;
    bool normalTensor = false;
    bool smoothCurvature = false;
    bool nonManifold = false;
    bool cleanupBridge = false;
    bool consolidationBridge = false;
    bool removedByCleanup = false;
    int signedKind = 0;
};

/// 从特征图顶点离开的一条有向分支。
struct FeatureGraphBranch {
    int edgeId = -1;
    int neighborVertex = -1;
    Vec3 tangent = Vec3::Zero();
    int signedKind = 0;
};

/// 连接点处两条入射分支之间的最佳延续配对。
struct FeatureGraphBranchPair {
    int firstBranch = -1;
    int secondBranch = -1;
    double alignment = 0.0;
};

/// 显式特征图中的逐顶点归属。
///
/// 只有当两个以上活动边在顶点处汇合（或顶点被多个环共享）时，顶点才是连接点；
/// 恰好只有一条活动边的顶点是链端点，而不是连接点。
struct FeatureGraphVertex {
    std::vector<int> incidentEdges;
    std::vector<int> loopIds;
    std::vector<FeatureGraphBranch> branches;
    std::vector<FeatureGraphBranchPair> branchPairs;
    bool junction = false;
    bool shared = false;
    bool endpoint = false;
    bool ambiguousJunction = false;
};

/// 检测特征边和恢复环的显式图视图。
struct FeatureGraph {
    std::vector<FeatureGraphEdge> edges;
    std::vector<FeatureGraphVertex> vertices;
    std::vector<int> junctionVertices;
    std::vector<int> sharedVertices;
    std::vector<int> endpointVertices;
};

/// 追踪清理后的一个连通特征图组件。
///
/// 这些诊断信息显式表达弱特征决策：即使两者都产生特征顶点，下游简化仍可区分闭合且有强支持的 CAD 环
/// 与稀疏的弱证据脊片段。
struct FeatureComponent {
    int id = -1;
    std::vector<int> vertices;
    int edgeCount = 0;
    int boundaryEdges = 0;
    int dihedralEdges = 0;
    int normalTensorEdges = 0;
    int smoothCurvatureEdges = 0;
    int nonManifoldEdges = 0;
    int cleanupBridgeEdges = 0;
    int consolidationBridgeEdges = 0;
    int strongEvidenceEdges = 0;
    int weakEvidenceEdges = 0;
    int junctionVertices = 0;
    int endpointVertices = 0;
    int cycleRank = 0;
    bool closed = false;
    double closureRate = 0.0;
    double strongEvidenceRatio = 0.0;
    double meanTensorPersistence = 0.0;
    double meanCurvaturePersistence = 0.0;
    double meanPrimitiveResidual = 0.0;
    double confidence = 0.0;
};

/// 由活动特征边分隔的一个连通面区域。
struct FeaturePatch {
    int id = -1;
    int faceCount = 0;
    int featureBoundaryEdges = 0;
    int meshBoundaryEdges = 0;
    int nonManifoldBoundaryEdges = 0;
    bool closed = false;
    double area = 0.0;
    Vec3 normal = Vec3(0.0, 0.0, 1.0);
    std::vector<int> neighboringPatches;
};

/// 两个曲面分区之间的特征边邻接关系。
struct FeaturePatchAdjacency {
    int firstPatch = -1;
    int secondPatch = -1;
    int featureEdges = 0;
};

/// 网格的完整特征检测结果。
///
/// 计数区分构建显式图时使用的证据来源。下游算法应优先使用 `loops` 和 `vertices` 获取特征归属，
/// 并使用计数进行诊断和策略校验。
///
/// `featureEdges` 只统计证据边。图清理合成的桥接边会追加到 `graph.edges`（标记为 `cleanupBridge`），
/// 但不属于证据，因此 `graph.edges.size()` 可能大于 `featureEdges`。
struct FeatureAnalysis {
    std::vector<VertexFeature> vertices;
    std::vector<FeatureLoop> loops;
    std::vector<FeatureComponent> components;
    FeatureGraph graph;
    int featureEdges = 0;
    int tracedFeatureEdges = 0;
    int untracedFeatureEdges = 0;
    int graphCleanupBridgedGaps = 0;
    int graphCleanupRemovedSpurs = 0;
    int graphCleanupMergedJunctions = 0;
    int boundaryFeatureEdges = 0;
    int dihedralFeatureEdges = 0;
    int normalTensorFeatureEdges = 0;
    int smoothCurvatureFeatureEdges = 0;
    int nonManifoldFeatureEdges = 0;
    int normalTensorScoredVertices = 0;
    int smoothCurvatureScoredVertices = 0;
    int convexFeatureEdges = 0;
    int concaveFeatureEdges = 0;
    int unknownSignedFeatureEdges = 0;
    int weakFeatureComponents = 0;
    int highConfidenceFeatureComponents = 0;
    double maxNormalTensorFeatureScore = 0.0;
    double maxNormalTensorPersistentScore = 0.0;
    double meanNormalTensorLocalScale = 0.0;
    double meanNormalTensorPersistence = 0.0;
    double maxSmoothCurvatureFeatureScore = 0.0;
    double maxSmoothCurvaturePersistentScore = 0.0;
    double meanSmoothCurvatureLocalScale = 0.0;
    double meanSmoothCurvaturePersistence = 0.0;
    double meanSmoothCurvatureScaleStability = 0.0;
    double meanFeatureComponentConfidence = 0.0;
    double minFeatureComponentConfidence = 0.0;
    /// 两个面绕序不一致的内部边；对这些边，二面角评分回退到无符号法向夹角。
    int inconsistentWindingEdges = 0;
    /// 因达到端点/连接点硬上限而跳过的清理轮数。
    int graphCleanupSkippedByCap = 0;
    /// 三元组扫描被截断的圆簇恢复组件数。
    int circularRecoveryTruncated = 0;
    /// 被容忍为退化的输入面（顶点位置重复或数值上面积为零）。其法向不可用，因此逐面证据会跳过贡献；
    /// 该计数使降级情况可见，而不是静默吸收脏输入。
    int degenerateFaces = 0;
    FeatureNormalFilterReport normalFilter;
    int graphConsolidationBridges = 0;
    int graphConsolidationSkippedByCap = 0;
    int junctionBranchPairs = 0;
    int ambiguousJunctions = 0;
    std::vector<int> facePatchIds;
    std::vector<FeaturePatch> patches;
    std::vector<FeaturePatchAdjacency> patchAdjacencies;
    int closedSurfacePatches = 0;
    int segmentationIgnoredRecoveryEdges = 0;
};

/// 一个带标签特征连接点处的真实延续关系。
struct FeatureBranchPairLabel {
    int junctionVertex = -1;
    int firstNeighbor = -1;
    int secondNeighbor = -1;
};

/// 面向边、连接点、分支和面分区基准测试的可扩展标签。
struct FeatureBenchmarkLabels {
    std::vector<std::pair<int, int>> edges;
    std::vector<int> junctionVertices;
    std::vector<FeatureBranchPairLabel> branchPairs;
    /// 每个面的真实分区 ID；负值表示未标注。
    std::vector<int> facePatchIds;
};

/// 一个检测特征图的边标签基准摘要。
struct FeatureEdgeBenchmark {
    int groundTruthEdges = 0;
    int detectedEdges = 0;
    int truePositiveEdges = 0;
    int falsePositiveEdges = 0;
    int falseNegativeEdges = 0;
    int groundTruthJunctions = 0;
    int detectedJunctions = 0;
    int truePositiveJunctions = 0;
    int falsePositiveJunctions = 0;
    int falseNegativeJunctions = 0;
    double edgePrecision = 0.0;
    double edgeRecall = 0.0;
    double edgeF1 = 0.0;
    double junctionPrecision = 0.0;
    double junctionRecall = 0.0;
    double junctionF1 = 0.0;
    double loopClosureRate = 0.0;
    double meanComponentConfidence = 0.0;
    int groundTruthBranchPairs = 0;
    int detectedBranchPairs = 0;
    int truePositiveBranchPairs = 0;
    int falsePositiveBranchPairs = 0;
    int falseNegativeBranchPairs = 0;
    double branchPairPrecision = 0.0;
    double branchPairRecall = 0.0;
    double branchPairF1 = 0.0;
    int labeledFaceAdjacencies = 0;
    int correctFaceAdjacencies = 0;
    double patchAdjacencyAccuracy = 0.0;
};

/// 检测环相对于圆形参考曲线的误差。
struct DirectionalCurveError {
    int samples = 0;
    double radialRms = 0.0;
    double radialMax = 0.0;
    double planeRms = 0.0;
    double planeMax = 0.0;
};

} // namespace manumesh::feature
