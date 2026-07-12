#pragma once

#include "detail/CollapseLegality.h"
#include "detail/FeatureConstraints.h"
#include "detail/Quadrics.h"
#include "detail/SimplificationPolicies.h"
#include "detail/TextureProtection.h"

namespace manumesh::simplification {

enum class CollapseAttemptStatus {
    Accepted,
    FeatureRejected,
    BoundaryRejected,
    CurveBudgetRejected,
    TextureRejected,
    LegalityRejected,
};

struct CollapseAttemptInput {
    CollapseEdge edge;
    const Mat4& mergedQ;
    const std::vector<SolveResult>& placements;
    const SimplifyOptions& options;
    const SimplificationPolicies& policies;
    const std::vector<VertexState>& vertices;
    const std::vector<FaceState>& faces;
    const DynamicTopology& topology;
    const std::vector<int>& activeLoopCounts;
    const std::vector<FeatureCurveConstraint>& featureCurves;
    const FeatureConstraintPolicy& featurePolicy;
    const TextureProtection& textureProtection;
    const std::vector<FaceTexCoords>& faceTexCoords;
    const SpatialFaceIndex* spatialIndex = nullptr;
    const manumesh::detail::MeshDistanceIndex* referenceSurface = nullptr;
    double meshDiagonal = 0.0;
    double areaEps = 0.0;
    double minNormalDot = 0.0;
    double maxLocalError = 0.0;
};

struct CollapseAttemptResult {
    CollapseAttemptStatus status = CollapseAttemptStatus::LegalityRejected;
    Vec3 acceptedPosition = Vec3::Zero();
    bool projected = false;
    FeatureCollapseRejectKind featureRejectKind = FeatureCollapseRejectKind::None;
    TextureCollapseRejectReason textureRejectReason = TextureCollapseRejectReason::None;
    CollapseRejectReason legalityReason = CollapseRejectReason::None;

    bool accepted() const { return status == CollapseAttemptStatus::Accepted; }
};

CollapseAttemptResult evaluateCollapseAttempt(const CollapseAttemptInput& input);

} // namespace manumesh::simplification
