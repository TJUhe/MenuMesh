/**
 * @file src/simplification/CollapseAttempt.cpp
 * @brief 实现 ManuMesh 的简化模块的折叠尝试功能。
 * @ingroup manumesh_simplification
 *
 * @details 评估当前边候选在修改网格前必须通过的全部策略。
 * @algorithm 按 QEM 代价升序尝试放置候选。每个候选依次通过特征投影/预算、边界策略、可选 UV 规划、拓扑、法向/质量/误差以及自交检查；第一个完全合法的放置会返回其已准备好的修改计划。
 * @invariants 评估期间不会修改网格、拓扑、UV 或空间索引状态。
 */

#include "detail/CollapseAttempt.h"

#include "detail/Placement.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace manumesh::simplification {
namespace {

bool curveBudgetAllows(const CollapseAttemptInput& input, const Vec3& position) {
    return featureCurveBudgetAllows(
        input.vertices[input.edge.keep],
        input.vertices[input.edge.remove],
        input.featureCurves,
        input.primitiveFits,
        input.options,
        input.meshDiagonal,
        position
    );
}

} // 结束匿名命名空间

CollapseAttemptResult evaluateCollapseAttempt(const CollapseAttemptInput& input) {
    CollapseAttemptResult result;

    const FeatureCollapseRejectKind featureRejectKind =
        input.featurePolicy.collapseRejectKind({input.edge, input.vertices, input.activeLoopCounts});
    if (featureRejectKind != FeatureCollapseRejectKind::None) {
        result.status = CollapseAttemptStatus::FeatureRejected;
        result.featureRejectKind = featureRejectKind;
        return result;
    }

    const BoundaryCollapseDecision boundaryDecision =
        boundaryCollapseDecision({input.edge, input.faces, input.vertices, input.topology, input.options});
    if (!boundaryDecision.allowed) {
        result.status = CollapseAttemptStatus::BoundaryRejected;
        return result;
    }

    if (input.placementCount <= 0 || input.placements == nullptr) {
        result.status = CollapseAttemptStatus::LegalityRejected;
        result.legalityReason = CollapseRejectReason::Topology;
        return result;
    }

    const bool featureCurveCollapse = input.featurePolicy.isHardProtectedCollapse(input.edge, input.vertices);
    const bool tryFallbackPlacements =
        !featureCurveCollapse &&
        (input.policies.legality.minTriangleQuality > 0.0 || input.maxLocalError > 0.0 || input.minNormalDot > -1.0 ||
         input.policies.legality.preventLocalIntersections || input.textureProtection.active());
    const int placementCount = tryFallbackPlacements ? input.placementCount : 1;
    const bool preservesTopology = collapseWouldPreserveLinkCondition(
        input.edge.keep, input.edge.remove, input.faces, input.vertices, input.topology
    );

    // 拒绝报告将整个尝试归因于第一个被硬过滤器拒绝的放置候选对应的首个过滤器。
    CollapseAttemptStatus firstRejectStatus = CollapseAttemptStatus::Accepted;
    for (int placementIndex = 0; placementIndex < placementCount; ++placementIndex) {
        Vec3 collapsePosition = input.placements[placementIndex].position;
        projectBoundaryPlacement(
            {input.edge, boundaryDecision, input.vertices, input.faces, input.topology}, collapsePosition
        );
        if (!curveBudgetAllows(input, collapsePosition)) {
            if (firstRejectStatus == CollapseAttemptStatus::Accepted) {
                firstRejectStatus = CollapseAttemptStatus::CurveBudgetRejected;
            }
            continue;
        }

        const bool projected = input.featurePolicy.projectPlacement(
            {input.edge, input.vertices, input.featureCurves, input.primitiveFits}, collapsePosition
        );
        // 约束优先级为：边界 > 特征。当 preserveBoundary 将此次折叠限制为边界边时，若特征投影把放置点拉离边界线段，则重新将其夹回边界线段。
        if (projected && boundaryDecision.boundaryEdge) {
            projectBoundaryPlacement(
                {input.edge, boundaryDecision, input.vertices, input.faces, input.topology}, collapsePosition
            );
        }
        TextureUpdatePlan texturePlan = input.textureProtection.buildPlan(
            input.edge, collapsePosition, input.faces, input.vertices, input.topology, input.faceTexCoords
        );
        if (!texturePlan.evaluation.allowed()) {
            if (firstRejectStatus == CollapseAttemptStatus::Accepted) {
                firstRejectStatus = CollapseAttemptStatus::TextureRejected;
                result.textureRejectReason = texturePlan.evaluation.rejectReason;
            }
            continue;
        }
        const CollapseRejectReason rejectReason = preservesTopology
                                                      ? collapsePlacementRejectReason(
                                                            {input.edge,
                                                             collapsePosition,
                                                             {input.faces, input.vertices, input.topology},
                                                             input.areaEps,
                                                             input.policies.legality.minTriangleQuality,
                                                             input.minNormalDot,
                                                             input.maxLocalError,
                                                             input.policies.legality.preventLocalIntersections,
                                                             input.spatialIndex,
                                                             input.referenceSurface}
                                                        )
                                                      : CollapseRejectReason::Topology;
        if (rejectReason == CollapseRejectReason::None) {
            result.status = CollapseAttemptStatus::Accepted;
            result.acceptedPosition = collapsePosition;
            result.projected = projected;
            result.texturePlan = std::move(texturePlan);
            return result;
        }
        if (firstRejectStatus == CollapseAttemptStatus::Accepted) {
            firstRejectStatus = CollapseAttemptStatus::LegalityRejected;
            result.legalityReason = rejectReason;
        }
    }

    if (firstRejectStatus != CollapseAttemptStatus::Accepted) {
        result.status = firstRejectStatus;
        return result;
    }
    result.status = CollapseAttemptStatus::LegalityRejected;
    result.legalityReason = CollapseRejectReason::Topology;
    return result;
}

} // 结束 manumesh::simplification 命名空间
