#include "SimplificationTestSupport.h"
#include "algorithms/simplification/Metrics.h"
#include "algorithms/simplification/QEMSimplifier.h"
#include "core/MeshGenerators.h"
#include "core/MeshTopology.h"
#include "simplification/detail/CollapseTopology.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <gtest/gtest.h>
#include <limits>
#include <vector>

using manumesh::test::SimplifiedMesh;
using manumesh::test::simplifyWithReport;
using namespace manumesh::test::simplification;

namespace {

/// Returns the highest number of boundary edges incident to any single vertex.
/// A vertex-manifold open surface never exceeds two; a boundary pinch
/// (bowtie) vertex has four or more.
int maxBoundaryEdgesAtAnyVertex(const manumesh::Mesh& mesh) {
    const manumesh::Result<manumesh::MeshTopology> topologyResult = manumesh::MeshTopology::build(mesh);
    if (!topologyResult.ok()) {
        return -1;
    }
    std::vector<int> counts(mesh.vertices.size(), 0);
    for (const manumesh::TopologyEdge& edge : topologyResult.value().edges()) {
        if (!edge.boundary()) {
            continue;
        }
        ++counts[edge.vertices[0]];
        ++counts[edge.vertices[1]];
    }
    return counts.empty() ? 0 : *std::max_element(counts.begin(), counts.end());
}

struct BoundaryPolyline {
    std::vector<std::array<manumesh::Vec3, 2>> segments;
    double maxSegmentLength = 0.0;
};

BoundaryPolyline collectBoundaryPolyline(const manumesh::Mesh& mesh) {
    BoundaryPolyline polyline;
    const manumesh::Result<manumesh::MeshTopology> topologyResult = manumesh::MeshTopology::build(mesh);
    if (!topologyResult.ok()) {
        return polyline;
    }
    for (const manumesh::TopologyEdge& edge : topologyResult.value().edges()) {
        if (!edge.boundary()) {
            continue;
        }
        const manumesh::Vec3& a = mesh.vertices[edge.vertices[0]];
        const manumesh::Vec3& b = mesh.vertices[edge.vertices[1]];
        polyline.segments.push_back({a, b});
        polyline.maxSegmentLength = std::max(polyline.maxSegmentLength, (b - a).norm());
    }
    return polyline;
}

double distanceToPolyline(const manumesh::Vec3& point, const BoundaryPolyline& polyline) {
    double best2 = std::numeric_limits<double>::infinity();
    for (const std::array<manumesh::Vec3, 2>& segment : polyline.segments) {
        const manumesh::Vec3 edge = segment[1] - segment[0];
        const double len2 = edge.squaredNorm();
        manumesh::Vec3 closest = segment[0];
        if (len2 > 1e-30) {
            const double t = std::clamp((point - segment[0]).dot(edge) / len2, 0.0, 1.0);
            closest = segment[0] + t * edge;
        }
        best2 = std::min(best2, (point - closest).squaredNorm());
    }
    return std::sqrt(best2);
}

double maxBoundaryVertexDriftFromPolyline(const manumesh::Mesh& output, const BoundaryPolyline& inputBoundary) {
    const manumesh::Result<manumesh::MeshTopology> topologyResult = manumesh::MeshTopology::build(output);
    if (!topologyResult.ok()) {
        return std::numeric_limits<double>::infinity();
    }
    std::vector<char> isBoundary(output.vertices.size(), 0);
    for (const manumesh::TopologyEdge& edge : topologyResult.value().edges()) {
        if (!edge.boundary()) {
            continue;
        }
        isBoundary[edge.vertices[0]] = 1;
        isBoundary[edge.vertices[1]] = 1;
    }
    double maxDrift = 0.0;
    for (int vertex = 0; vertex < static_cast<int>(output.vertices.size()); ++vertex) {
        if (isBoundary[vertex]) {
            maxDrift = std::max(maxDrift, distanceToPolyline(output.vertices[vertex], inputBoundary));
        }
    }
    return maxDrift;
}

void expectVertexManifoldOpenSurface(const manumesh::Mesh& mesh) {
    const manumesh::simplification::MeshStats stats = manumesh::simplification::computeMeshStats(mesh);
    EXPECT_EQ(0, stats.nonManifoldEdges);
    const int maxBoundaryEdges = maxBoundaryEdgesAtAnyVertex(mesh);
    ASSERT_GE(maxBoundaryEdges, 0);
    EXPECT_LE(maxBoundaryEdges, 2);
}

/// Non-convex planar disk around interior vertex 1: the one-ring polygon
/// (0, 2, 3, 4) is star-shaped with respect to vertex 1, but placing the
/// merged vertex at vertex 0 folds face {1, 3, 4} over its opposite edge.
manumesh::Mesh makeNormalFlipFallbackMesh() {
    manumesh::Mesh mesh;
    mesh.vertices = {
        manumesh::Vec3(1.0, 0.0, 0.0),
        manumesh::Vec3(0.0, 0.3, 0.0),
        manumesh::Vec3(0.0, 1.0, 0.0),
        manumesh::Vec3(-1.0, 0.0, 0.0),
        manumesh::Vec3(0.05, 0.02, 0.0),
    };
    mesh.faces = {
        manumesh::Face{{1, 0, 2}},
        manumesh::Face{{1, 2, 3}},
        manumesh::Face{{1, 3, 4}},
        manumesh::Face{{1, 4, 0}},
    };
    return mesh;
}

manumesh::Mesh makeTetrahedronMesh() {
    manumesh::Mesh mesh;
    mesh.vertices = {
        manumesh::Vec3(1.0, 1.0, 1.0),
        manumesh::Vec3(-1.0, -1.0, 1.0),
        manumesh::Vec3(-1.0, 1.0, -1.0),
        manumesh::Vec3(1.0, -1.0, -1.0),
    };
    mesh.faces = {
        manumesh::Face{{0, 2, 1}},
        manumesh::Face{{0, 1, 3}},
        manumesh::Face{{0, 3, 2}},
        manumesh::Face{{1, 2, 3}},
    };
    return mesh;
}

manumesh::Mesh makeOctahedronMesh() {
    manumesh::Mesh mesh;
    mesh.vertices = {
        manumesh::Vec3(0.0, 0.0, 1.0),
        manumesh::Vec3(0.0, 0.0, -1.0),
        manumesh::Vec3(1.0, 0.0, 0.0),
        manumesh::Vec3(0.0, 1.0, 0.0),
        manumesh::Vec3(-1.0, 0.0, 0.0),
        manumesh::Vec3(0.0, -1.0, 0.0),
    };
    mesh.faces = {
        manumesh::Face{{0, 2, 3}},
        manumesh::Face{{0, 3, 4}},
        manumesh::Face{{0, 4, 5}},
        manumesh::Face{{0, 5, 2}},
        manumesh::Face{{1, 3, 2}},
        manumesh::Face{{1, 4, 3}},
        manumesh::Face{{1, 5, 4}},
        manumesh::Face{{1, 2, 5}},
    };
    return mesh;
}

manumesh::Mesh makeTwoDisjointTrianglesMesh() {
    manumesh::Mesh mesh;
    mesh.vertices = {
        manumesh::Vec3(0.0, 0.0, 0.0),
        manumesh::Vec3(1.0, 0.0, 0.0),
        manumesh::Vec3(0.0, 1.0, 0.0),
        manumesh::Vec3(3.0, 0.0, 0.0),
        manumesh::Vec3(4.0, 0.0, 0.0),
        manumesh::Vec3(3.0, 1.0, 0.0),
    };
    mesh.faces = {
        manumesh::Face{{0, 1, 2}},
        manumesh::Face{{3, 4, 5}},
    };
    return mesh;
}

} // namespace

