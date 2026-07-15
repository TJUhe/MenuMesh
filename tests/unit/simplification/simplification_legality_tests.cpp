#include "SimplificationTestSupport.h"
#include "algorithms/feature_detection/FeatureDetector.h"
#include "algorithms/simplification/Metrics.h"
#include "algorithms/simplification/PlainSimplifier.h"
#include "algorithms/simplification/QEMSimplifier.h"
#include "core/MeshGenerators.h"
#include "core/MeshTopology.h"
#include "core/PlainMesh.h"
#include "simplification/detail/CollapseLegality.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

using manumesh::test::countCircularLoops;
using manumesh::test::loadExternalMesh;
using manumesh::test::loadExternalStl;
using manumesh::test::SimplifiedMesh;
using manumesh::test::simplifyWithReport;
using namespace manumesh::test::simplification;

namespace simplification = manumesh::simplification;

TEST(ManuMesh, CollapseIntersectionGuardRejectsOverlapInsideTheNewOneRing) {
    std::vector<simplification::VertexState> vertices(6);
    vertices[0].p = manumesh::Vec3(-1.0, 0.0, 0.0);
    vertices[1].p = manumesh::Vec3(3.0, 0.0, 0.0);
    vertices[2].p = manumesh::Vec3(2.0, 0.0, 0.0);
    vertices[3].p = manumesh::Vec3(0.0, 1.0, 0.0);
    vertices[4].p = manumesh::Vec3(1.0, 0.5, 0.0);
    vertices[5].p = manumesh::Vec3(1.0, -1.0, 0.0);

    const std::vector<simplification::FaceState> faces = {
        {{{0, 2, 3}}, true},
        {{{1, 4, 2}}, true},
        {{{0, 1, 5}}, true},
    };
    const simplification::DynamicTopology topology(faces, static_cast<int>(vertices.size()));
    simplification::CollapseLegalityInput input{
        {0, 1},
        manumesh::Vec3(0.0, 0.0, 0.0),
        {faces, vertices, topology},
        1e-18,
        0.0,
        -1.0,
        0.0,
        true,
        nullptr,
        nullptr,
    };

    EXPECT_EQ(
        simplification::CollapseRejectReason::SelfIntersection, simplification::collapsePlacementRejectReason(input)
    );

    vertices[4].p = manumesh::Vec3(1.0, -0.5, 0.0);
    EXPECT_EQ(simplification::CollapseRejectReason::None, simplification::collapsePlacementRejectReason(input));
}

TEST(ManuMesh, StrictTriangleQualityRejectsPoorCollapsePlacements) {
    const manumesh::Mesh input = manumesh::generatePlaneGrid(4, 1.0, false);

    manumesh::simplification::SimplifyOptions options = standardQemOptions(0.25);
    options.minTriangleQuality = 0.95;
    options.maxNormalDeviationDeg = 180.0;
    const SimplifiedMesh result = simplifyWithReport(input, options);

    EXPECT_FALSE(result.mesh.empty());
    EXPECT_GT(result.report.qualityRejectedCollapses, 0);
    EXPECT_EQ(
        result.report.rejectedCollapses,
        result.report.topologyRejectedCollapses + result.report.normalFlipRejectedCollapses +
            result.report.qualityRejectedCollapses + result.report.boundaryRejectedCollapses +
            result.report.selfIntersectionRejectedCollapses + result.report.curveBudgetRejectedCollapses +
            result.report.errorRejectedCollapses + result.report.featureRejectedCollapses
    );
}

TEST(ManuMesh, TriesEndpointPlacementWhenBestPlacementFailsLegality) {
    const manumesh::Mesh input = makePlacementFallbackMesh();

    manumesh::simplification::SimplifyOptions options = standardQemOptions(0.5);
    options.targetFaces = 1;
    options.minTriangleQuality = 0.35;
    options.maxNormalDeviationDeg = 180.0;
    const SimplifiedMesh result = simplifyWithReport(input, options);

    EXPECT_EQ(manumesh::simplification::SimplifyTerminationReason::ReachedTarget, result.report.terminationReason);
    EXPECT_EQ(1, result.report.finalFaces);
    EXPECT_EQ(0, result.report.rejectedCollapses);
}

