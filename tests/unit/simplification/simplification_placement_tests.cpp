#include "SimplificationTestSupport.h"
#include "algorithms/simplification/QEMSimplifier.h"
#include "core/MeshGenerators.h"
#include "core/MeshTopology.h"
#include "simplification/detail/FeatureConstraints.h"
#include "simplification/detail/Placement.h"
#include "simplification/detail/Quadrics.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <gtest/gtest.h>
#include <limits>
#include <vector>

namespace {

using manumesh::Mesh;
using manumesh::Vec3;
using manumesh::mesh_edit::DynamicTopology;
using manumesh::simplification::BoundaryCollapseDecision;
using manumesh::simplification::FaceState;
using manumesh::simplification::FeatureCurveConstraint;
using manumesh::simplification::FeatureCurveKind;
using manumesh::simplification::SolveResult;
using manumesh::simplification::VertexState;

double evaluateCost(const manumesh::Mat4& q, const Vec3& p) { return manumesh::simplification::evaluateQuadric(q, p); }

/// Small open strip whose top boundary chain is prev - keep - remove - next.
/// The boundary half-edges incident to keep/remove are exactly that chain.
struct BoundaryStrip {
    std::vector<VertexState> vertices;
    std::vector<FaceState> faces;
    DynamicTopology topology;

    explicit BoundaryStrip(const std::array<Vec3, 4>& chain)
        : topology(makeState(chain), 6) {}

    static constexpr int kKeep = 1;
    static constexpr int kRemove = 2;

private:
    std::vector<FaceState> makeState(const std::array<Vec3, 4>& chain) {
        const Vec3 interiorA(0.0, chain[1].y() - 1.0, 0.0);
        const Vec3 interiorB(1.5, chain[1].y() - 1.0, 0.0);
        const std::array<Vec3, 6> points = {chain[0], chain[1], chain[2], chain[3], interiorA, interiorB};
        vertices.resize(points.size());
        for (std::size_t i = 0; i < points.size(); ++i) {
            vertices[i].p = points[i];
            vertices[i].isBoundary = true;
        }
        vertices[4].isBoundary = true;
        vertices[5].isBoundary = true;
        faces.resize(4);
        faces[0].v = {0, 4, 1};
        faces[1].v = {1, 4, 5};
        faces[2].v = {1, 5, 2};
        faces[3].v = {2, 5, 3};
        return faces;
    }
};

/// Lindstrom-Turk boundary objective over the directed chain half-edges
/// (1,0), (2,1), (3,2): fB(v) = || e2 - v x e1 ||^2.
double boundaryAreaObjective(const std::array<Vec3, 4>& chain, const Vec3& v) {
    const Vec3 e1 = (chain[0] - chain[1]) + (chain[1] - chain[2]) + (chain[2] - chain[3]);
    const Vec3 e2 = chain[1].cross(chain[0]) + chain[2].cross(chain[1]) + chain[3].cross(chain[2]);
    return (e2 - v.cross(e1)).squaredNorm();
}

Vec3 clampToSegmentReference(const Vec3& position, const Vec3& a, const Vec3& b) {
    const Vec3 edge = b - a;
    const double len2 = edge.squaredNorm();
    if (len2 <= 1e-30) {
        return 0.5 * (a + b);
    }
    const double t = std::clamp((position - a).dot(edge) / len2, 0.0, 1.0);
    return a + t * edge;
}

double distanceToSegments(const Vec3& point, const std::vector<std::array<Vec3, 2>>& segments) {
    double best2 = std::numeric_limits<double>::infinity();
    for (const std::array<Vec3, 2>& segment : segments) {
        const Vec3 closest = clampToSegmentReference(point, segment[0], segment[1]);
        best2 = std::min(best2, (point - closest).squaredNorm());
    }
    return std::sqrt(best2);
}

std::vector<std::array<Vec3, 2>> collectBoundarySegments(const Mesh& mesh, double* maxSegmentLength) {
    std::vector<std::array<Vec3, 2>> segments;
    const manumesh::Result<manumesh::MeshTopology> topologyResult = manumesh::MeshTopology::build(mesh);
    if (!topologyResult.ok()) {
        return segments;
    }
    for (const manumesh::TopologyEdge& edge : topologyResult.value().edges()) {
        if (!edge.boundary()) {
            continue;
        }
        const Vec3& a = mesh.vertices[edge.vertices[0]];
        const Vec3& b = mesh.vertices[edge.vertices[1]];
        segments.push_back({a, b});
        if (maxSegmentLength) {
            *maxSegmentLength = std::max(*maxSegmentLength, (b - a).norm());
        }
    }
    return segments;
}

} // namespace