TEST(ManuMesh, BoundaryWeightPullsOpenBoundaryPlacementsBackToPolyline) {
    // Soft boundary preservation (SimplifyOptions::boundaryWeight): the plane
    // grid is an open planar disk, so face-plane quadrics are degenerate in
    // the tangent plane and the default line quadrics pull merged placements
    // tangentially toward one-ring centroids -- with boundaryWeight = 0 the
    // outer boundary vertices drift into the interior. addBoundaryQuadrics
    // adds a plane quadric perpendicular to the surface through each boundary
    // edge, so a large weight makes the merged quadric optimum stay on the
    // original boundary polyline.
    const manumesh::Mesh input = manumesh::generatePlaneGrid(16, 2.0, false);
    const BoundaryPolyline inputBoundary = collectBoundaryPolyline(input);
    ASSERT_FALSE(inputBoundary.segments.empty());
    // Grid spacing of generatePlaneGrid(16, 2.0): h = 2.0 / 16 = 0.125.
    const double gridSpacing = 2.0 / 16.0;

    const auto driftForWeight = [&](double boundaryWeight) {
        manumesh::simplification::SimplifyOptions options;
        options.targetRatio = 0.2;
        options.boundaryWeight = boundaryWeight;
        const SimplifiedMesh result = simplifyWithReport(input, options);
        EXPECT_EQ(manumesh::simplification::SimplifyTerminationReason::ReachedTarget, result.report.terminationReason);
        EXPECT_LT(result.report.finalFaces, result.report.initialFaces);
        // boundaryWeight is a soft ranking/placement cost only: the hard
        // legality filters (extended link condition, fold-over guard) still
        // apply, so the output must stay a vertex-manifold open surface at
        // any weight.
        expectVertexManifoldOpenSurface(result.mesh);
        return maxBoundaryVertexDriftFromPolyline(result.mesh, inputBoundary);
    };

    const double driftUnweighted = driftForWeight(0.0);
    const double driftWeighted = driftForWeight(1e3);

    // Measured on this deterministic fixture: driftUnweighted = 0.1667
    // (4/3 h, i.e. beyond one removed boundary ring) and driftWeighted = 0.0
    // exactly. h/2 and h/20 keep a 10x separation between the two bounds
    // while leaving 2.7x (unweighted) and unbounded (weighted) margins to the
    // measured values, so the test detects the boundaryWeight channel going
    // dead without locking in exact placement coordinates.
    EXPECT_GE(driftUnweighted, 0.5 * gridSpacing);
    EXPECT_LE(driftWeighted, 0.05 * gridSpacing);
    EXPECT_LT(driftWeighted, driftUnweighted);
}

