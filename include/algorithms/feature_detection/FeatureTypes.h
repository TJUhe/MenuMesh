/**
 * @file include/algorithms/feature_detection/FeatureTypes.h
 * @brief 定义特征证据、特征图、曲线、组件和曲面分区结果。
 * @ingroup manumesh_feature_detection
 *
 * @details FeatureAnalysis 是现有兼容结果；普通消费者应只读取所需的图、曲线或组件数据。
 */

#pragma once

#include "algorithms/feature_detection/FeatureOptions.h"
#include "core/Mesh.h"

#include <cstdint>
#include <utility>
#include <vector>

namespace manumesh {
namespace feature {

/// 一个检测特征环的拟合几何基元类型。
enum class FeaturePrimitiveType {
    Unknown,
    Circle,
    NearCircle,
    Ellipse,
    PolygonalLoop,
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

/// 每个顶点的法向张量分解和特征显著性。
struct NormalTensorVertex {
    Vec3 normal = Vec3(0.0, 0.0, 1.0);        ///< 主导张量特征向量。
    Vec3 creaseTangent = Vec3(1.0, 0.0, 0.0); ///< 最小特征值对应方向，用作折痕切线。
    double surfaceSaliency = 0.0;             ///< 局部平面支持度。
    double creaseSaliency = 0.0;              ///< 类曲线法向变化。
    double cornerSaliency = 0.0;              ///< 各向同性多方向变化。
    double featureScore = 0.0;                ///< 接受的单尺度最大特征分数。
    /// 所有采样尺度上的每尺度特征分数平均值，不论该尺度是否支持获胜候选
    /// （所有尺度分数之和除以尺度数量）。
    double averageFeatureScore = 0.0;
    double persistentFeatureScore = 0.0; ///< 跨尺度持久性门限后的分数。
    /// 选中尺度单次平滑使用的名义高斯核半径，单位为模型单位。
    /// 保留该字段用于源码兼容；多次顺序平滑后的累计支持域应读取 `effectiveRadius`。
    double localScale = 0.0;
    /// 达到阈值且与获胜尺度类型一致的尺度数量。折痕尺度还必须保持切向一致；
    /// 稳定角点会保留为简化权重，但不会因此生成折痕边。
    int persistentScales = 0;
    /// 从零开始的获胜尺度；没有可分析支持域时为 -1。
    int selectedScale = -1;
    /// 获胜尺度前已执行的张量一环平滑总步数，包含基础平滑和尺度扩散。
    int smoothingSteps = 0;
    /// 将初始一环支持和各次高斯扩散半径按平方和合成的近似有效半径。
    /// 它不是严格测地球半径，但比 `localScale` 更准确地表达顺序扩散后的支持宽度。
    double effectiveRadius = 0.0;
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
    /// 接受的最大尺度无量纲分数，范围为 [0, kMaxSmoothCurvatureFeatureScore]，不是 [0,1] 概率。
    double featureScore = 0.0;
    /// 仅对支持获胜候选的尺度（符号持久且切线一致）取分数平均值；不支持尺度贡献为零。
    /// 这有意区别于对每个尺度无条件取平均的 NormalTensorVertex::averageFeatureScore。
    double averageFeatureScore = 0.0;
    /// 符号/切线持久性门限后的无量纲分数，范围为 [0, kMaxSmoothCurvatureFeatureScore]。
    double persistentFeatureScore = 0.0;
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

    /// @name 几何基元拟合
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
    double tensorPersistence = 0.0;
    int tensorPersistentScales = 0;
    double curvaturePersistence = 0.0;
    int curvaturePersistentScales = 0;

    /// @return 此边是否仅由图恢复阶段合成，而不是原始局部证据直接产生。
    bool synthetic() const { return cleanupBridge || consolidationBridge; }
    /// @return 此边是否携带边界、二面角或非流形等离散几何证据。
    bool hasDiscreteEvidence() const { return boundary || dihedral || nonManifold; }
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
    /// 面积加权代表法向；当闭合/对称面片的合成法向数值上不可定义时稳定回退为 +Z。
    Vec3 normal = Vec3(0.0, 0.0, 1.0);
    std::vector<int> neighboringPatches;
};

/// 两个曲面分区之间的特征边邻接关系。
struct FeaturePatchAdjacency {
    int firstPatch = -1;
    int secondPatch = -1;
    int featureEdges = 0;
};

/// 标识生成特征分析结果时使用的精确索引几何。
///
/// 指纹使用固定字节编码而不是 `std::hash`，因此跨进程和标准库版本保持确定性。
/// 拓扑包含顶点/面数量以及存储顺序中的每个面角点；几何还包含存储顺序中的每个
/// IEEE-754 顶点坐标。特征检测不使用纹理坐标，因此有意将其排除在外。
struct FeatureAnalysisSource {
    std::uint64_t vertexCount = 0;
    std::uint64_t faceCount = 0;
    std::uint64_t topologyFingerprint = 0;
    std::uint64_t geometryFingerprint = 0;
};

/// 网格的完整特征检测结果。
///
/// 计数区分构建显式图时使用的证据来源。下游算法应优先使用 `loops` 和 `vertices` 获取特征归属，
/// 并使用计数进行诊断和策略校验。
///
/// `featureEdges` 只统计证据边。图清理合成的桥接边会追加到 `graph.edges`（标记为 `cleanupBridge`），
/// 但不属于证据，因此 `graph.edges.size()` 可能大于 `featureEdges`。
///
/// 此聚合类型保留公开字段以维持既有源码和字段偏移。新的只读消费者应包含
/// `FeatureAnalysisViews.h`，并按职责选择 evidence、curve、segmentation 或 diagnostics 视图。
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
    /// 追加字段，用于保留既有公共字段的布局偏移。
    FeatureAnalysisSource source;
    /// 使用生成本分析结果的配置解析得到的紧凑逐顶点 Normal Tensor 权重。
    /// 禁用 Normal Tensor 证据时为空。下游应直接使用这些权重，不要用另一组阈值
    /// 或尺度配置重新解释它们。
    std::vector<double> normalTensorVertexWeights;
    /// 使用生成本分析结果的配置解析得到的紧凑逐顶点平滑曲率持久性权重。
    /// 禁用平滑曲率证据时为空。下游应直接使用这些权重，不要用另一组阈值
    /// 或尺度配置重新解释它们。
    std::vector<double> smoothCurvatureVertexWeights;
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

} // namespace feature
} // namespace manumesh
