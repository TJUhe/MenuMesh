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

} // namespace

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

    // Rejection reporting attributes the whole attempt to the first hard
    // filter that rejected the first rejected placement candidate.
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
        // Constraint priority: boundary > feature. When preserveBoundary
        // limited this collapse to a boundary edge, re-clamp the placement to
        // the boundary segment in case the feature projection pulled it off.
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

} // namespace manumesh::simplification