TEST(ManuMesh, BoundaryChordCollapseFailsExtendedLinkCondition) {
    // Two triangles sharing the interior edge (1, 2); every vertex lies on the
    // open boundary, so collapsing the chord would pinch the boundary.
    const manumesh::Mesh input = makePlacementFallbackMesh();
    std::vector<manumesh::simplification::FaceState> faces(input.faces.size());
    for (int face = 0; face < static_cast<int>(input.faces.size()); ++face) {
        faces[face].v = input.faces[face].v;
    }
    std::vector<manumesh::simplification::VertexState> vertices(input.vertices.size());
    for (int vertex = 0; vertex < static_cast<int>(input.vertices.size()); ++vertex) {
        vertices[vertex].p = input.vertices[vertex];
        vertices[vertex].isBoundary = true;
    }
    const manumesh::simplification::DynamicTopology topology(faces, static_cast<int>(vertices.size()));

    EXPECT_FALSE(manumesh::simplification::collapseWouldPreserveLinkCondition(1, 2, faces, vertices, topology));
    EXPECT_FALSE(manumesh::simplification::collapseWouldPreserveLinkCondition(2, 1, faces, vertices, topology));
    EXPECT_TRUE(manumesh::simplification::collapseWouldPreserveLinkCondition(0, 1, faces, vertices, topology));

    // Without boundary flags the same chord satisfies the classic link
    // condition, which shows the rejection comes from the boundary extension.
    for (manumesh::simplification::VertexState& vertex : vertices) {
        vertex.isBoundary = false;
    }
    EXPECT_TRUE(manumesh::simplification::collapseWouldPreserveLinkCondition(1, 2, faces, vertices, topology));
}

