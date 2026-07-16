/**
 * @file src/simplification/detail/CollapseAttempt.h
 * @brief Declares collapse attempt facilities for ManuMesh's simplification module.
 * @ingroup manumesh_simplification
 *
 * @details This file is part of the feature-aware edge-collapse pipeline. Quadric costs rank candidates; topology, geometry, feature, boundary, error, and optional texture policies decide whether a placement may mutate the mesh.
 */

#pragma once

#include "detail/CollapseLegality.h"
#include "detail/FeatureConstraints.h"
#include "detail/Quadrics.h"
#include "detail/SimplificationPolicies.h"
#include "detail/TextureProtection.h"

namespace manumesh::simplification {

/// Coarse outcome of evaluating all placements for one current candidate.
enum class CollapseAttemptStatus {
    Accepted,
    FeatureRejected,
    BoundaryRejected,
    CurveBudgetRejected,
    TextureRejected,
    LegalityRejected,
};

/// Immutable mesh state, policies, and cached placements needed for evaluation.
struct CollapseAttemptInput {
    CollapseEdge edge;
    const Mat4& mergedQ;
    /// Placement candidates sorted by ascending quadric cost. Usually these
    /// come straight from the popped Candidate's cached solve.
    const SolveResult* placements = nullptr;
    int placementCount = 0;
    const SimplifyOptions& options;
    const SimplificationPolicies& policies;
    const std::vector<VertexState>& vertices;
    const std::vector<FaceState>& faces;
    const DynamicTopology& topology;
    const std::vector<int>& activeLoopCounts;
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

/// Accepted placement and prepared plans, or the first observable rejection class.
struct CollapseAttemptResult {
    CollapseAttemptStatus status = CollapseAttemptStatus::LegalityRejected;
    Vec3 acceptedPosition = Vec3::Zero();
    bool projected = false;
    FeatureCollapseRejectKind featureRejectKind = FeatureCollapseRejectKind::None;
    TextureCollapseRejectReason textureRejectReason = TextureCollapseRejectReason::None;
    CollapseRejectReason legalityReason = CollapseRejectReason::None;
    /// Texture update plan built for the accepted placement, so applyCollapse
    /// can reuse it instead of rebuilding the same plan.
    TextureUpdatePlan texturePlan;

    bool accepted() const { return status == CollapseAttemptStatus::Accepted; }
};

/**
 * @brief Tries cached placements in ascending cost order without mutating state.
 * @param[in] input Complete evaluation view.
 * @return First accepted placement, or a categorized rejection after all fail.
 */
CollapseAttemptResult evaluateCollapseAttempt(const CollapseAttemptInput& input);

} // namespace manumesh::simplification
