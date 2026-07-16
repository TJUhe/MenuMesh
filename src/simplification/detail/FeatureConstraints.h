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

/**
 * @brief Projects a point to a fitted circle using deterministic radial fallback axes.
 */
Vec3 projectToCircle(const Vec3& p, const VertexState& feature, const FeaturePrimitiveFit& fit);
/**
 * @brief Projects a point to a fitted ellipse in its orthonormal fit frame.
 */
Vec3 projectToEllipse(const Vec3& p, const VertexState& feature, const FeaturePrimitiveFit& fit);
/**
 * @brief Refreshes the tangent after a circular constrained vertex moves.
 */
void refreshCircularTangent(VertexState& vertex, const FeaturePrimitiveFit& fit);
/**
 * @brief Refreshes the tangent after an elliptical constrained vertex moves.
 */
void refreshEllipseTangent(VertexState& vertex, const FeaturePrimitiveFit& fit);

/**
 * @brief Loops with at least this many segments get a PolylineSegmentIndex; shorter
 * loops keep the plain linear scan, whose constant factor is smaller.
 */
inline constexpr int kPolylineIndexMinSegments = 64;

/**
 * @brief Builds curve.segmentIndex when the polyline is long enough. Call once per
 * loop after the samples are final; queries then run in O(log L).
 * Builds a BVH-like segment index for long polygonal feature curves.
 */
void buildPolylineSegmentIndex(FeatureCurveConstraint& curve);

/**
 * @brief Closest point on the feature polyline (all segments, closed loops wrap).
 * Uses the prebuilt segment index when available, otherwise scans linearly.
 * outDistanceSquared receives +infinity when the curve has no segments.
 * @return Closest constrained-curve point and writes its squared distance.
 */
Vec3 closestPointOnFeatureCurve(const FeatureCurveConstraint& curve, const Vec3& position, double& outDistanceSquared);

/**
 * @brief Returns true when a placement stays within the feature-curve deviation
 * budget (maxFeatureCurveDeviationRatio) implied by the endpoints' shared
 * feature curve. Pass the same vertex for both endpoints to validate a
 * single-vertex relocation, e.g. during quality refinement.
 */
bool featureCurveBudgetAllows(
    const VertexState& a,
    const VertexState& b,
    const std::vector<FeatureCurveConstraint>& featureCurves,
    const std::vector<FeaturePrimitiveFit>& primitiveFits,
    const SimplifyOptions& options,
    double meshDiagonal,
    const Vec3& position
);

/**
 * @brief Edge ownership and loop budgets needed for a hard feature decision.
 */
struct FeatureCollapseInput {
    CollapseEdge edge;
    const std::vector<VertexState>& vertices;
    const std::vector<int>& activeLoopCounts;
};

/**
 * @brief Accepted raw placement and curve data needed for constrained projection.
 */
struct FeatureProjectionInput {
    CollapseEdge edge;
    const std::vector<VertexState>& vertices;
    const std::vector<FeatureCurveConstraint>& curves;
    const std::vector<FeaturePrimitiveFit>& primitiveFits;
};

/**
 * @brief Stateless hard feature-policy evaluator derived from SimplifyOptions.
 */
class FeatureConstraintPolicy {
public:
    /** @brief Binds the policy to immutable simplification options. */
    explicit FeatureConstraintPolicy(const SimplifyOptions& options);

    /** @brief Classifies the feature-protection reason for an edge collapse. */
    FeatureCollapseRejectKind collapseRejectKind(const FeatureCollapseInput& input) const;
    /** @brief Reports whether a vertex must remain fixed by hard protection. */
    bool isHardProtectedVertex(int vertex, const std::vector<VertexState>& vertices) const;
    /** @brief Reports whether hard protection forbids collapsing an edge. */
    bool isHardProtectedCollapse(CollapseEdge edge, const std::vector<VertexState>& vertices) const;
    /** @brief Projects an accepted raw placement onto its protected primitive. */
    bool projectPlacement(const FeatureProjectionInput& input, Vec3& position) const;

private:
    const SimplifyOptions& options_;
};

} // namespace manumesh::simplification