// GH97 fallback level 2: when the 3x3 system is rank deficient (straight
// crease: two independent planes), the 1D optimum along the edge must beat
// endpoints and midpoint. Quadric: planes z = 0 and y = 0, so
// f(x, y, z) = y^2 + z^2 and A = diag(0, 1, 1) has rank 2.
TEST(PlacementSolve, RankTwoQuadricUsesAlongEdgeOptimum) {
    const manumesh::Mat4 q = manumesh::simplification::planeQuadric(Vec3(0.0, 0.0, 1.0), Vec3::Zero()) +
                             manumesh::simplification::planeQuadric(Vec3(0.0, 1.0, 0.0), Vec3::Zero());
    const Vec3 a(0.0, 1.0, 0.0);
    const Vec3 b(1.0, -3.0, 0.0);

    const std::vector<SolveResult> placements = manumesh::simplification::solvePlacementCandidates(q, a, b);

    ASSERT_FALSE(placements.empty());
    // The full-rank solve must have been rejected: this is a fallback path.
    EXPECT_TRUE(placements.front().usedFallback);
    // Along the edge y(t) = 1 - 4t, the optimum sits at t = 0.25 with cost 0,
    // strictly better than both endpoints (1 and 9) and the midpoint (1).
    const double costA = evaluateCost(q, a);
    const double costB = evaluateCost(q, b);
    const double costMid = evaluateCost(q, 0.5 * (a + b));
    EXPECT_LT(placements.front().cost, costA);
    EXPECT_LT(placements.front().cost, costB);
    EXPECT_LT(placements.front().cost, costMid);
    EXPECT_NEAR(placements.front().position.x(), 0.25, 1e-12);
    EXPECT_NEAR(placements.front().position.y(), 0.0, 1e-12);
    EXPECT_NEAR(placements.front().position.z(), 0.0, 1e-12);
    EXPECT_NEAR(placements.front().cost, 0.0, 1e-18);
}

// The 1D fallback must be scale invariant together with the spectral checks.
TEST(PlacementSolve, RankTwoAlongEdgeOptimumIsScaleInvariant) {
    for (const double scale : {1e-3, 1.0, 1e+3}) {
        const manumesh::Mat4 q = manumesh::simplification::planeQuadric(Vec3(0.0, 0.0, 1.0), Vec3::Zero()) +
                                 manumesh::simplification::planeQuadric(Vec3(0.0, 1.0, 0.0), Vec3::Zero());
        const Vec3 a = scale * Vec3(0.0, 1.0, 0.0);
        const Vec3 b = scale * Vec3(1.0, -3.0, 0.0);
        const std::vector<SolveResult> placements = manumesh::simplification::solvePlacementCandidates(q, a, b);
        ASSERT_FALSE(placements.empty());
        EXPECT_NEAR(placements.front().position.x(), 0.25 * scale, 1e-12 * scale) << "scale " << scale;
        EXPECT_NEAR(placements.front().position.y(), 0.0, 1e-12 * scale) << "scale " << scale;
    }
}

// Lindstrom-Turk boundary preservation: on a straight boundary chain the
// constraint line IS the boundary, so the placement lands exactly on it.
TEST(BoundaryPlacement, StraightChainProjectsOntoBoundaryLine) {
    const std::array<Vec3, 4> chain = {
        Vec3(-1.0, 1.0, 0.0),
        Vec3(0.0, 1.0, 0.0),
        Vec3(1.0, 1.0, 0.0),
        Vec3(2.0, 1.0, 0.0),
    };
    BoundaryStrip strip(chain);
    const BoundaryCollapseDecision decision{true, true};

    Vec3 position(0.4, 1.25, 0.1);
    const bool projected = manumesh::simplification::projectBoundaryPlacement(
        {{BoundaryStrip::kKeep, BoundaryStrip::kRemove}, decision, strip.vertices, strip.faces, strip.topology},
        position
    );

    EXPECT_TRUE(projected);
    EXPECT_NEAR(position.x(), 0.4, 1e-12);
    EXPECT_NEAR(position.y(), 1.0, 1e-12);
    EXPECT_NEAR(position.z(), 0.0, 1e-12);
}

