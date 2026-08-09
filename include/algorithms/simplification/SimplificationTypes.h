/**
 * @file include/algorithms/simplification/SimplificationTypes.h
 * @brief 声明 ManuMesh 简化模块的简化类型设施。
 * @ingroup manumesh_simplification
 *
 * @details 此文件属于面向特征的边坍缩管线。二次误差代价负责候选排序；拓扑、几何、特征、边界、误差和可选纹理策略共同决定位置是否可以修改网格。
 */

#pragma once

#include "Export.h"

#include <string>

namespace manumesh::simplification {

/// 空间变化 line-quadric 权重的内置策略。
enum class WeightMode {
    Uniform,
    Dihedral,
    NormalTensor,
    Height,
    XBand,
};

/// 一次简化运行停止的原因。
enum class SimplifyTerminationReason {
    NotStarted,
    ReachedTarget,
    AlreadyAtOrBelowTarget,
    NoCandidates,
    RejectionLimit,
};

/// 启用 preserveFeatureCurves 时使用的硬特征约束策略。
///
/// 此策略只控制硬性坍缩拒绝。line quadrics 和特征曲线 quadrics 的软代价仍可影响候选排序。
enum class FeatureProtectionMode {
    /// 禁用硬特征曲线保护。
    None,
    /// 仅硬保护圆环和近圆环。
    CircularOnly,
    /// 硬保护拟合的基本体环：圆、近圆和椭圆。
    PrimitiveCurves,
    /// 严格模式：硬保护所有检测到的特征边。
    AllFeatureEdges,
};

/// 一次简化运行的用户侧控制项。
///
/// 选项按概念分为目标选择、QEM/line-quadric 代价、特征检测、硬合法性过滤和诊断信息。
/// 简化器将 QEM 和 line quadrics 视为候选排序代价，然后对拓扑、边界行为、法向、三角形质量、
/// 局部误差、自交和特征曲线策略应用显式过滤器。
struct SimplifyOptions {
    /// @name 目标选择
    /// @{
    /// 输出面的绝对目标数。为正时覆盖 targetRatio。
    int targetFaces = -1;
    /// 未设置 targetFaces 时使用的输出面比例。
    double targetRatio = 0.25;
    /// @}

    /// @name QEM 和 line-quadric 排序代价
    /// @{
    /// 添加 line quadrics 以减少约束不足区域的切向漂移。
    bool useLineQuadrics = true;
    /// 基础 line-quadric 权重。应保持足够小，使平面 quadrics 和硬过滤器继续控制几何保真度。
    double lineWeight = 1e-3;
    /// line quadrics 和特征敏感代价的空间加权策略。
    WeightMode weightMode = WeightMode::Uniform;
    /// 在检测到的特征证据附近增加的 line-quadric 权重。
    double featureBoost = 0.05;
    /// 硬边特征检测的二面角阈值，单位为度。
    double featureAngleDeg = 40.0;
    /// 将检测到的边追踪为环归属时使用的二面角阈值。
    /// 负值表示“复用 featureAngleDeg”。
    double loopTraceAngleDeg = -1.0;
    /// 根据局部网格尺度缩放线权重，而不是只使用 lineWeight。
    bool adaptiveScale = false;
    double adaptiveBaseLineWeight = 1e-2;
    /// 边界贴合的软代价。需要硬拓扑约束时使用 preserveBoundary。
    double boundaryWeight = 0.0;
    /// 拒绝会合并或移除开放边界结构的坍缩。
    bool preserveBoundary = false;
    /// @}

