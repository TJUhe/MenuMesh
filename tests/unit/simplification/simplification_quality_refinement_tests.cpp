#include "TestSupport.h"

#include "algorithms/feature_detection/FeatureDetector.h"
#include "algorithms/simplification/Metrics.h"
#include "core/MeshGenerators.h"
#include "core/MeshTopology.h"

#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <limits>

namespace {

manumesh::Mesh makePoorQualityPlaneGrid() {
    manumesh::Mesh mesh;
    mesh.vertices = {
        {0.0, 0.0, 0.0},
        {0.5, 0.0, 0.0},
        {1.0, 0.0, 0.0},
        {0.0, 0.5, 0.0},
        {0.12, 0.12, 0.0},
        {1.0, 0.5, 0.0},
        {0.0, 1.0, 0.0},
        {0.5, 1.0, 0.0},
        {1.0, 1.0, 0.0},
    };
    mesh.faces = {
        {{0, 1, 4}},
        {{0, 4, 3}},
        {{1, 2, 5}},
        {{1, 5, 4}},
        {{3, 4, 7}},
        {{3, 7, 6}},
        {{4, 5, 8}},
        {{4, 8, 7}},
    };
    return mesh;
}

void expectSameMesh(const manumesh::Mesh& lhs, const manumesh::Mesh& rhs) {
    ASSERT_EQ(lhs.vertices.size(), rhs.vertices.size());
    ASSERT_EQ(lhs.faces.size(), rhs.faces.size());
    for (std::size_t i = 0; i < lhs.vertices.size(); ++i) {
        EXPECT_EQ(lhs.vertices[i].x(), rhs.vertices[i].x());
        EXPECT_EQ(lhs.vertices[i].y(), rhs.vertices[i].y());
        EXPECT_EQ(lhs.vertices[i].z(), rhs.vertices[i].z());
    }
    for (std::size_t i = 0; i < lhs.faces.size(); ++i) {
        EXPECT_EQ(lhs.faces[i].v, rhs.faces[i].v);
    }
}

} // namespace

TEST(QualityRefinement, ImprovesWorstTriangleWithoutChangingTopologyOrEnvelope) {
    const manumesh::Mesh input = makePoorQualityPlaneGrid();
    const manumesh::simplification::MeshStats before = manumesh::simplification::computeMeshStats(input);
    const manumesh::Result<manumesh::MeshTopology> beforeTopology = manumesh::MeshTopology::build(input);
    ASSERT_TRUE(beforeTopology.ok());

    manumesh::simplification::SimplifyOptions options;
    options.targetRatio = 1.0;
    options.preserveBoundary = true;
    options.maxNormalDeviationDeg = 5.0;
    options.maxLocalError = 1e-8;
    options.preventLocalIntersections = true;
    options.qualityRefinementIterations = 3;

    const manumesh::test::SimplifiedMesh refined = manumesh::test::simplifyWithReport(input, options);
    const manumesh::simplification::MeshStats after = manumesh::simplification::computeMeshStats(refined.mesh);
    const manumesh::simplification::DistanceStats distance =
        manumesh::simplification::compareMeshesBySampledDistance(input, refined.mesh, 1000);
    const manumesh::Result<manumesh::MeshTopology> afterTopology = manumesh::MeshTopology::build(refined.mesh);
    ASSERT_TRUE(afterTopology.ok());

    EXPECT_EQ(input.faces.size(), refined.mesh.faces.size());
    EXPECT_EQ(beforeTopology.value().edgeCount(), afterTopology.value().edgeCount());
    EXPECT_EQ(beforeTopology.value().boundaryEdgeCount(), afterTopology.value().boundaryEdgeCount());
    EXPECT_GT(after.minTriangleQuality, before.minTriangleQuality + 0.10);
    EXPECT_GE(after.meanTriangleQuality, before.meanTriangleQuality);
    EXPECT_LE(distance.maxOriginalToSimplified, options.maxLocalError + 1e-10);
    EXPECT_LE(distance.maxSimplifiedToOriginal, options.maxLocalError + 1e-10);
    EXPECT_GT(refined.report.qualityRefinementAttemptedMoves, 0);
    EXPECT_GT(refined.report.qualityRefinementAcceptedMoves, 0);
    EXPECT_LE(refined.report.qualityRefinementAcceptedMoves, refined.report.qualityRefinementAttemptedMoves);
    EXPECT_GE(refined.report.qualityRefinementIterationsCompleted, 1);
    EXPECT_LE(refined.report.qualityRefinementIterationsCompleted, options.qualityRefinementIterations);
}