// On a cornered boundary chain the LT constraint line balances the swept
// directed area; the projected placement must beat the plain segment clamp
// under the LT boundary objective (i.e. less first-order boundary drift).
TEST(BoundaryPlacement, CorneredChainReducesDirectedAreaChangeVersusClamp) {
    const std::array<Vec3, 4> chain = {
        Vec3(-1.0, 0.3, 0.0),
        Vec3(0.0, 0.0, 0.0),
        Vec3(1.0, 0.0, 0.0),
        Vec3(2.0, 0.3, 0.0),
    };
    BoundaryStrip strip(chain);
    const BoundaryCollapseDecision decision{true, true};

    const Vec3 qemPosition(0.5, -0.05, 0.0);
    Vec3 position = qemPosition;
    const bool projected = manumesh::simplification::projectBoundaryPlacement(
        {{BoundaryStrip::kKeep, BoundaryStrip::kRemove}, decision, strip.vertices, strip.faces, strip.topology},
        position
    );
    ASSERT_TRUE(projected);

    const Vec3 clamped = clampToSegmentReference(qemPosition, chain[1], chain[2]);
    const double ltObjective = boundaryAreaObjective(chain, position);
    const double clampObjective = boundaryAreaObjective(chain, clamped);
    EXPECT_LT(ltObjective, clampObjective);
    // The analytic minimizer line for this chain is y = -0.1, z = 0.
    EXPECT_NEAR(position.y(), -0.1, 1e-12);
    EXPECT_NEAR(position.z(), 0.0, 1e-12);
    // The safety clamp keeps the placement within the collapsing edge shadow.
    EXPECT_GE(position.x(), 0.0);
    EXPECT_LE(position.x(), 1.0);
}

// End-to-end drift guard: preserveBoundary simplification of an open mesh
// keeps every output boundary vertex close to the input boundary polyline.
// The bound is tighter than the historical 2x max-segment-length guarantee of
// the segment-clamp implementation, so a regression back to larger drift
// fails this test.
TEST(BoundaryPlacement, PreserveBoundarySimplifyKeepsBoundaryDriftTight) {
    const Mesh input = manumesh::generateHolePlaneGrid(16, 2.0, 0.35);
    double maxSegmentLength = 0.0;
    const std::vector<std::array<Vec3, 2>> inputBoundary = collectBoundarySegments(input, &maxSegmentLength);
    ASSERT_FALSE(inputBoundary.empty());
    ASSERT_GT(maxSegmentLength, 0.0);

    manumesh::simplification::SimplifyOptions options;
    options.targetRatio = 0.4;
    options.preserveBoundary = true;
    const Mesh output = manumesh::simplification::simplifyMesh(input, options);
    ASSERT_FALSE(output.empty());

    double drift = 0.0;
    const std::vector<std::array<Vec3, 2>> outputBoundary = collectBoundarySegments(output, nullptr);
    ASSERT_FALSE(outputBoundary.empty());
    for (const std::array<Vec3, 2>& segment : outputBoundary) {
        drift = std::max(drift, distanceToSegments(segment[0], inputBoundary));
        drift = std::max(drift, distanceToSegments(segment[1], inputBoundary));
    }
    EXPECT_LE(drift, 1.0 * maxSegmentLength);
}

