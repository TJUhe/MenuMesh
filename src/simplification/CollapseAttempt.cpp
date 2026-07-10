#include "detail/CollapseAttempt.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace manumesh::simplification {
namespace {

bool curveBudgetAllows(const CollapseAttemptInput& input, const Vec3& position) {
    if (!input.options.preserveFeatureCurves || input.options.maxFeatureCurveDeviationRatio <= 0.0) {
        return true;
    }
    const VertexState& a = input.vertices[input.edge.keep];
    const VertexState& b = input.vertices[input.edge.remove];
    if (!a.isFeature || !b.isFeature || a.featureLoopId < 0 || a.featureLoopId != b.featureLoopId ||
        a.featureLoopId >= static_cast<int>(input.featureCurves.size())) {
        return true;
    }
    const FeatureCurveConstraint& curve = input.featureCurves[a.featureLoopId];
    if (!curve.valid) {
        return true;
    }
    const double maxDistance = input.options.maxFeatureCurveDeviationRatio * std::max(1e-12, input.meshDiagonal);
    if (a.circularFeature || b.circularFeature) {
        const Vec3 projected = projectToCircle(position, a.circularFeature ? a : b);
        return (position - projected).squaredNorm() <= maxDistance * maxDistance;
    }
    if (a.featurePrimitive == FeatureCurveKind::Ellipse || b.featurePrimitive == FeatureCurveKind::Ellipse) {
        const Vec3 projected = projectToEllipse(position, a.featurePrimitive == FeatureCurveKind::Ellipse ? a : b);
        return (position - projected).squaredNorm() <= maxDistance * maxDistance;
    }
    if (curve.primitive != FeatureCurveKind::PolygonalLoop) {
        return true;
    }

    const int segmentCount =
        curve.closed ? static_cast<int>(curve.samples.size()) : std::max(0, static_cast<int>(curve.samples.size()) - 1);
    double bestDist2 = std::numeric_limits<double>::infinity();
    for (int i = 0; i < segmentCount; ++i) {
        const Vec3& p0 = curve.samples[i];
        const Vec3& p1 = curve.samples[(i + 1) % curve.samples.size()];
        const Vec3 edge = p1 - p0;
        const double len2 = edge.squaredNorm();
        Vec3 closest = p0;
        if (len2 > 1e-30) {
            const double t = std::clamp((position - p0).dot(edge) / len2, 0.0, 1.0);
            closest = p0 + t * edge;
        }
        bestDist2 = std::min(bestDist2, (position - closest).squaredNorm());
    }
    return std::isfinite(bestDist2) && bestDist2 <= maxDistance * maxDistance;
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

    if (input.placements.empty()) {
        result.status = CollapseAttemptStatus::LegalityRejected;
        result.legalityReason = CollapseRejectReason::Topology;
        return result;
    }

    const bool featureCurveCollapse = input.featurePolicy.isHardProtectedCollapse(input.edge, input.vertices);
    const bool tryFallbackPlacements =
        !featureCurveCollapse && (input.policies.legality.minTriangleQuality > 0.0 || input.maxLocalError > 0.0 ||
                                  input.policies.legality.preventLocalIntersections);
    const int placementCount = tryFallbackPlacements ? static_cast<int>(input.placements.size()) : 1;

    bool sawCurveBudgetReject = false;
    for (int placementIndex = 0; placementIndex < placementCount; ++placementIndex) {
        Vec3 collapsePosition = input.placements[placementIndex].position;
        projectBoundaryPlacement({input.edge, boundaryDecision, input.vertices}, collapsePosition);
        if (!curveBudgetAllows(input, collapsePosition)) {
            sawCurveBudgetReject = true;
            continue;
        }

        const bool projected =
            input.featurePolicy.projectPlacement({input.edge, input.vertices, input.featureCurves}, collapsePosition);
        const CollapseRejectReason rejectReason = collapseRejectReason(
            {input.edge,
             collapsePosition,
             {input.faces, input.vertices, input.topology},
             input.areaEps,
             input.policies.legality.minTriangleQuality,
             input.minNormalDot,
             input.maxLocalError,
             input.policies.legality.preventLocalIntersections,
             input.spatialIndex}
        );
        if (rejectReason == CollapseRejectReason::None) {
            result.status = CollapseAttemptStatus::Accepted;
            result.acceptedPosition = collapsePosition;
            result.projected = projected;
            return result;
        }
        if (result.legalityReason == CollapseRejectReason::None) {
            result.legalityReason = rejectReason;
        }
    }

    if (result.legalityReason != CollapseRejectReason::None) {
        result.status = CollapseAttemptStatus::LegalityRejected;
        return result;
    }
    if (sawCurveBudgetReject) {
        result.status = CollapseAttemptStatus::CurveBudgetRejected;
        return result;
    }
    result.status = CollapseAttemptStatus::LegalityRejected;
    result.legalityReason = CollapseRejectReason::Topology;
    return result;
}

} // namespace manumesh::simplification