TEST(QualityRefinement, ZeroIterationsPreservesOneRoundOutputExactly) {
    const manumesh::Mesh input = makePoorQualityPlaneGrid();

    manumesh::simplification::SimplifyOptions baselineOptions;
    baselineOptions.targetRatio = 1.0;
    baselineOptions.preserveBoundary = true;
    const manumesh::test::SimplifiedMesh baseline = manumesh::test::simplifyWithReport(input, baselineOptions);

    manumesh::simplification::SimplifyOptions explicitZero = baselineOptions;
    explicitZero.qualityRefinementIterations = 0;
    const manumesh::test::SimplifiedMesh result = manumesh::test::simplifyWithReport(input, explicitZero);

    expectSameMesh(baseline.mesh, result.mesh);
    EXPECT_EQ(0, result.report.qualityRefinementIterationsCompleted);
    EXPECT_EQ(0, result.report.qualityRefinementAttemptedMoves);
    EXPECT_EQ(0, result.report.qualityRefinementAcceptedMoves);
}

TEST(QualityRefinement, ImprovesQualityAfterActualEdgeCollapse) {
    const manumesh::Mesh input = manumesh::generatePlaneGrid(20, 2.0, true);

    manumesh::simplification::SimplifyOptions baselineOptions;
    baselineOptions.targetRatio = 0.20;
    baselineOptions.preserveBoundary = true;
    baselineOptions.maxNormalDeviationDeg = 5.0;
    baselineOptions.maxLocalError = 0.02;
    baselineOptions.preventLocalIntersections = true;
    const manumesh::test::SimplifiedMesh baseline = manumesh::test::simplifyWithReport(input, baselineOptions);

    manumesh::simplification::SimplifyOptions refinedOptions = baselineOptions;
    refinedOptions.qualityRefinementIterations = 3;
    const manumesh::test::SimplifiedMesh refined = manumesh::test::simplifyWithReport(input, refinedOptions);

    const manumesh::simplification::MeshStats baselineStats = manumesh::simplification::computeMeshStats(baseline.mesh);
    const manumesh::simplification::MeshStats refinedStats = manumesh::simplification::computeMeshStats(refined.mesh);
    const manumesh::simplification::DistanceStats refinedDistance =
        manumesh::simplification::compareMeshesBySampledDistance(input, refined.mesh, 1500);
    const manumesh::Result<manumesh::MeshTopology> baselineTopology = manumesh::MeshTopology::build(baseline.mesh);
    const manumesh::Result<manumesh::MeshTopology> refinedTopology = manumesh::MeshTopology::build(refined.mesh);
    ASSERT_TRUE(baselineTopology.ok());
    ASSERT_TRUE(refinedTopology.ok());

    EXPECT_GT(baseline.report.collapsedEdges, 0);
    EXPECT_EQ(baseline.mesh.faces.size(), refined.mesh.faces.size());
    EXPECT_EQ(baselineTopology.value().boundaryEdgeCount(), refinedTopology.value().boundaryEdgeCount());
    // The worst surviving triangle can be pinned by frozen boundary vertices, so the
    // minimum must never regress while the mean must strictly improve.
    EXPECT_GE(refinedStats.minTriangleQuality, baselineStats.minTriangleQuality);
    EXPECT_GT(refinedStats.meanTriangleQuality, baselineStats.meanTriangleQuality);
    EXPECT_GT(refined.report.qualityRefinementAcceptedMoves, 0);
    EXPECT_LE(refinedDistance.maxOriginalToSimplified, refinedOptions.maxLocalError + 1e-10);
    EXPECT_LE(refinedDistance.maxSimplifiedToOriginal, refinedOptions.maxLocalError + 1e-10);
}