TEST(ManuMesh, TetrahedronEdgeCollapseFailsFullSimplicialLinkCondition) {
    const manumesh::Mesh input = makeTetrahedronMesh();
    std::vector<manumesh::simplification::FaceState> faces(input.faces.size());
    for (int face = 0; face < static_cast<int>(input.faces.size()); ++face) {
        faces[face].v = input.faces[face].v;
    }
    std::vector<manumesh::simplification::VertexState> vertices(input.vertices.size());
    for (int vertex = 0; vertex < static_cast<int>(input.vertices.size()); ++vertex) {
        vertices[vertex].p = input.vertices[vertex];
    }
    const manumesh::simplification::DynamicTopology topology(faces, static_cast<int>(vertices.size()));

    // The endpoint links share the edge opposite each tetrahedron edge. The
    // edge link itself contains only the two opposite vertices, so the full
    // simplicial-complex equality fails even though the link vertex sets match.
    for (int keep = 0; keep < 4; ++keep) {
        for (int remove = keep + 1; remove < 4; ++remove) {
            EXPECT_FALSE(
                manumesh::simplification::collapseWouldPreserveLinkCondition(keep, remove, faces, vertices, topology)
            ) << "edge ("
              << keep << ", " << remove << ")";
            EXPECT_FALSE(
                manumesh::simplification::collapseWouldPreserveLinkCondition(remove, keep, faces, vertices, topology)
            ) << "edge ("
              << remove << ", " << keep << ")";
        }
    }
}

TEST(ManuMesh, SimplifyDoesNotCollapseTetrahedronThroughDuplicateFaceRemoval) {
    const manumesh::Mesh input = makeTetrahedronMesh();
    manumesh::simplification::SimplifyOptions options;
    options.targetFaces = 1;

    const SimplifiedMesh result = simplifyWithReport(input, options);

    EXPECT_TRUE(
        result.report.terminationReason == manumesh::simplification::SimplifyTerminationReason::RejectionLimit ||
        result.report.terminationReason == manumesh::simplification::SimplifyTerminationReason::NoCandidates
    );
    EXPECT_EQ(4, result.report.finalVertices);
    EXPECT_EQ(4, result.report.finalFaces);
    EXPECT_EQ(0, result.report.collapsedEdges);
    EXPECT_GT(result.report.topologyRejectedCollapses, 0);
}

TEST(ManuMesh, OctahedronEdgesPassFullSimplicialLinkCondition) {
    const manumesh::Mesh input = makeOctahedronMesh();
    std::vector<manumesh::simplification::FaceState> faces(input.faces.size());
    for (int face = 0; face < static_cast<int>(input.faces.size()); ++face) {
        faces[face].v = input.faces[face].v;
    }
    std::vector<manumesh::simplification::VertexState> vertices(input.vertices.size());
    for (int vertex = 0; vertex < static_cast<int>(input.vertices.size()); ++vertex) {
        vertices[vertex].p = input.vertices[vertex];
    }
    const manumesh::simplification::DynamicTopology topology(faces, static_cast<int>(vertices.size()));
    const std::array<std::array<int, 2>, 12> edges = {
        {{0, 2}, {0, 3}, {0, 4}, {0, 5}, {1, 2}, {1, 3}, {1, 4}, {1, 5}, {2, 3}, {3, 4}, {4, 5}, {5, 2}}
    };

    for (const std::array<int, 2>& edge : edges) {
        EXPECT_TRUE(
            manumesh::simplification::collapseWouldPreserveLinkCondition(edge[0], edge[1], faces, vertices, topology)
        ) << "edge ("
          << edge[0] << ", " << edge[1] << ")";
        EXPECT_TRUE(
            manumesh::simplification::collapseWouldPreserveLinkCondition(edge[1], edge[0], faces, vertices, topology)
        ) << "edge ("
          << edge[1] << ", " << edge[0] << ")";
    }
}

TEST(ManuMesh, ClosedSurfaceSimplifiesToTetrahedronAndThenStops) {
    const manumesh::Mesh input = makeOctahedronMesh();
    manumesh::simplification::SimplifyOptions options;
    options.targetFaces = 1;

    const SimplifiedMesh result = simplifyWithReport(input, options);

    EXPECT_TRUE(
        result.report.terminationReason == manumesh::simplification::SimplifyTerminationReason::RejectionLimit ||
        result.report.terminationReason == manumesh::simplification::SimplifyTerminationReason::NoCandidates
    );
    EXPECT_EQ(4, result.report.finalVertices);
    EXPECT_EQ(4, result.report.finalFaces);
    EXPECT_EQ(2, result.report.collapsedEdges);
    EXPECT_GT(result.report.topologyRejectedCollapses, 0);
}