TEST(ManuMesh, StrictNormalDeviationRejectsFoldoverRisk) {
    const manumesh::Mesh input = manumesh::generateCubeGrid(3, 1.0);

    manumesh::simplification::SimplifyOptions options = standardQemOptions(0.25);
    options.minTriangleQuality = 0.0;
    options.maxNormalDeviationDeg = 0.0;
    const SimplifiedMesh result = simplifyWithReport(input, options);

    EXPECT_FALSE(result.mesh.empty());
    EXPECT_GT(result.report.collapsedEdges, 0);
    // With zero allowed deviation the guard must hold exactly: fallback placements
    // (endpoint/midpoint) may satisfy it where the optimal point would flip, so the
    // rejection counter can legitimately be zero. The hard guarantee is geometric:
    // every surviving face normal of the axis-aligned cube shell stays axis-aligned.
    for (const manumesh::Face& face : result.mesh.faces) {
        const manumesh::Vec3 normal = manumesh::triangleNormal(
            result.mesh.vertices[face.v[0]], result.mesh.vertices[face.v[1]], result.mesh.vertices[face.v[2]]
        );
        const double maxComponent = std::max({std::abs(normal.x()), std::abs(normal.y()), std::abs(normal.z())});
        ASSERT_GT(maxComponent, 0.0);
        EXPECT_NEAR(maxComponent, normal.norm(), 1e-9);
    }
    EXPECT_EQ(
        result.report.rejectedCollapses,
        result.report.topologyRejectedCollapses + result.report.normalFlipRejectedCollapses +
            result.report.qualityRejectedCollapses + result.report.boundaryRejectedCollapses +
            result.report.selfIntersectionRejectedCollapses + result.report.curveBudgetRejectedCollapses +
            result.report.errorRejectedCollapses + result.report.featureRejectedCollapses
    );
}

TEST(ManuMesh, StrictLocalErrorRejectsLargeVertexDrift) {
    const manumesh::Mesh input = manumesh::generatePlaneGrid(3, 2.0, false);

    manumesh::simplification::SimplifyOptions options = standardQemOptions(0.25);
    options.maxNormalDeviationDeg = 180.0;
    options.maxLocalErrorRatio = 1e-12;
    const SimplifiedMesh result = simplifyWithReport(input, options);

    EXPECT_FALSE(result.mesh.empty());
    EXPECT_GT(result.report.errorRejectedCollapses, 0);
    EXPECT_EQ(
        result.report.rejectedCollapses,
        result.report.topologyRejectedCollapses + result.report.normalFlipRejectedCollapses +
            result.report.qualityRejectedCollapses + result.report.boundaryRejectedCollapses +
            result.report.selfIntersectionRejectedCollapses + result.report.curveBudgetRejectedCollapses +
            result.report.errorRejectedCollapses + result.report.featureRejectedCollapses
    );
}

TEST(ManuMesh, SimplifiesOpenBoundaryEdgesWhenTopologyIsPreserved) {
    const manumesh::Mesh input = manumesh::generatePlaneGrid(8, 2.0, false);
    const manumesh::simplification::MeshStats inputStats = manumesh::simplification::computeMeshStats(input);
    ASSERT_GT(inputStats.boundaryEdges, 0);
    ASSERT_EQ(1, countBoundaryComponents(input));

    manumesh::simplification::SimplifyOptions options = standardQemOptions(0.08);
    options.preserveBoundary = true;
    const SimplifiedMesh result = simplifyWithReport(input, options);
    const manumesh::simplification::MeshStats outputStats = manumesh::simplification::computeMeshStats(result.mesh);

    EXPECT_FALSE(result.mesh.empty());
    EXPECT_LT(result.report.finalFaces, result.report.initialFaces);
    EXPECT_LT(outputStats.boundaryEdges, inputStats.boundaryEdges);
    EXPECT_LT(countBoundaryVertices(result.mesh), countBoundaryVertices(input));
    EXPECT_EQ(1, countBoundaryComponents(result.mesh));
    EXPECT_EQ(0, outputStats.nonManifoldEdges);
    EXPECT_GT(result.report.boundaryRejectedCollapses, 0);
}

TEST(ManuMesh, KeepsSeparateBoundaryLoopsWhenBoundaryEdgesCollapse) {
    const manumesh::Mesh input = manumesh::generateHolePlaneGrid(16, 2.0, 0.35);
    const manumesh::simplification::MeshStats inputStats = manumesh::simplification::computeMeshStats(input);
    ASSERT_GT(inputStats.boundaryEdges, 0);
    ASSERT_GE(countBoundaryComponents(input), 2);

    manumesh::simplification::SimplifyOptions options = standardQemOptions(0.15);
    options.preserveBoundary = true;
    const SimplifiedMesh result = simplifyWithReport(input, options);
    const manumesh::simplification::MeshStats outputStats = manumesh::simplification::computeMeshStats(result.mesh);

    EXPECT_FALSE(result.mesh.empty());
    EXPECT_LT(result.report.finalFaces, result.report.initialFaces);
    EXPECT_EQ(countBoundaryComponents(input), countBoundaryComponents(result.mesh));
    EXPECT_LE(outputStats.boundaryEdges, inputStats.boundaryEdges);
    EXPECT_EQ(0, outputStats.nonManifoldEdges);
    EXPECT_GT(result.report.boundaryRejectedCollapses, 0);
}

