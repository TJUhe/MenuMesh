/**
 * @file tests/unit/simplification/qem_parameter_external_tests.cpp
 * @brief Verifies qem parameter external tests behavior in the ManuMesh tests.
 * @ingroup manumesh_tests
 *
 * @details The fixture and assertions document observable contracts, numeric tolerances, determinism requirements, and previously fixed regressions.
 */

// QEM parameter regressions that depend on large external validation meshes
// (tests/data/external). They run in the `external` CTest label so the fast
// suite (`ctest -LE "performance|external"`) stays quick.
#include "QemParameterTestSupport.h"
#include "TestSupport.h"
#include "algorithms/feature_detection/FeatureDetector.h"
#include "algorithms/simplification/Metrics.h"

#include <gtest/gtest.h>
#include <vector>

using manumesh::test::CaseLine;
using manumesh::test::circularFeatureOptions;
using manumesh::test::countCircularLoops;
using manumesh::test::expectBudget;
using manumesh::test::lineOptions;
using manumesh::test::loadCaseMesh;
using manumesh::test::protectedOptions;
using manumesh::test::readCaseLines;
using manumesh::test::SimplifiedMesh;
using manumesh::test::simplifyWithReport;
TEST(ManuMeshParameters, FeatureProtectionChangesCircularHoleDiagnostics) {
    const std::vector<CaseLine> cases = readCaseLines("circular_holes/cases.txt");
    ASSERT_FALSE(cases.empty());

    const manumesh::Mesh input = loadCaseMesh(cases.front().relativePath);
    ASSERT_FALSE(input.empty());

    manumesh::simplification::SimplifyOptions permissive = protectedOptions(0.80);
    permissive.minFeatureLoopVertices = 8;
    const SimplifiedMesh lowThreshold = simplifyWithReport(input, permissive);

    manumesh::simplification::SimplifyOptions strict = protectedOptions(0.80);
    strict.minFeatureLoopVertices = 100000;
    const SimplifiedMesh highThreshold = simplifyWithReport(input, strict);

    EXPECT_GT(lowThreshold.report.circularFeatureLoops, 0);
    EXPECT_GT(lowThreshold.report.projectedFeaturePlacements, 0);
    EXPECT_GT(lowThreshold.report.featureRejectedCollapses, 0);
    EXPECT_GT(highThreshold.report.featureLoops, 0);
    EXPECT_EQ(0, highThreshold.report.circularFeatureLoops);
    EXPECT_EQ(0, highThreshold.report.projectedFeaturePlacements);

    const manumesh::feature::FeatureAnalysis lowFeatures =
        manumesh::feature::detectFeatureCurves(lowThreshold.mesh, circularFeatureOptions());
    EXPECT_GT(countCircularLoops(lowFeatures), 0);
}

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

TEST(ManuMeshParameters, PrimitiveModeSoftensGenericCreasesOnExternalFandisk) {
    const manumesh::Mesh input = loadCaseMesh("external/fandisk_2014.stl");
    ASSERT_FALSE(input.empty());

    manumesh::simplification::SimplifyOptions primitive = protectedOptions(0.25);
    primitive.featureProtectionMode = manumesh::simplification::FeatureProtectionMode::PrimitiveCurves;
    primitive.useNormalTensorFeatures = false;
    primitive.featureAngleDeg = 30.0;
    primitive.maxNormalDeviationDeg = 85.0;
    primitive.minTriangleQuality = 1e-6;
    const SimplifiedMesh primitiveResult = simplifyWithReport(input, primitive);

    manumesh::simplification::SimplifyOptions strict = primitive;
    strict.featureProtectionMode = manumesh::simplification::FeatureProtectionMode::AllFeatureEdges;
    const SimplifiedMesh strictResult = simplifyWithReport(input, strict);

    expectBudget(primitiveResult, input, 0.25);
    EXPECT_EQ(
        manumesh::simplification::SimplifyTerminationReason::ReachedTarget, primitiveResult.report.terminationReason
    );
    EXPECT_EQ(0, primitiveResult.report.genericFeatureRejectedCollapses);
    EXPECT_GT(strictResult.report.genericFeatureRejectedCollapses, 0);
    EXPECT_LT(primitiveResult.report.featureRejectedCollapses, strictResult.report.featureRejectedCollapses);
}