TEST(ManuMesh, IsolatedOpenTriangleEdgesFailBoundaryExtendedLinkCondition) {
    const manumesh::Mesh input = makeTwoDisjointTrianglesMesh();
    std::vector<manumesh::simplification::FaceState> faces(input.faces.size());
    for (int face = 0; face < static_cast<int>(input.faces.size()); ++face) {
        faces[face].v = input.faces[face].v;
    }
    std::vector<manumesh::simplification::VertexState> vertices(input.vertices.size());
    for (int vertex = 0; vertex < static_cast<int>(input.vertices.size()); ++vertex) {
        vertices[vertex].p = input.vertices[vertex];
        vertices[vertex].isBoundary = true;
    }
    const manumesh::simplification::DynamicTopology topology(faces, static_cast<int>(vertices.size()));

    for (int keep = 0; keep < 3; ++keep) {
        const int remove = (keep + 1) % 3;
        EXPECT_FALSE(
            manumesh::simplification::collapseWouldPreserveLinkCondition(keep, remove, faces, vertices, topology)
        ) << "edge ("
          << keep << ", " << remove << ")";
        EXPECT_FALSE(
            manumesh::simplification::collapseWouldPreserveLinkCondition(remove, keep, faces, vertices, topology)
        ) << "edge ("
          << remove << ", " << keep << ")";
    }
}

TEST(ManuMesh, SimplifyDoesNotEraseIsolatedTriangleComponent) {
    const manumesh::Mesh input = makeTwoDisjointTrianglesMesh();
    manumesh::simplification::SimplifyOptions options;
    options.targetFaces = 1;

    const SimplifiedMesh result = simplifyWithReport(input, options);

    EXPECT_TRUE(
        result.report.terminationReason == manumesh::simplification::SimplifyTerminationReason::RejectionLimit ||
        result.report.terminationReason == manumesh::simplification::SimplifyTerminationReason::NoCandidates
    );
    EXPECT_EQ(6, result.report.finalVertices);
    EXPECT_EQ(2, result.report.finalFaces);
    EXPECT_EQ(0, result.report.collapsedEdges);
    EXPECT_GT(result.report.topologyRejectedCollapses, 0);
}

TEST(ManuMesh, NonManifoldEdgeFailsLinkConditionInBothDirections) {
    std::vector<manumesh::simplification::FaceState> faces(3);
    faces[0].v = {0, 1, 2};
    faces[1].v = {1, 0, 3};
    faces[2].v = {0, 1, 4};
    std::vector<manumesh::simplification::VertexState> vertices(5);
    const manumesh::simplification::DynamicTopology topology(faces, static_cast<int>(vertices.size()));

    EXPECT_FALSE(manumesh::simplification::collapseWouldPreserveLinkCondition(0, 1, faces, vertices, topology));
    EXPECT_FALSE(manumesh::simplification::collapseWouldPreserveLinkCondition(1, 0, faces, vertices, topology));
}

TEST(ManuMesh, DefaultSimplifyKeepsOpenBoundaryVertexManifold) {
    const std::vector<manumesh::Mesh> inputs = {
        manumesh::generatePlaneGrid(8, 2.0, false),
        manumesh::generateHolePlaneGrid(16, 2.0, 0.35),
    };
    const std::vector<double> ratios = {0.25, 0.12};

    for (const manumesh::Mesh& input : inputs) {
        ASSERT_GT(manumesh::simplification::computeMeshStats(input).boundaryEdges, 0);
        for (double ratio : ratios) {
            SCOPED_TRACE(testing::Message() << "faces=" << input.faces.size() << " ratio=" << ratio);
            manumesh::simplification::SimplifyOptions options;
            options.targetRatio = ratio;

            const SimplifiedMesh result = simplifyWithReport(input, options);

            EXPECT_FALSE(result.mesh.empty());
            EXPECT_LT(result.report.finalFaces, result.report.initialFaces);
            expectVertexManifoldOpenSurface(result.mesh);
        }
    }
}

