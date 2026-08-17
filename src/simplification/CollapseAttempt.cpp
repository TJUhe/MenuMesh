/**
 * @file src/simplification/CollapseAttempt.cpp
 * @brief 在提交拓扑修改前评估一个边坍缩候选。
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

namespace manumesh {
namespace simplification {
namespace {

bool curveBudgetAllows(const CollapseAttemptInput& input, const Vec3& position) {
    return featureCurveBudgetAllows(
        input.vertices[input.edge.keep],
        input.vertices[input.edge.remove],
        input.featureCurves,
        input.primitiveFits,
        input.options,
        input.meshDiagonal,
        position,
        &input.featureConstraints,
        input.edge
    );
}

} // 结束匿名命名空间

CollapseAttemptResult evaluateCollapseAttempt(const CollapseAttemptInput& input) {
    CollapseAttemptResult result;
    if (input.edge.keep < 0 || input.edge.remove < 0 || input.edge.keep == input.edge.remove ||
        input.edge.keep >= static_cast<int>(input.vertices.size()) ||
        input.edge.remove >= static_cast<int>(input.vertices.size()) ||
        input.edge.keep >= static_cast<int>(input.topology.vertexFaces.size()) ||
        input.edge.remove >= static_cast<int>(input.topology.vertexFaces.size()) ||
        !input.vertices[static_cast<std::size_t>(input.edge.keep)].active ||
        !input.vertices[static_cast<std::size_t>(input.edge.remove)].active) {
        result.status = CollapseAttemptStatus::LegalityRejected;
        result.legalityReason = CollapseRejectReason::Topology;
        return result;
    }

    const FeatureCollapseRejectKind featureRejectKind = input.featurePolicy.collapseRejectKind(
        {input.edge, input.vertices, input.activeLoopCounts, input.featureConstraints}
    );
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

    // Every cached placement is bounded (currently at most four). Trying all
    // of them is required for hard feature curves too: projection can map the
    // lowest-QEM candidate to an illegal point while an endpoint remains legal.
    const int placementCount = input.placementCount;
    const bool preservesTopology = collapseWouldPreserveLinkCondition(
        input.edge.keep, input.edge.remove, input.faces, input.vertices, input.topology
    );

    // 多个放置共享同一局部拓扑；先收集一次关联面，避免每个放置都重复分配和排序。
    const CollapseLegalityInput legalityBase{
        input.edge,
        Vec3::Zero(),
        {input.faces, input.vertices, input.topology},
        input.areaEps,
        input.policies.legality.minTriangleQuality,
        input.minNormalDot,
        input.maxLocalError,
        input.policies.legality.preventLocalIntersections,
        input.spatialIndex,
        input.referenceSurface
    };
    const std::vector<int> touchedFaces =
        preservesTopology ? collectCollapseTouchedFaces(legalityBase) : std::vector<int>();

    // 拒绝报告将整个尝试归因于第一个被硬过滤器拒绝的放置候选对应的首个过滤器。
    CollapseAttemptStatus firstRejectStatus = CollapseAttemptStatus::Accepted;
    for (int placementIndex = 0; placementIndex < placementCount; ++placementIndex) {
        Vec3 collapsePosition = input.placements[placementIndex].position;
        projectBoundaryPlacement(
            {input.edge, boundaryDecision, input.vertices, input.faces, input.topology}, collapsePosition
        );
        const bool projected = input.featurePolicy.projectPlacement(
            {input.edge, input.vertices, input.featureCurves, input.primitiveFits, input.featureConstraints},
            collapsePosition
        );
        // 约束优先级为：边界 > 特征。当 preserveBoundary 将此次折叠限制为边界边时，若特征投影把放置点拉离边界线段，则重新将其夹回边界线段。
        if (projected && boundaryDecision.boundaryEdge) {
            projectBoundaryPlacement(
                {input.edge, boundaryDecision, input.vertices, input.faces, input.topology}, collapsePosition
            );
        }
        // The budget applies to the position that will actually be committed.
        // Checking before feature/boundary projection can both reject a legal
        // projected point and accept a final point that has left the curve.
        if (!curveBudgetAllows(input, collapsePosition)) {
            if (firstRejectStatus == CollapseAttemptStatus::Accepted) {
                firstRejectStatus = CollapseAttemptStatus::CurveBudgetRejected;
            }
            continue;
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
        CollapseLegalityInput legalityInput = legalityBase;
        legalityInput.newPosition = collapsePosition;
        const CollapseRejectReason rejectReason = preservesTopology
                                                      ? collapsePlacementRejectReason(legalityInput, &touchedFaces)
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

} // namespace simplification
} // namespace manumesh
