#include "TestSupport.h"

#include "algorithms/feature_detection/FeatureDetector.h"
#include "algorithms/simplification/Metrics.h"
#include "core/MeshGenerators.h"
#include "core/MeshTopology.h"

#include <gtest/gtest.h>

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
    EXPECT_GT(refinedStats.minTriangleQuality, baselineStats.minTriangleQuality);
    EXPECT_GE(refinedStats.meanTriangleQuality, baselineStats.meanTriangleQuality);
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
