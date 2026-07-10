#pragma once

#include "core/Mesh.h"
#include "mesh_edit/detail/MeshEditTypes.h"

#include <vector>

namespace manumesh::simplification {

enum class FeatureCurveKind {
    Unknown,
    Circle,
    NearCircle,
    Ellipse,
    PolygonalLoop,
};

/// Mutable vertex record used only during one simplification run.
///
/// The fields are kept flat because the hot path reads them from several
/// modules. Conceptually they form three groups: geometry/QEM state, feature
/// ownership, and queue invalidation.
struct VertexState {
    // Geometry and QEM state.
    Vec3 p = Vec3::Zero();
    Mat4 q = Mat4::Zero();
    bool active = true;

    // Feature ownership and primitive-fit data copied from FeatureGuidance.
    bool isFeature = false;
    bool isBoundary = false;
    bool circularFeature = false;
    bool featureJunction = false;
    bool weakFeature = false;
    FeatureCurveKind featurePrimitive = FeatureCurveKind::Unknown;
    int featureLoopId = -1;
    int featureComponentId = -1;
    double featureConfidence = 0.0;
    Vec3 curveTangent = Vec3::Zero();
    Vec3 circleCenter = Vec3::Zero();
    Vec3 circleNormal = Vec3(0.0, 0.0, 1.0);
    double circleRadius = 0.0;
    Vec3 ellipseCenter = Vec3::Zero();
    Vec3 ellipseNormal = Vec3(0.0, 0.0, 1.0);
    Vec3 ellipseMajorAxis = Vec3(1.0, 0.0, 0.0);
    Vec3 ellipseMinorAxis = Vec3(0.0, 1.0, 0.0);
    double ellipseMajorRadius = 0.0;
    double ellipseMinorRadius = 0.0;

    // Incremented after collapse so queued candidates can detect stale endpoints.
    int version = 0;
};

using FaceState = mesh_edit::EditableFace;

/// Directed edge-collapse choice: keep one endpoint and remove the other.
struct CollapseEdge {
    int keep = -1;
    int remove = -1;
};

/// Priority-queue entry. The comparison is reversed for std::priority_queue so
/// the lowest-cost candidate is popped first.
struct Candidate {
    double cost = 0.0;
    int a = -1;
    int b = -1;
    int versionA = 0;
    int versionB = 0;

    bool operator<(const Candidate& other) const { return cost > other.cost; }
};

/// Candidate collapse placement and its evaluated quadric cost.
struct SolveResult {
    Vec3 position = Vec3::Zero();
    double cost = 0.0;
    bool usedFallback = false;
};

enum class FeatureCollapseRejectKind {
    None,
    Primitive,
    Generic,
};

enum class CollapseRejectReason {
    None,
    Topology,
    NormalFlip,
    TriangleQuality,
    SelfIntersection,
    LocalError,
};

struct BoundaryCollapseDecision {
    bool allowed = true;
    bool boundaryEdge = false;
};

struct FeatureCurveConstraint {
    bool valid = false;
    bool closed = false;
    FeatureCurveKind primitive = FeatureCurveKind::Unknown;
    std::vector<Vec3> samples;
};

} // namespace manumesh::simplification
