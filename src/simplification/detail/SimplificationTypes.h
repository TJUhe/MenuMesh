#pragma once

#include "core/Mesh.h"
#include "mesh_edit/detail/MeshEditTypes.h"

#include <array>
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
/// ownership, and queue invalidation. Bulky circle/ellipse fit parameters
/// live in a compact side table (FeaturePrimitiveFit) referenced through
/// primitiveFitId, so non-feature vertices carry no fit payload.
struct VertexState {
    // Geometry and QEM state.
    Vec3 p = Vec3::Zero();
    Mat4 q = Mat4::Zero();
    bool active = true;
    /// Queue-priority multiplier derived from feature evidence (adaptiveScale
    /// mode, Wang 2008 decoupling): it only scales the candidate ordering cost
    /// in the queue and never enters the quadric or the placement solve.
    double priorityScale = 1.0;

    // Feature ownership copied from FeatureGuidance.
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
    /// Index into the run's FeaturePrimitiveFit side table; -1 when the vertex
    /// owns no fitted circle/ellipse. Entries are immutable during a run and
    /// the keep vertex of a collapse keeps its own entry.
    int primitiveFitId = -1;

    // Incremented after collapse so queued candidates can detect stale endpoints.
    int version = 0;
};

/// Circle/ellipse fit parameters for one feature vertex. Stored out of line
/// (see VertexState::primitiveFitId) so the hot per-vertex state stays small:
/// only vertices on fitted primitive loops own an entry.
struct FeaturePrimitiveFit {
    Vec3 circleCenter = Vec3::Zero();
    Vec3 circleNormal = Vec3(0.0, 0.0, 1.0);
    double circleRadius = 0.0;
    Vec3 ellipseCenter = Vec3::Zero();
    Vec3 ellipseNormal = Vec3(0.0, 0.0, 1.0);
    Vec3 ellipseMajorAxis = Vec3(1.0, 0.0, 0.0);
    Vec3 ellipseMinorAxis = Vec3(0.0, 1.0, 0.0);
    double ellipseMajorRadius = 0.0;
    double ellipseMinorRadius = 0.0;
};

/// Fetches the vertex's primitive fit, or a zero-radius default when it owns
/// none. Projections treat zero radii as "no primitive" and pass positions
/// through unchanged, so the default is a safe no-op.
inline const FeaturePrimitiveFit&
primitiveFitOf(const VertexState& vertex, const std::vector<FeaturePrimitiveFit>& fits) {
    static const FeaturePrimitiveFit kNoFit{};
    if (vertex.primitiveFitId >= 0 && vertex.primitiveFitId < static_cast<int>(fits.size())) {
        return fits[static_cast<std::size_t>(vertex.primitiveFitId)];
    }
    return kNoFit;
}

using FaceState = mesh_edit::EditableFace;

/// Directed edge-collapse choice: keep one endpoint and remove the other.
struct CollapseEdge {
    int keep = -1;
    int remove = -1;
};

/// Candidate collapse placement and its evaluated quadric cost.
struct SolveResult {
    Vec3 position = Vec3::Zero();
    double cost = 0.0;
    bool usedFallback = false;
};

/// Priority-queue entry. The comparison is reversed for std::priority_queue so
/// the lowest-cost candidate is popped first. Cost ties fall back to the
/// canonical edge key (a, b) so pop order stays deterministic.
///
/// The entry carries the placement candidates solved at push time. They stay
/// valid exactly as long as the version stamps match: the merged quadric and
/// both endpoint positions can only change through a collapse, which bumps the
/// endpoint versions. This lets pop/tryCollapse reuse the solve instead of
/// re-running the 3x3 spectral analysis.
struct Candidate {
    double cost = 0.0;
    int a = -1;
    int b = -1;
    int versionA = 0;
    int versionB = 0;
    /// Cached placement candidates, sorted by ascending quadric cost.
    std::array<SolveResult, 4> placements{};
    int placementCount = 0;

    bool operator<(const Candidate& other) const {
        if (cost != other.cost) {
            return cost > other.cost;
        }
        if (a != other.a) {
            return a > other.a;
        }
        return b > other.b;
    }
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

enum class TextureCollapseRejectReason {
    None,
    ChartMismatch,
    TriangleFlip,
};

struct BoundaryCollapseDecision {
    bool allowed = true;
    bool boundaryEdge = false;
};

/// Static AABB tree over the segments of one feature polyline. Built once per
/// loop (only when the loop is long enough to matter) so closest-point
/// queries drop from O(L) to O(log L). Construction and traversal order are
/// deterministic: splits use nth_element with an index tie-break and queries
/// visit the nearer child first with strict-improvement pruning.
struct PolylineSegmentIndex {
    struct Node {
        Vec3 lo = Vec3::Zero();
        Vec3 hi = Vec3::Zero();
        int left = -1;
        int right = -1;
        int begin = 0;
        int end = 0;

        bool leaf() const { return left < 0; }
    };
    std::vector<Node> nodes;
    std::vector<int> segmentOrder;

    bool built() const { return !nodes.empty(); }
};

struct FeatureCurveConstraint {
    bool valid = false;
    bool closed = false;
    FeatureCurveKind primitive = FeatureCurveKind::Unknown;
    std::vector<Vec3> samples;
    /// Optional acceleration structure over the polyline segments; empty for
    /// short loops, which keep the plain linear scan.
    PolylineSegmentIndex segmentIndex;
};

} // namespace manumesh::simplification