    /// @name 特征检测、投影和硬策略
    /// @{
    /// 启用特征检测、特征 quadrics、投影和保护。
    bool preserveFeatureCurves = false;
    /// 首选硬特征策略。默认使用基本体曲线，使通用折痕保持软约束，除非调用方明确请求严格锁定。
    FeatureProtectionMode featureProtectionMode = FeatureProtectionMode::PrimitiveCurves;
    /// 应用于检测环的软特征曲线 quadric 权重。
    double featureCurveWeight = 0.05;
    /// 在投影前拒绝偏离特征曲线过远的原始坍缩位置。零值禁用曲线距离预算。
    double maxFeatureCurveDeviationRatio = 0.0;
    /// 简化前特征检测使用的基本体拟合阈值。
    double circleFitRelativeThreshold = 0.05;
    double ellipseFitRelativeThreshold = 0.05;
    double nearCircleAxisRatioTolerance = 0.08;
    /// 通用特征和圆/近圆特征的最小环尺寸。
    int minFeatureLoopVertices = 16;
    int minCircularFeatureLoopVertices = 6;
    /// 除二面角边外，启用法向张量弱特征证据。
    bool useNormalTensorFeatures = true;
    double normalTensorFeatureThreshold = 0.16;
    double normalTensorMinEdgeAlignment = 0.45;
    int normalTensorSmoothingIterations = 0;
    int normalTensorScaleCount = 1;
    int normalTensorMinPersistentScales = 1;
    /// 在简化前的特征分析阶段启用确定性的多尺度平滑脊/谷证据。
    bool useSmoothCurvatureFeatures = false;
    double smoothCurvatureFeatureThreshold = 0.015;
    double smoothCurvatureMinEdgeAlignment = 0.55;
    double smoothCurvatureMinTangentConsistency = 0.65;
    int smoothCurvatureBaseNeighborhoodRings = 2;
    int smoothCurvatureScaleCount = 3;
    int smoothCurvatureMinPersistentScales = 2;
    int smoothCurvatureRobustFitIterations = 2;
    /// 在环恢复前清理弱特征图片段。
    bool cleanupFeatureGraph = true;
    double featureGraphGapLengthRatio = 1.25;
    int featureGraphMaxWeakSpurEdges = 2;
    /// 裁剪悬挂弱证据链时使用的无量纲 Yoshizawa 风格积分强度阈值。零值保持按边数处理的行为。
    double featureGraphMinWeakSpurStrength = 0.0;
    double featureComponentMinConfidence = 0.35;
    /// 面向含噪输入的可选特征保护法向域预处理。
    bool useFeatureNormalFilter = false;
    int featureNormalFilterIterations = 4;
    double featureNormalFilterAngleSigmaDeg = 20.0;
    double featureNormalFilterPreserveAngleDeg = 50.0;
    double featureNormalFilterRelaxation = 0.8;
    /// 平滑曲率证据的稳定尺度参考选择。
    bool smoothCurvatureUseStableScaleSelection = false;
    double smoothCurvatureMinScaleStability = 0.0;
    /// 局部图清理后的组件级弱特征恢复。
    bool consolidateFeatureGraph = false;
    double featureGraphConsolidationGapLengthRatio = 3.0;
    double featureGraphConsolidationMinAlignment = 0.75;
    /// @}

    /// @name 位置后的硬过滤器和诊断
    /// @{
    /// 位置后的硬过滤器。局部误差预算约束近似双向分区包络，并使新样本保持在原始输入曲面附近。
    /// 零局部误差预算禁用这些测试。
    double minTriangleQuality = 0.0;
    double maxNormalDeviationDeg = 90.0;
    double maxLocalError = 0.0;
    double maxLocalErrorRatio = 0.0;
    bool preventLocalIntersections = false;
    bool verbose = false;
    /// 边坍缩后的固定拓扑切向质量改进轮次。零值准确保持单轮简化行为。
    int qualityRefinementIterations = 0;
    /// @}

    /// @name 纹理感知排序和硬 UV 图表约束
    /// @{
    /// 当输入网格带有纹理坐标时，启用逐角 UV 保护。
    bool preserveTexture = false;
    /// 加到 4x4 几何 QEM 代价中的标量局部 UV 失真权重。
    /// 这不会改变几何 quadric 维度或位置求解。
    double textureWeight = 1.0;
    /// 将相等逐角 UV 分组为局部图表时使用的相对容差。
    double textureSeamTolerance = 1e-8;
    /// 每个保留三角形允许的最小带符号 UV 面积比。
    double minTextureAreaRatio = 1e-8;
    /// @}
};

/// 简化期间收集的诊断信息。
///
/// 拒绝计数将每次被拒绝的坍缩尝试归因于第一个拒绝其首个位置候选的硬过滤器
///（位置按代价顺序尝试）。这些计数用于参数调节和回归测试，不应视为每个失败子检查的独立总数。
struct SimplifyReport {
    /// @name 网格大小摘要
    /// @{
    int initialVertices = 0;
    int initialFaces = 0;
    int finalVertices = 0;
    int finalFaces = 0;
    /// 被容忍为退化的输入面（顶点位置重复或数值上面积为零）。它们只贡献小型点 quadric 回退，而不是面 quadric；
    /// 会使退化面存活的坍缩会被合法性过滤器拒绝。该计数使被容忍的脏输入可见，而不是使运行失败。
    int degenerateInputFaces = 0;
    /// @}

    /// @name 队列和求解进度
    /// @{
    int collapsedEdges = 0;
    int rejectedCollapses = 0;
    /// 位置求解使用端点/中点回退候选，而不是稳定线性系统最优解的当前坍缩候选数量。
    int solverFallbacks = 0;
    /// 运行期间因补充或旧候选恢复触发的队列重建次数。不计入初始队列构建。
    int queueRebuilds = 0;
    /// @}

