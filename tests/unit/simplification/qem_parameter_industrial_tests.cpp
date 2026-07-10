#include "QemParameterTestSupport.h"
#include "TestSupport.h"
#include "algorithms/feature_detection/FeatureDetector.h"
#include "algorithms/simplification/Metrics.h"
#include "core/MeshGenerators.h"

#include <algorithm>
#include <gtest/gtest.h>
#include <vector>

using manumesh::test::CaseLine;
using manumesh::test::circularFeatureOptions;
using manumesh::test::countCircularLoops;
using manumesh::test::expectBudget;
using manumesh::test::lineOptions;
using manumesh::test::loadCaseMesh;
using manumesh::test::maxCircularRelativeError;
using manumesh::test::protectedOptions;
using manumesh::test::readCaseLines;
using manumesh::test::SimplifiedMesh;
using manumesh::test::simplifyWithReport;
using manumesh::test::standardOptions;
using manumesh::test::qem_parameters::innerEllipseLoops;
TEST(ManuMeshParameters, StrictQualityModeImprovesWorstThingi10kFixture) {
    const manumesh::Mesh input = loadCaseMesh("external/thingi10k/thingi10k_104188_iphone_tank_case_gen_4_and_4s.stl");
    ASSERT_FALSE(input.empty());

    manumesh::simplification::SimplifyOptions defaultOptions = lineOptions(0.50);
    const SimplifiedMesh defaultResult = simplifyWithReport(input, defaultOptions);

    manumesh::simplification::SimplifyOptions strictOptions = lineOptions(0.50);
    strictOptions.preserveBoundary = true;
    strictOptions.minTriangleQuality = 1e-4;
    strictOptions.maxNormalDeviationDeg = 75.0;
    const SimplifiedMesh strictResult = simplifyWithReport(input, strictOptions);

    expectBudget(defaultResult, input, 0.50);
    expectBudget(strictResult, input, 0.50);
    EXPECT_GT(strictResult.report.qualityRejectedCollapses, 0);

    const manumesh::simplification::MeshStats defaultStats =
        manumesh::simplification::computeMeshStats(defaultResult.mesh);
    const manumesh::simplification::MeshStats strictStats =
        manumesh::simplification::computeMeshStats(strictResult.mesh);
    EXPECT_GT(strictStats.minTriangleQuality, defaultStats.minTriangleQuality);
}

TEST(ManuMeshParameters, IndustrialGateChecksFeatureDriftDistanceAndTopology) {
    const manumesh::Mesh input = loadCaseMesh("feature_fixtures/coaxial_hole_plate.obj");
    ASSERT_FALSE(input.empty());

    const manumesh::feature::FeatureAnalysis originalFeatures =
        manumesh::feature::detectFeatureCurves(input, circularFeatureOptions());
    ASSERT_GE(countCircularLoops(originalFeatures), 4);

    manumesh::simplification::SimplifyOptions options = protectedOptions(0.80);
    options.preserveBoundary = true;
    options.preventLocalIntersections = true;
    options.maxLocalErrorRatio = 0.05;
    options.maxFeatureCurveDeviationRatio = 0.10;
    options.minTriangleQuality = 1e-6;
    options.maxNormalDeviationDeg = 85.0;
    const SimplifiedMesh result = simplifyWithReport(input, options);

    expectBudget(result, input, 0.80);
    EXPECT_EQ(
        result.report.rejectedCollapses,
        result.report.featureRejectedCollapses + result.report.boundaryRejectedCollapses +
            result.report.topologyRejectedCollapses + result.report.normalFlipRejectedCollapses +
            result.report.qualityRejectedCollapses + result.report.selfIntersectionRejectedCollapses +
            result.report.curveBudgetRejectedCollapses + result.report.errorRejectedCollapses
    );

    const manumesh::feature::FeatureAnalysis outputFeatures =
        manumesh::feature::detectFeatureCurves(result.mesh, circularFeatureOptions());
    EXPECT_GE(countCircularLoops(outputFeatures), 2);
    EXPECT_LT(maxCircularRelativeError(outputFeatures), 0.10);

    const manumesh::simplification::MeshStats inputStats = manumesh::simplification::computeMeshStats(input);
    const manumesh::simplification::MeshStats outputStats = manumesh::simplification::computeMeshStats(result.mesh);
    EXPECT_LE(outputStats.nonManifoldEdges, inputStats.nonManifoldEdges);
    EXPECT_GE(outputStats.minTriangleQuality, 0.0);

    const manumesh::simplification::DistanceStats distance =
        manumesh::simplification::compareMeshesBySampledDistance(input, result.mesh, 512);
    EXPECT_LT(distance.maxOriginalToSimplified, 0.20 * input.bboxDiag());
}

TEST(ManuMeshParameters, ReferenceSurfaceEnvelopeReducesCumulativeTerrainDrift) {
    const manumesh::Mesh input = manumesh::generateSineTerrainGrid(28, 2.0);
    ASSERT_FALSE(input.empty());

    manumesh::simplification::SimplifyOptions freeOptions = lineOptions(0.12);
    freeOptions.maxNormalDeviationDeg = 180.0;
    const SimplifiedMesh freeResult = simplifyWithReport(input, freeOptions);

    manumesh::simplification::SimplifyOptions boundedOptions = freeOptions;
    boundedOptions.maxLocalErrorRatio = 0.01;
    const SimplifiedMesh boundedResult = simplifyWithReport(input, boundedOptions);

    expectBudget(freeResult, input, 0.12);
    expectBudget(boundedResult, input, 0.12);
    EXPECT_GT(boundedResult.report.errorRejectedCollapses, 0);

    const manumesh::simplification::DistanceStats freeDistance =
        manumesh::simplification::compareMeshesBySampledDistance(input, freeResult.mesh, 3000);
    const manumesh::simplification::DistanceStats boundedDistance =
        manumesh::simplification::compareMeshesBySampledDistance(input, boundedResult.mesh, 3000);

    EXPECT_LT(boundedDistance.meanOriginalToSimplified, freeDistance.meanOriginalToSimplified);
    EXPECT_LT(boundedDistance.maxOriginalToSimplified, 0.5 * freeDistance.maxOriginalToSimplified);
    EXPECT_LT(boundedDistance.maxSimplifiedToOriginal, 0.015 * input.bboxDiag());
}
