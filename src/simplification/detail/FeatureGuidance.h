/**
 * @file src/simplification/detail/FeatureGuidance.h
 * @brief 声明 ManuMesh 的简化模块的特征引导功能。
 * @ingroup manumesh_simplification
 *
 * @details 本文件属于带特征感知的边折叠流水线。二次误差代价用于排序候选；拓扑、几何、特征、边界、误差及可选纹理策略共同决定一个放置是否可以修改网格。
 */

#pragma once

#include "algorithms/feature_detection/FeatureTypes.h"
#include "algorithms/simplification/SimplificationTypes.h"
#include "core/Mesh.h"
#include "detail/FeatureConstraintGraph.h"
#include "detail/SimplificationTypes.h"

#include <vector>

namespace manumesh {
namespace feature {
struct FeatureAnalysis;
} // namespace feature
} // namespace manumesh

namespace manumesh {
namespace simplification {

struct FeatureDetectionPolicy;

/**
 * @brief 复制到一个简化顶点上的软特征属性。
 */
struct FeatureVertexGuidance {
    bool isFeature = false;
    bool circular = false;
    bool junction = false;
    bool weakFeature = false;
    FeatureCurveKind primitive = FeatureCurveKind::Unknown;
    int loopId = -1;
    int componentId = -1;
    std::vector<int> loopIds;
    std::vector<int> componentIds;
    double confidence = 0.0;
    Vec3 tangent = Vec3::Zero();
    Vec3 circleCenter = Vec3::Zero();
    Vec3 circleNormal = Vec3(0.0, 0.0, 1.0);
    double circleRadius = 0.0;
    Vec3 ellipseCenter = Vec3::Zero();
    Vec3 ellipseNormal = Vec3(0.0, 0.0, 1.0);
    Vec3 ellipseMajorAxis = Vec3(1.0, 0.0, 0.0);
    Vec3 ellipseMinorAxis = Vec3(0.0, 1.0, 0.0);
    double ellipseMajorRadius = 0.0;
    double ellipseMinorRadius = 0.0;
};

/**
 * @brief 复制到 SimplifyReport 的特征分析诊断汇总。
 */
struct FeatureGuidanceSummary {
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
    feature::FeatureNormalFilterReport normalFilter;
    double meanSmoothCurvatureScaleStability = 0.0;
    int graphConsolidationBridges = 0;
    int graphConsolidationSkippedByCap = 0;
    int junctionBranchPairs = 0;
    int ambiguousFeatureJunctions = 0;
};

/**
 * @brief 从一次特征分析派生的逐顶点软引导和解析图元拟合。
 */
struct FeatureGuidance {
    bool enabled = false;
    std::vector<FeatureVertexGuidance> vertices;
    std::vector<FeatureCurveConstraint> curves;
    FeatureConstraintGraph constraints;
    FeatureGuidanceSummary summary;
};

/**
 * @brief 与放置二次误差解耦的队列优先级敏感度因子。
 */
struct FeatureWeightScores {
    std::vector<double> values;
    int normalTensorScoredVertices = 0;
    double maxNormalTensorPersistentScore = 0.0;
    double meanNormalTensorLocalScale = 0.0;
    double meanNormalTensorPersistence = 0.0;
};

/**
 * @brief 检测特征并将其转换为简化引导。
 */
FeatureGuidance buildFeatureGuidance(const Mesh& mesh, const FeatureDetectionPolicy& policy);
FeatureGuidance buildFeatureGuidance(
    const Mesh& mesh, const FeatureDetectionPolicy& policy, const feature::FeatureAnalysis* precomputed
);

/**
 * @brief 计算可选的特征敏感队列权重，不修改二次误差矩阵。
 */
FeatureWeightScores computeFeatureWeightScores(
    const Mesh& mesh, const SimplifyOptions& options, const feature::FeatureAnalysis* precomputed = nullptr
);

/**
 * @brief 将特征诊断复制到运行报告中。
 */
void applyFeatureGuidanceSummary(const FeatureGuidanceSummary& summary, SimplifyReport& report);

} // namespace simplification
} // namespace manumesh
