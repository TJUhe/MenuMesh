/**
 * @file src/simplification/detail/FeatureGuidance.h
 * @brief 声明从特征分析派生的逐顶点引导和曲线约束。
 * @ingroup manumesh_simplification
 *
 * @details 该适配层把检测结果转换为简化器自己的紧凑数据，不把 FeatureAnalysis 传播到坍缩热循环。
 */

#pragma once

#include "algorithms/feature_detection/FeatureTypes.h"
#include "algorithms/simplification/SimplificationTypes.h"
#include "core/ExecutionOptions.h"
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
    int primitiveFitId = -1;
};

/**
 * @brief 从一次特征分析派生的逐顶点软引导、曲线约束和可选几何基元旁表。
 */
struct FeatureGuidance {
    bool enabled = false;
    std::vector<FeatureVertexGuidance> vertices;
    std::vector<FeatureCurveConstraint> curves;
    /** @brief 解析圆/椭圆顶点与 loop 拥有条目；顶点引导和曲线约束各自用 primitiveFitId 引用该表。 */
    std::vector<FeaturePrimitiveFit> primitiveFits;
    FeatureConstraintGraph constraints;
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
    int smoothCurvatureScoredVertices = 0;
    double maxSmoothCurvaturePersistentScore = 0.0;
    double meanSmoothCurvatureLocalScale = 0.0;
    double meanSmoothCurvaturePersistence = 0.0;
    double meanSmoothCurvatureScaleStability = 0.0;
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
    const Mesh& mesh,
    const SimplifyOptions& options,
    const feature::FeatureAnalysis* precomputed = nullptr,
    const ExecutionOptions& executionOptions = {}
);

/**
 * @brief 将特征分析的稳定计数复制到兼容运行报告中。
 */
void applyFeatureAnalysisReport(
    const feature::FeatureAnalysis& analysis, const FeatureGuidance& guidance, SimplifyReport& report
);

} // namespace simplification
} // namespace manumesh