TEST(QualityRefinement, HardProtectedCircularFeatureLoopsRemainStable) {
    const manumesh::Mesh input = manumesh::generateCylinderGrid(32, 5, 1.0, 2.0);

    manumesh::feature::FeatureOptions featureOptions;
    featureOptions.featureAngleDeg = 25.0;
    featureOptions.minFeatureLoopVertices = 8;
    featureOptions.useNormalTensorFeatures = false;
    const manumesh::feature::FeatureAnalysis before = manumesh::feature::detectFeatureCurves(input, featureOptions);

    manumesh::simplification::SimplifyOptions options;
    options.targetRatio = 1.0;
    options.preserveFeatureCurves = true;
    options.featureProtectionMode = manumesh::simplification::FeatureProtectionMode::PrimitiveCurves;
    options.featureAngleDeg = featureOptions.featureAngleDeg;
    options.minFeatureLoopVertices = featureOptions.minFeatureLoopVertices;
    options.useNormalTensorFeatures = false;
    options.qualityRefinementIterations = 2;
    const manumesh::test::SimplifiedMesh refined = manumesh::test::simplifyWithReport(input, options);

    const manumesh::feature::FeatureAnalysis after =
        manumesh::feature::detectFeatureCurves(refined.mesh, featureOptions);
    EXPECT_EQ(manumesh::test::countCircularLoops(before), manumesh::test::countCircularLoops(after));
    EXPECT_NEAR(
        manumesh::test::maxCircularRelativeError(before), manumesh::test::maxCircularRelativeError(after), 1e-12
    );
    EXPECT_EQ(before.featureEdges, after.featureEdges);
}

namespace {

// Distance from p to the nearest of the twelve cube-edge lines of the cube
// [-half, half]^3: the two off-axis coordinates must sit at +-half.
double distanceToNearestCubeEdgeLine(const manumesh::Vec3& p, double half) {
    double best = std::numeric_limits<double>::infinity();
    for (int freeAxis = 0; freeAxis < 3; ++freeAxis) {
        const int axisA = (freeAxis + 1) % 3;
        const int axisB = (freeAxis + 2) % 3;
        const double da = std::abs(p[axisA]) - half;
        const double db = std::abs(p[axisB]) - half;
        best = std::min(best, std::sqrt(da * da + db * db));
    }
    return best;
}

} // namespace

TEST(QualityRefinement, SoftProtectedPolygonalCreaseVerticesOnlySlideAlongTheCrease) {
    // Closed cube: the twelve 90-degree edges become soft-protected polygonal
    // feature loops under PrimitiveCurves (only circles/ellipses are hard
    // protected), so quality refinement is allowed to move crease vertices.
    const double half = 1.0;
    const manumesh::Mesh input = manumesh::generateWeldedCubeGrid(10, 2.0 * half);

    manumesh::simplification::SimplifyOptions baselineOptions;
    baselineOptions.targetRatio = 0.3;
    baselineOptions.preserveFeatureCurves = true;
    baselineOptions.featureProtectionMode = manumesh::simplification::FeatureProtectionMode::PrimitiveCurves;
    baselineOptions.useNormalTensorFeatures = false;
    const manumesh::test::SimplifiedMesh baseline = manumesh::test::simplifyWithReport(input, baselineOptions);

    manumesh::simplification::SimplifyOptions refinedOptions = baselineOptions;
    refinedOptions.qualityRefinementIterations = 5;
    const manumesh::test::SimplifiedMesh refined = manumesh::test::simplifyWithReport(input, refinedOptions);

    // Refinement is a post-collapse pass, so both runs share the collapse
    // sequence and the compacted vertex order corresponds one-to-one.
    ASSERT_EQ(baseline.mesh.vertices.size(), refined.mesh.vertices.size());
    ASSERT_EQ(baseline.mesh.faces.size(), refined.mesh.faces.size());
    EXPECT_GT(refined.report.qualityRefinementAcceptedMoves, 0);

    // Feature-constrained relaxation may only slide crease vertices along the
    // local curve tangent, so their distance to the true crease line must not
    // grow. Before the tangent constraint (average-normal tangent-plane
    // projection only), this same setup rounded the creases: measured max
    // drift growth was 8.66e-02 off the cube edge lines (mesh half-extent
    // 1.0, 5 refinement iterations).
    int creaseVertices = 0;
    double maxDriftGrowth = 0.0;
    for (std::size_t i = 0; i < baseline.mesh.vertices.size(); ++i) {
        const double baselineDistance = distanceToNearestCubeEdgeLine(baseline.mesh.vertices[i], half);
        if (baselineDistance > 1e-6) {
            continue;
        }
        ++creaseVertices;
        const double refinedDistance = distanceToNearestCubeEdgeLine(refined.mesh.vertices[i], half);
        maxDriftGrowth = std::max(maxDriftGrowth, refinedDistance - baselineDistance);
    }
    EXPECT_GT(creaseVertices, 8);
    EXPECT_LE(maxDriftGrowth, 1e-12);
}