// Long polygonal feature curves get a prebuilt segment index; its
// closest-point answers must match the plain linear scan.
TEST(FeatureCurves, SegmentIndexMatchesLinearScan) {
    FeatureCurveConstraint linear;
    linear.valid = true;
    linear.closed = true;
    linear.primitive = FeatureCurveKind::PolygonalLoop;
    const int sampleCount = 200;
    for (int i = 0; i < sampleCount; ++i) {
        const double angle = 2.0 * 3.14159265358979323846 * static_cast<double>(i) / sampleCount;
        const double radius = 2.0 + 0.35 * std::sin(7.0 * angle);
        linear.samples.push_back(Vec3(radius * std::cos(angle), radius * std::sin(angle), 0.2 * std::sin(3.0 * angle)));
    }
    FeatureCurveConstraint indexed = linear;
    manumesh::simplification::buildPolylineSegmentIndex(indexed);
    ASSERT_TRUE(indexed.segmentIndex.built());
    ASSERT_FALSE(linear.segmentIndex.built());

    for (int i = 0; i < 64; ++i) {
        const double angle = 0.37 + 0.11 * static_cast<double>(i);
        const Vec3 query(2.6 * std::cos(angle), 2.6 * std::sin(angle), 0.3 * std::sin(angle * 2.0));
        double linearDist2 = 0.0;
        double indexedDist2 = 0.0;
        const Vec3 linearPoint = manumesh::simplification::closestPointOnFeatureCurve(linear, query, linearDist2);
        const Vec3 indexedPoint = manumesh::simplification::closestPointOnFeatureCurve(indexed, query, indexedDist2);
        EXPECT_NEAR(linearDist2, indexedDist2, 1e-15) << "query " << i;
        EXPECT_NEAR((linearPoint - indexedPoint).norm(), 0.0, 1e-9) << "query " << i;
    }
}

// Wang 2008 decoupling: in adaptiveScale mode the feature boost must not
// change the quadrics (placement stays clean); it only produces per-vertex
// queue-priority scales.
TEST(FeatureBoost, AdaptiveScaleKeepsQuadricsCleanAndFillsPriorityScales) {
    const Mesh input = manumesh::generateRidgeGrid(16, 2.0, 0.6);
    manumesh::simplification::SimplifyOptions boosted;
    boosted.useLineQuadrics = true;
    boosted.lineWeight = 1e-3;
    boosted.weightMode = manumesh::simplification::WeightMode::NormalTensor;
    boosted.normalTensorFeatureThreshold = 0.04;
    boosted.adaptiveScale = true;
    boosted.adaptiveBaseLineWeight = 1e-2;
    boosted.featureBoost = 0.5;
    manumesh::simplification::SimplifyOptions unboosted = boosted;
    unboosted.featureBoost = 0.0;

    manumesh::simplification::FeatureGuidance guidance;
    manumesh::simplification::SimplifyReport boostedReport;
    manumesh::simplification::SimplifyReport unboostedReport;
    manumesh::simplification::InitialQuadrics boostedQ;
    manumesh::simplification::InitialQuadrics unboostedQ;
    manumesh::simplification::computeInitialQuadrics(input, boosted, guidance, boostedQ, boostedReport);
    manumesh::simplification::computeInitialQuadrics(input, unboosted, guidance, unboostedQ, unboostedReport);

    ASSERT_EQ(boostedQ.quadrics.size(), unboostedQ.quadrics.size());
    for (std::size_t i = 0; i < boostedQ.quadrics.size(); ++i) {
        EXPECT_TRUE(boostedQ.quadrics[i].isApprox(unboostedQ.quadrics[i], 0.0))
            << "quadric " << i << " was distorted by featureBoost";
    }

    ASSERT_EQ(boostedQ.priorityScales.size(), input.vertices.size());
    double maxScale = 0.0;
    for (double scale : boostedQ.priorityScales) {
        EXPECT_GE(scale, 1.0);
        maxScale = std::max(maxScale, scale);
    }
    // The ridge produces positive normal-tensor scores, so at least one
    // vertex must receive a queue-priority boost.
    EXPECT_GT(maxScale, 1.0);
    // Without boost every priority scale collapses to exactly 1.
    for (double scale : unboostedQ.priorityScales) {
        EXPECT_DOUBLE_EQ(1.0, scale);
    }
    // The quadric line weight reported for adaptive mode is the clean base
    // weight, identical with and without boost.
    EXPECT_DOUBLE_EQ(boostedReport.minAppliedLineWeight, boostedReport.maxAppliedLineWeight);
    EXPECT_DOUBLE_EQ(boostedReport.minAppliedLineWeight, unboostedReport.minAppliedLineWeight);
}