TEST(ManuMesh, PreserveBoundaryKeepsHoleBoundaryNearOriginalPolyline) {
    const manumesh::Mesh input = manumesh::generateHolePlaneGrid(16, 2.0, 0.35);
    ASSERT_GE(countBoundaryComponents(input), 2);
    const BoundaryPolyline inputBoundary = collectBoundaryPolyline(input);
    ASSERT_FALSE(inputBoundary.segments.empty());
    ASSERT_GT(inputBoundary.maxSegmentLength, 0.0);

    manumesh::simplification::SimplifyOptions options;
    options.targetRatio = 0.5;
    options.preserveBoundary = true;
    options.preserveFeatureCurves = true;
    const SimplifiedMesh result = simplifyWithReport(input, options);

    EXPECT_FALSE(result.mesh.empty());
    EXPECT_LT(result.report.finalFaces, result.report.initialFaces);
    EXPECT_EQ(countBoundaryComponents(input), countBoundaryComponents(result.mesh));
    expectVertexManifoldOpenSurface(result.mesh);

    // Boundary placements are clamped onto boundary segments and outrank
    // feature projection, so hole and outer boundary vertices stay within a
    // small tolerance of the original boundary polyline.
    const double drift = maxBoundaryVertexDriftFromPolyline(result.mesh, inputBoundary);
    EXPECT_LE(drift, 2.0 * inputBoundary.maxSegmentLength);
}

TEST(ManuMesh, RepeatedSimplifyRunsProduceIdenticalOutput) {
    const manumesh::Mesh input = manumesh::generateHolePlaneGrid(12, 2.0, 0.35);
    manumesh::simplification::SimplifyOptions options;
    options.targetRatio = 0.3;
    options.preserveBoundary = true;
    options.preserveFeatureCurves = true;

    manumesh::simplification::SimplifyReport firstReport;
    manumesh::simplification::SimplifyReport secondReport;
    const manumesh::Mesh first = manumesh::simplification::simplifyMesh(input, options, &firstReport);
    const manumesh::Mesh second = manumesh::simplification::simplifyMesh(input, options, &secondReport);

    EXPECT_EQ(firstReport.collapsedEdges, secondReport.collapsedEdges);
    EXPECT_EQ(firstReport.rejectedCollapses, secondReport.rejectedCollapses);
    EXPECT_EQ(firstReport.finalFaces, secondReport.finalFaces);
    ASSERT_EQ(first.vertices.size(), second.vertices.size());
    ASSERT_EQ(first.faces.size(), second.faces.size());
    for (std::size_t vertex = 0; vertex < first.vertices.size(); ++vertex) {
        EXPECT_EQ(0, std::memcmp(first.vertices[vertex].data(), second.vertices[vertex].data(), 3 * sizeof(double)))
            << "vertex " << vertex;
    }
    for (std::size_t face = 0; face < first.faces.size(); ++face) {
        EXPECT_EQ(first.faces[face].v, second.faces[face].v) << "face " << face;
    }
}

TEST(ManuMesh, NormalFlipRejectionFallsBackToEndpointPlacement) {
    const manumesh::Mesh input = makeNormalFlipFallbackMesh();

    manumesh::simplification::SimplifyOptions options;
    options.targetFaces = 2;
    options.useLineQuadrics = false;
    options.lineWeight = 0.0;
    // Default maxNormalDeviationDeg (90) keeps the fold-over filter active:
    // the first placement candidate (endpoint 0) flips face {1, 3, 4}, but the
    // fallback endpoint placement is legal and must be tried.
    const SimplifiedMesh result = simplifyWithReport(input, options);

    EXPECT_EQ(manumesh::simplification::SimplifyTerminationReason::ReachedTarget, result.report.terminationReason);
    EXPECT_EQ(2, result.report.finalFaces);
    EXPECT_EQ(1, result.report.collapsedEdges);
    EXPECT_EQ(0, result.report.rejectedCollapses);
    EXPECT_EQ(0, result.report.normalFlipRejectedCollapses);
}
