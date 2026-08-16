/**
 * @file src/simplification/detail/CollapseAttempt.h
 * @brief 声明单个边坍缩候选的无副作用评估过程。
 * @ingroup manumesh_simplification
 *
 * @details 评估返回接受计划或首个可观察拒绝原因，真正的状态提交由 SimplificationRun 负责。
 */

#pragma once

#include "detail/CollapseLegality.h"
#include "detail/FeatureConstraints.h"
#include "detail/Quadrics.h"
#include "detail/SimplificationPolicies.h"
#include "detail/TextureProtection.h"

namespace manumesh {
namespace simplification {

/**
 * @brief 评估当前候选所有放置后的粗粒度结果。
 */
enum class CollapseAttemptStatus {
    Accepted,
    FeatureRejected,
    BoundaryRejected,
    CurveBudgetRejected,
    TextureRejected,
    LegalityRejected,
};

/**
 * @brief 评估所需的不可变网格状态、策略和缓存放置。
 */
struct CollapseAttemptInput {
    CollapseEdge edge;
    const Mat4& mergedQ;
    /**
     * @brief 按二次误差代价升序排列的放置候选，通常直接来自弹出 Candidate 时缓存的求解结果。
     */
    const SolveResult* placements = nullptr;
    int placementCount = 0;
    const SimplifyOptions& options;
    const SimplificationPolicies& policies;
    const std::vector<VertexState>& vertices;
    const std::vector<FaceState>& faces;
    const DynamicTopology& topology;
    const std::vector<int>& activeLoopCounts;
    const FeatureConstraintGraph& featureConstraints;
    const std::vector<FeatureCurveConstraint>& featureCurves;
    const std::vector<FeaturePrimitiveFit>& primitiveFits;
    const FeatureConstraintPolicy& featurePolicy;
    const TextureProtection& textureProtection;
    const std::vector<FaceTexCoords>& faceTexCoords;
    const SpatialFaceIndex* spatialIndex = nullptr;
    const manumesh::common::MeshDistanceIndex* referenceSurface = nullptr;
    double meshDiagonal = 0.0;
    double areaEps = 0.0;
    double minNormalDot = 0.0;
    double maxLocalError = 0.0;
};

/**
 * @brief 接受的放置及已准备好的计划，或第一个可观察到的拒绝类别。
 */
struct CollapseAttemptResult {
    CollapseAttemptStatus status = CollapseAttemptStatus::LegalityRejected;
    Vec3 acceptedPosition = Vec3::Zero();
    bool projected = false;
    FeatureCollapseRejectKind featureRejectKind = FeatureCollapseRejectKind::None;
    TextureCollapseRejectReason textureRejectReason = TextureCollapseRejectReason::None;
    CollapseRejectReason legalityReason = CollapseRejectReason::None;
    /**
     * @brief 为接受的放置构建的纹理更新计划，applyCollapse 可以直接复用，避免再次构建相同计划。
     */
    TextureUpdatePlan texturePlan;

    /** @brief 报告评估是否产生了可应用的放置。*/
    bool accepted() const { return status == CollapseAttemptStatus::Accepted; }
};

/**
 * @brief 按代价升序尝试缓存的放置而不修改状态。
 * @param[in] input 完整的评估视图。
 * @return 第一个被接受的放置；若全部失败，则返回分类后的拒绝结果。
 */
CollapseAttemptResult evaluateCollapseAttempt(const CollapseAttemptInput& input);

} // namespace simplification
} // namespace manumesh
