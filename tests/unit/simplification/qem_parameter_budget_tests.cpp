/**
 * @file tests/unit/simplification/qem_parameter_budget_tests.cpp
 * @brief Verifies qem parameter budget tests behavior in the ManuMesh tests.
 * @ingroup manumesh_tests
 *
 * @details The fixture and assertions document observable contracts, numeric tolerances, determinism requirements, and previously fixed regressions.
 */

#include "QemParameterTestSupport.h"
#include "TestSupport.h"
#include "algorithms/feature_detection/FeatureDetector.h"
#include "algorithms/simplification/Metrics.h"

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
TEST(ManuMeshParameters, TargetFacesOverridesRatioOnRealStlFixtures) {
    for (const CaseLine& testCase : readCaseLines("parameter_sensitivity/cases.txt")) {
        SCOPED_TRACE(testCase.relativePath.generic_string());
        const manumesh::Mesh input = loadCaseMesh(testCase.relativePath);
        ASSERT_FALSE(input.empty());

        manumesh::simplification::SimplifyOptions options = lineOptions(0.98);
        options.targetFaces = std::max(4, static_cast<int>(input.faces.size() * 0.82));

        const SimplifiedMesh result = simplifyWithReport(input, options);
        EXPECT_FALSE(result.mesh.empty());
        EXPECT_EQ(result.report.initialFaces, static_cast<int>(input.faces.size()));
        EXPECT_EQ(result.report.finalFaces, static_cast<int>(result.mesh.faces.size()));
        EXPECT_LT(result.report.finalFaces, result.report.initialFaces);
        // Slack derivation: collapseUntilTarget stops at the first
        // activeFaceCount <= targetFaces, and one collapse removes 2 faces
        // (interior edge) or 1 face (boundary edge), so a reached-target run
        // always ends in [targetFaces - 1, targetFaces] (measured on these
        // fixtures: deltas of -1, 0, -1). The +2 headroom only matters when
        // the queue is exhausted or the rejection limit fires just above the
        // target, allowing the run to stop within one interior collapse
        // (2 faces) above targetFaces instead of failing the suite on a
        // legitimate no-candidates termination.
        EXPECT_LE(result.report.finalFaces, options.targetFaces + 2);
    }
}

TEST(ManuMeshParameters, LineQuadricsExposeWeightDiagnosticsOnRealStlFixtures) {
    bool sawSpatiallyVaryingLineWeight = false;
    for (const CaseLine& testCase : readCaseLines("parameter_sensitivity/cases.txt")) {
        SCOPED_TRACE(testCase.relativePath.generic_string());
        const manumesh::Mesh input = loadCaseMesh(testCase.relativePath);
        ASSERT_FALSE(input.empty());

        const SimplifiedMesh standard = simplifyWithReport(input, standardOptions(0.85));
        const SimplifiedMesh line = simplifyWithReport(input, lineOptions(0.85));

        expectBudget(standard, input, 0.85);
        expectBudget(line, input, 0.85);
        EXPECT_EQ(0.0, standard.report.minAppliedLineWeight);
        EXPECT_EQ(0.0, standard.report.maxAppliedLineWeight);
        EXPECT_GE(line.report.minAppliedLineWeight, 1e-3);
        EXPECT_GE(line.report.maxAppliedLineWeight, line.report.minAppliedLineWeight);
        sawSpatiallyVaryingLineWeight =
            sawSpatiallyVaryingLineWeight || line.report.maxAppliedLineWeight > line.report.minAppliedLineWeight;
    }
    EXPECT_TRUE(sawSpatiallyVaryingLineWeight);
}