    /// @name 运行开始时记录的特征分析摘要
    /// @{
    int featureLoops = 0;
    int circularFeatureLoops = 0;
    int featureVertices = 0;
    int tracedFeatureEdges = 0;
    int untracedFeatureEdges = 0;
    int normalTensorFeatureEdges = 0;
    int normalTensorScoredVertices = 0;
    int smoothCurvatureFeatureEdges = 0;
    int smoothCurvatureScoredVertices = 0;
    int featureComponents = 0;
    int weakFeatureComponents = 0;
    int highConfidenceFeatureComponents = 0;
    int graphCleanupBridgedGaps = 0;
    int graphCleanupRemovedSpurs = 0;
    int graphCleanupMergedJunctions = 0;
    double maxNormalTensorPersistentScore = 0.0;
    double meanNormalTensorLocalScale = 0.0;
    double meanNormalTensorPersistence = 0.0;
    double maxSmoothCurvaturePersistentScore = 0.0;
    double meanSmoothCurvatureLocalScale = 0.0;
    double meanSmoothCurvaturePersistence = 0.0;
    double meanFeatureComponentConfidence = 0.0;
    double minFeatureComponentConfidence = 0.0;
    int inconsistentWindingEdges = 0;
    int graphCleanupSkippedByCap = 0;
    int circularRecoveryTruncated = 0;
    int featureNormalFilterIterationsCompleted = 0;
    int featureNormalFilterChangedFaces = 0;
    int featureNormalFilterPreservedEdges = 0;
    double meanFeatureNormalFilterAngularChangeDeg = 0.0;
    double maxFeatureNormalFilterAngularChangeDeg = 0.0;
    double meanFeatureNormalFilterEdgeIndicator = 0.0;
    double meanSmoothCurvatureScaleStability = 0.0;
    int graphConsolidationBridges = 0;
    int graphConsolidationSkippedByCap = 0;
    int junctionBranchPairs = 0;
    int ambiguousFeatureJunctions = 0;
    /// @}

    /// @name 当前坍缩候选的首次拒绝计数
    /// @{
    int featureRejectedCollapses = 0;
    int primitiveFeatureRejectedCollapses = 0;
    int genericFeatureRejectedCollapses = 0;
    int boundaryRejectedCollapses = 0;
    int topologyRejectedCollapses = 0;
    int normalFlipRejectedCollapses = 0;
    int qualityRejectedCollapses = 0;
    int selfIntersectionRejectedCollapses = 0;
    int curveBudgetRejectedCollapses = 0;
    int errorRejectedCollapses = 0;
    int projectedFeaturePlacements = 0;
    /// @}

    /// @name 最终终止和应用权重范围
    /// @{
    SimplifyTerminationReason terminationReason = SimplifyTerminationReason::NotStarted;
    double minAppliedLineWeight = 0.0;
    double maxAppliedLineWeight = 0.0;
    /// @}

    /// @name 固定拓扑第二轮质量精修
    /// @{
    int qualityRefinementIterationsCompleted = 0;
    int qualityRefinementAttemptedMoves = 0;
    int qualityRefinementAcceptedMoves = 0;
    /// @}

    /// @name 纹理感知坍缩诊断
    /// @{
    int textureRejectedCollapses = 0;
    int textureProtectedEdges = 0;
    /// 已接受但无法重新应用纹理更新计划的坍缩。
    /// 这表示内部不一致，正常情况下应保持为零。
    int textureApplyFailures = 0;
    /// @}
};

/// 将命令/用户字符串解析为权重模式。
/// @param[in] value 稳定的小写令牌。
/// @return 匹配的模式。
/// @throws std::invalid_argument 当令牌未知时抛出。
MANUMESH_API WeightMode parseWeightMode(const std::string& value);
/// 将权重模式转换为稳定的小写字符串表示。
/// @param[in] mode 待序列化的模式。
/// @return 稳定的 CLI/API 令牌。
MANUMESH_API std::string toString(WeightMode mode);
/// 将终止原因转换为稳定的小写字符串表示。
/// @param[in] reason 终止值。
/// @return 稳定的诊断令牌。
MANUMESH_API std::string toString(SimplifyTerminationReason reason);
/// 从稳定的小写字符串解析特征保护模式。
/// @param[in] value 稳定的小写令牌。
/// @return 匹配的策略。
/// @throws std::invalid_argument 当令牌未知时抛出。
MANUMESH_API FeatureProtectionMode parseFeatureProtectionMode(const std::string& value);
/// 将特征保护模式转换为稳定的小写字符串表示。
/// @param[in] mode 待序列化的策略。
/// @return 稳定的 CLI/API 令牌。
MANUMESH_API std::string toString(FeatureProtectionMode mode);

} // namespace manumesh::simplification
