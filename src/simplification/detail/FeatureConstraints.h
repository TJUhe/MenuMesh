/**
 * @file src/simplification/detail/FeatureConstraints.h
 * @brief Declares feature constraints facilities for ManuMesh's simplification module.
 * @ingroup manumesh_simplification
 *
 * @details This file is part of the feature-aware edge-collapse pipeline. Quadric costs rank candidates; topology, geometry, feature, boundary, error, and optional texture policies decide whether a placement may mutate the mesh.
 */

#pragma once

#include "algorithms/simplification/SimplificationTypes.h"
#include "detail/SimplificationTypes.h"
#include "mesh_edit/detail/DynamicTopology.h"

#include <vector>

namespace manumesh::simplification {

/// Projects a point to a fitted circle using deterministic radial fallback axes.
Vec3 projectToCircle(const Vec3& p, const VertexState& feature, const FeaturePrimitiveFit& fit);
/// Projects a point to a fitted ellipse in its orthonormal fit frame.
Vec3 projectToEllipse(const Vec3& p, const VertexState& feature, const FeaturePrimitiveFit& fit);
/// Refreshes the tangent after a circular constrained vertex moves.
void refreshCircularTangent(VertexState& vertex, const FeaturePrimitiveFit& fit);
/// Refreshes the tangent after an elliptical constrained vertex moves.
void refreshEllipseTangent(VertexState& vertex, const FeaturePrimitiveFit& fit);

/// Loops with at least this many segments get a PolylineSegmentIndex; shorter
/// loops keep the plain linear scan, whose constant factor is smaller.
inline constexpr int kPolylineIndexMinSegments = 64;

/// Builds curve.segmentIndex when the polyline is long enough. Call once per
/// loop after the samples are final; queries then run in O(log L).
/// Builds a BVH-like segment index for long polygonal feature curves.
void buildPolylineSegmentIndex(FeatureCurveConstraint& curve);

/// Closest point on the feature polyline (all segments, closed loops wrap).
/// Uses the prebuilt segment index when available, otherwise scans linearly.
/// outDistanceSquared receives +infinity when the curve has no segments.
/// @return Closest constrained-curve point and writes its squared distance.
Vec3 closestPointOnFeatureCurve(const FeatureCurveConstraint& curve, const Vec3& position, double& outDistanceSquared);

/// Returns true when a placement stays within the feature-curve deviation
/// budget (maxFeatureCurveDeviationRatio) implied by the endpoints' shared
/// feature curve. Pass the same vertex for both endpoints to validate a
/// single-vertex relocation, e.g. during quality refinement.
bool featureCurveBudgetAllows(
    const VertexState& a,
    const VertexState& b,
    const std::vector<FeatureCurveConstraint>& featureCurves,
    const std::vector<FeaturePrimitiveFit>& primitiveFits,
    const SimplifyOptions& options,
    double meshDiagonal,
    const Vec3& position
);

/// Edge ownership and loop budgets needed for a hard feature decision.
struct FeatureCollapseInput {
    CollapseEdge edge;
    const std::vector<VertexState>& vertices;
    const std::vector<int>& activeLoopCounts;
};

/// Accepted raw placement and curve data needed for constrained projection.
struct FeatureProjectionInput {
    CollapseEdge edge;
    const std::vector<VertexState>& vertices;
    const std::vector<FeatureCurveConstraint>& curves;
    const std::vector<FeaturePrimitiveFit>& primitiveFits;
};

/// Stateless hard feature-policy evaluator derived from SimplifyOptions.
class FeatureConstraintPolicy {
public:
    explicit FeatureConstraintPolicy(const SimplifyOptions& options);

    FeatureCollapseRejectKind collapseRejectKind(const FeatureCollapseInput& input) const;
    bool isHardProtectedVertex(int vertex, const std::vector<VertexState>& vertices) const;
    bool isHardProtectedCollapse(CollapseEdge edge, const std::vector<VertexState>& vertices) const;
    bool projectPlacement(const FeatureProjectionInput& input, Vec3& position) const;

private:
    const SimplifyOptions& options_;
};

} // namespace manumesh::simplification
