#include "FeatureDetectionTestSupport.h"

#include "feature_detection/detail/FeatureDetectionCache.h"
#include "feature_detection/detail/FeatureDetectionTypes.h"
#include "feature_detection/detail/FeatureGraph.h"
#include "feature_detection/detail/FeatureGraphCleanup.h"

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

namespace manumesh::test::feature_detection {
namespace {

namespace detector_detail = feature::detector_detail;

using detector_detail::CandidateEdge;
using detector_detail::FeatureDetectionCache;
using detector_detail::TraceGraph;

/// Builds one mesh containing three disjoint trace chains, each edge of unit
/// length and backed by an equilateral support triangle so every chain vertex
/// gets a local average edge length of exactly one:
///  - chain A: 11 faint weak edges (long but weak),
///  - chain B: 5 very faint weak edges (medium-length noise),
///  - chain C: 3 dihedral edges plus a 2-edge strong weak branch.
struct SpurFixture {
    Mesh mesh;
    TraceGraph trace;
    FeatureAnalysis analysis;
    std::vector<int> chainA;
    std::vector<int> chainB;
    std::vector<int> chainC;
    std::vector<int> branchC;
};

int appendChainVertices(Mesh& mesh, int count, double z, std::vector<int>& ids) {
    const int base = static_cast<int>(mesh.vertices.size());
    for (int i = 0; i < count; ++i) {
        mesh.vertices.emplace_back(static_cast<double>(i), 0.0, z);
        ids.push_back(base + i);
    }
    return base;
}

void appendSupportTriangles(Mesh& mesh, const std::vector<int>& chain) {
    const double apexHeight = std::sqrt(3.0) / 2.0;
    for (std::size_t i = 0; i + 1 < chain.size(); ++i) {
        const Vec3 a = mesh.vertices[chain[i]];
        const Vec3 b = mesh.vertices[chain[i + 1]];
        // Perpendicular (in the xy-plane) apex at equilateral height, so every
        // support edge has the same unit length as the chain edge.
        const Vec3 direction = (b - a).normalized();
        const Vec3 perpendicular(-direction.y(), direction.x(), 0.0);
        const Vec3 apexPosition = 0.5 * (a + b) + apexHeight * perpendicular;
        const int apex = static_cast<int>(mesh.vertices.size());
        mesh.vertices.emplace_back(apexPosition.x(), apexPosition.y(), apexPosition.z());
        mesh.faces.push_back({{chain[i], chain[i + 1], apex}});
    }
}

CandidateEdge weakEdge(int a, int b, double persistence) {
    CandidateEdge edge;
    edge.a = a;
    edge.b = b;
    edge.smoothCurvature = true;
    edge.curvaturePersistentScore = persistence;
    edge.curvaturePersistentScales = 2;
    return edge;
}

CandidateEdge dihedralEdge(int a, int b) {
    CandidateEdge edge;
    edge.a = a;
    edge.b = b;
    edge.dihedral = true;
    edge.signedKind = 1;
    return edge;
}

SpurFixture makeSpurFixture() {
    SpurFixture fixture;
    appendChainVertices(fixture.mesh, 12, 0.0, fixture.chainA);
    appendChainVertices(fixture.mesh, 6, 5.0, fixture.chainB);
    appendChainVertices(fixture.mesh, 4, 10.0, fixture.chainC);
    // Branch hangs off the second vertex of chain C so that vertex keeps
    // degree three (a genuine junction) and the branch walk stops there.
    const int branchBase = static_cast<int>(fixture.mesh.vertices.size());
    fixture.mesh.vertices.emplace_back(1.0, -1.0, 10.0);
    fixture.mesh.vertices.emplace_back(1.0, -2.0, 10.0);
    fixture.branchC = {fixture.chainC[1], branchBase, branchBase + 1};

    appendSupportTriangles(fixture.mesh, fixture.chainA);
    appendSupportTriangles(fixture.mesh, fixture.chainB);
    appendSupportTriangles(fixture.mesh, fixture.chainC);
    appendSupportTriangles(fixture.mesh, fixture.branchC);

    fixture.trace.adjacency.resize(fixture.mesh.vertices.size());
    fixture.trace.traceVertex.assign(fixture.mesh.vertices.size(), 0);
    fixture.analysis.vertices.assign(fixture.mesh.vertices.size(), feature::VertexFeature{});
    fixture.analysis.graph.vertices.assign(fixture.mesh.vertices.size(), feature::FeatureGraphVertex{});

    // Chain A: long, faint (persistence 2x the default threshold 0.015).
    for (std::size_t i = 0; i + 1 < fixture.chainA.size(); ++i) {
        detector_detail::addTraceGraphEdge(
            fixture.trace, fixture.analysis, weakEdge(fixture.chainA[i], fixture.chainA[i + 1], 0.03)
        );
    }
    // Chain B: medium-length, extremely faint (0.2x the threshold).
    for (std::size_t i = 0; i + 1 < fixture.chainB.size(); ++i) {
        detector_detail::addTraceGraphEdge(
            fixture.trace, fixture.analysis, weakEdge(fixture.chainB[i], fixture.chainB[i + 1], 0.003)
        );
    }
    // Chain C: strong dihedral backbone with a short but strong weak branch
    // (8x the threshold).
    for (std::size_t i = 0; i + 1 < fixture.chainC.size(); ++i) {
        detector_detail::addTraceGraphEdge(
            fixture.trace, fixture.analysis, dihedralEdge(fixture.chainC[i], fixture.chainC[i + 1])
        );
    }
    for (std::size_t i = 0; i + 1 < fixture.branchC.size(); ++i) {
        detector_detail::addTraceGraphEdge(
            fixture.trace, fixture.analysis, weakEdge(fixture.branchC[i], fixture.branchC[i + 1], 0.12)
        );
    }
    return fixture;
}

bool chainFullyPresent(const TraceGraph& trace, const std::vector<int>& chain) {
    for (std::size_t i = 0; i + 1 < chain.size(); ++i) {
        if (!detector_detail::traceGraphHasEdge(trace, chain[i], chain[i + 1])) {
            return false;
        }
    }
    return true;
}

bool chainFullyAbsent(const TraceGraph& trace, const std::vector<int>& chain) {
    for (std::size_t i = 0; i + 1 < chain.size(); ++i) {
        if (detector_detail::traceGraphHasEdge(trace, chain[i], chain[i + 1])) {
            return false;
        }
    }
    return true;
}

FeatureOptions spurCleanupOptions() {
    FeatureOptions options;
    options.cleanupFeatureGraph = true;
    options.featureGraphGapLengthRatio = 0.0; // isolate spur removal
    options.featureGraphMaxWeakSpurEdges = 2;
    return options;
}

} // namespace

TEST(FeatureDetectionCleanup, LegacySpurRemovalPrunesByEdgeCountOnly) {
    SpurFixture fixture = makeSpurFixture();
    FeatureOptions options = spurCleanupOptions();
    options.featureGraphMinWeakSpurStrength = 0.0; // legacy behavior

    FeatureDetectionCache cache(fixture.mesh);
    detector_detail::cleanupTraceGraph(fixture.mesh, options, cache, fixture.trace, fixture.analysis);

    EXPECT_TRUE(chainFullyPresent(fixture.trace, fixture.chainA));
    // The faint 5-edge chain exceeds the 2-edge cap, so legacy cleanup keeps it.
    EXPECT_TRUE(chainFullyPresent(fixture.trace, fixture.chainB));
    EXPECT_TRUE(chainFullyPresent(fixture.trace, fixture.chainC));
    // The short strong branch is pruned purely because it is short.
    EXPECT_TRUE(chainFullyAbsent(fixture.trace, fixture.branchC));
    EXPECT_EQ(2, fixture.analysis.graphCleanupRemovedSpurs);
}

TEST(FeatureDetectionCleanup, StrengthFilterKeepsLongWeakLinesAndStrongBranches) {
    SpurFixture fixture = makeSpurFixture();
    FeatureOptions options = spurCleanupOptions();
    // Dimensionless curve strength threshold (Yoshizawa T = length x integral):
    // chain A scores ~242, the strong branch ~32, the faint chain B ~5.
    options.featureGraphMinWeakSpurStrength = 10.0;
    feature::validateFeatureOptions(options);

    FeatureDetectionCache cache(fixture.mesh);
    detector_detail::cleanupTraceGraph(fixture.mesh, options, cache, fixture.trace, fixture.analysis);

    // Long-but-weak line survives through its length factor.
    EXPECT_TRUE(chainFullyPresent(fixture.trace, fixture.chainA));
    // Medium-length noise falls below the strength threshold and is removed
    // even though it is longer than the legacy edge cap.
    EXPECT_TRUE(chainFullyAbsent(fixture.trace, fixture.chainB));
    EXPECT_TRUE(chainFullyPresent(fixture.trace, fixture.chainC));
    // Short-but-strong branch is rescued by its integrated strength.
    EXPECT_TRUE(chainFullyPresent(fixture.trace, fixture.branchC));
    EXPECT_EQ(5, fixture.analysis.graphCleanupRemovedSpurs);
}

TEST(FeatureDetectionCleanup, RejectsInvalidWeakSpurStrengthOption) {
    FeatureOptions options = spurCleanupOptions();
    options.featureGraphMinWeakSpurStrength = -1.0;
    EXPECT_THROW(feature::validateFeatureOptions(options), std::invalid_argument);
}

} // namespace manumesh::test::feature_detection