TEST(ManuMesh, LocalIntersectionGuardRejectsIntersectingCollapse) {
    const manumesh::Mesh input = makeLocalIntersectionGuardMesh();

    manumesh::simplification::SimplifyOptions options = standardQemOptions(0.25);
    options.targetFaces = 1;
    options.preventLocalIntersections = true;
    options.maxNormalDeviationDeg = 180.0;
    options.minTriangleQuality = 0.0;
    const SimplifiedMesh result = simplifyWithReport(input, options);

    EXPECT_FALSE(result.mesh.empty());
    EXPECT_GT(result.report.selfIntersectionRejectedCollapses, 0);
    EXPECT_EQ(
        result.report.rejectedCollapses,
        result.report.topologyRejectedCollapses + result.report.normalFlipRejectedCollapses +
            result.report.qualityRejectedCollapses + result.report.boundaryRejectedCollapses +
            result.report.selfIntersectionRejectedCollapses + result.report.curveBudgetRejectedCollapses +
            result.report.errorRejectedCollapses + result.report.featureRejectedCollapses
    );
}

TEST(ManuMesh, LocalIntersectionGuardFindsIndexedDistantCandidates) {
    const manumesh::Mesh input = makeSpatialIntersectionGuardMeshWithFarFaces();

    manumesh::simplification::SimplifyOptions options = standardQemOptions(0.98);
    options.targetFaces = static_cast<int>(input.faces.size()) - 1;
    options.preventLocalIntersections = true;
    options.maxNormalDeviationDeg = 180.0;
    options.minTriangleQuality = 0.0;
    const SimplifiedMesh result = simplifyWithReport(input, options);

    EXPECT_FALSE(result.mesh.empty());
    EXPECT_GT(result.report.selfIntersectionRejectedCollapses, 0);
    EXPECT_EQ(
        result.report.rejectedCollapses,
        result.report.topologyRejectedCollapses + result.report.normalFlipRejectedCollapses +
            result.report.qualityRejectedCollapses + result.report.boundaryRejectedCollapses +
            result.report.selfIntersectionRejectedCollapses + result.report.curveBudgetRejectedCollapses +
            result.report.errorRejectedCollapses + result.report.featureRejectedCollapses
    );
}

TEST(ManuMesh, LocalIntersectionGuardUsesFallbackPlacementForCoplanarOverlap) {
    const manumesh::Mesh input = makeCoplanarOverlapGuardMesh();

    manumesh::simplification::SimplifyOptions options = standardQemOptions(0.25);
    options.targetFaces = static_cast<int>(input.faces.size()) - 1;
    options.preventLocalIntersections = true;
    options.maxNormalDeviationDeg = 180.0;
    options.minTriangleQuality = 0.0;
    const SimplifiedMesh result = simplifyWithReport(input, options);

    EXPECT_FALSE(result.mesh.empty());
    EXPECT_EQ(manumesh::simplification::SimplifyTerminationReason::ReachedTarget, result.report.terminationReason);
    EXPECT_EQ(options.targetFaces, result.report.finalFaces);
    EXPECT_EQ(1, result.report.collapsedEdges);
    EXPECT_EQ(0, result.report.selfIntersectionRejectedCollapses);
    const auto containsPosition = [&](const manumesh::Vec3& position) {
        return std::any_of(result.mesh.vertices.begin(), result.mesh.vertices.end(), [&](const manumesh::Vec3& vertex) {
            return (vertex - position).norm() <= 1e-12;
        });
    };
    EXPECT_TRUE(containsPosition(manumesh::Vec3(2.0, 0.0, 0.0)));
    EXPECT_FALSE(containsPosition(manumesh::Vec3(0.0, 0.0, 0.0)));
}

TEST(ManuMesh, LocalIntersectionGuardAllowsCoplanarSeparatedTriangles) {
    const manumesh::Mesh input = makeCoplanarSeparatedGuardMesh();

    manumesh::simplification::SimplifyOptions options = standardQemOptions(0.25);
    options.targetFaces = static_cast<int>(input.faces.size()) - 1;
    options.preventLocalIntersections = true;
    options.maxNormalDeviationDeg = 180.0;
    options.minTriangleQuality = 0.0;
    const SimplifiedMesh result = simplifyWithReport(input, options);

    EXPECT_FALSE(result.mesh.empty());
    EXPECT_EQ(0, result.report.selfIntersectionRejectedCollapses);
}

TEST(ManuMesh, LocalIntersectionGuardAllowsSharedCoplanarEdges) {
    const manumesh::Mesh input = manumesh::generatePlaneGrid(3, 1.0, false);

    manumesh::simplification::SimplifyOptions options = standardQemOptions(0.55);
    options.preventLocalIntersections = true;
    options.maxNormalDeviationDeg = 180.0;
    const SimplifiedMesh result = simplifyWithReport(input, options);

    EXPECT_FALSE(result.mesh.empty());
    EXPECT_LT(result.report.finalFaces, result.report.initialFaces);
    EXPECT_EQ(0, result.report.selfIntersectionRejectedCollapses);
}
