/**
 * @file tests/performance/qem_dataset_tests.cpp
 * @brief Verifies qem dataset tests behavior in the ManuMesh tests.
 * @ingroup manumesh_tests
 *
 * @details The fixture and assertions document observable contracts, numeric tolerances, determinism requirements, and previously fixed regressions.
 */

#include "TestSupport.h"
#include "algorithms/feature_detection/FeatureDetector.h"
#include "algorithms/simplification/Metrics.h"

#include <algorithm>
#include <chrono>
#include <gtest/gtest.h>
#include <iomanip>
#include <iostream>
#include <vector>

namespace {

using manumesh::test::caseFieldInt;
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

double elapsedMs(std::chrono::steady_clock::time_point start, std::chrono::steady_clock::time_point end) {
    return std::chrono::duration<double, std::milli>(end - start).count();
}

} // namespace

TEST(ManuMeshDataset, AllQemTestStlInputsSimplifyAtSmokeBudget) {
    const std::vector<CaseLine> cases = readCaseLines("all_stl/cases.txt");
    ASSERT_GE(cases.size(), 100u);

    std::cout << "\nqem_test all_stl smoke simplify, ratio=0.90\n";
    std::cout << "model,vertices,input_faces,output_faces,load_ms,simplify_ms\n";

    for (const CaseLine& testCase : cases) {
        SCOPED_TRACE(testCase.relativePath.generic_string());

        const auto loadStart = std::chrono::steady_clock::now();
        const manumesh::Mesh input = loadCaseMesh(testCase.relativePath);
        const auto loadEnd = std::chrono::steady_clock::now();
        ASSERT_FALSE(input.empty());

        const manumesh::simplification::SimplifyOptions options = lineOptions(0.90);
        const auto simplifyStart = std::chrono::steady_clock::now();
        const SimplifiedMesh result = simplifyWithReport(input, options);
        const auto simplifyEnd = std::chrono::steady_clock::now();

        expectBudget(result, input, 0.90);
        const manumesh::simplification::MeshStats inputStats = manumesh::simplification::computeMeshStats(input);
        const manumesh::simplification::MeshStats outputStats = manumesh::simplification::computeMeshStats(result.mesh);
        EXPECT_LE(outputStats.nonManifoldEdges, inputStats.nonManifoldEdges)
            << "simplification should not add non-manifold edges";

        std::cout << testCase.relativePath.generic_string() << "," << input.vertices.size() << "," << input.faces.size()
                  << "," << result.mesh.faces.size() << "," << std::fixed << std::setprecision(2)
                  << elapsedMs(loadStart, loadEnd) << "," << elapsedMs(simplifyStart, simplifyEnd) << "\n";
    }
}

TEST(ManuMeshDataset, CircularHoleCasesHaveAuditableFeaturePreservation) {
    for (const CaseLine& testCase : readCaseLines("circular_holes/cases.txt")) {
        SCOPED_TRACE(testCase.relativePath.generic_string());
        const manumesh::Mesh input = loadCaseMesh(testCase.relativePath);
        ASSERT_FALSE(input.empty());

        const int minCircularLoops = caseFieldInt(testCase, 0, 1);
        const int minFeatureEdges = caseFieldInt(testCase, 1, 1);
        const manumesh::feature::FeatureAnalysis originalFeatures =
            manumesh::feature::detectFeatureCurves(input, circularFeatureOptions());
        ASSERT_GE(originalFeatures.featureEdges, minFeatureEdges);
        ASSERT_GE(countCircularLoops(originalFeatures), minCircularLoops);

        const SimplifiedMesh line = simplifyWithReport(input, lineOptions(0.80));
        const SimplifiedMesh protectedResult = simplifyWithReport(input, protectedOptions(0.80));

        expectBudget(line, input, 0.80);
        EXPECT_FALSE(protectedResult.mesh.empty());
        EXPECT_EQ(protectedResult.report.initialFaces, static_cast<int>(input.faces.size()));
        EXPECT_EQ(protectedResult.report.finalFaces, static_cast<int>(protectedResult.mesh.faces.size()));
        EXPECT_LT(protectedResult.report.finalFaces, protectedResult.report.initialFaces);
        EXPECT_GT(protectedResult.report.collapsedEdges, 0);
        EXPECT_GT(protectedResult.report.featureLoops, 0);
        EXPECT_GT(protectedResult.report.circularFeatureLoops, 0);
        EXPECT_GT(protectedResult.report.projectedFeaturePlacements, 0);

        const manumesh::feature::FeatureAnalysis lineFeatures =
            manumesh::feature::detectFeatureCurves(line.mesh, circularFeatureOptions());
        const manumesh::feature::FeatureAnalysis protectedFeatures =
            manumesh::feature::detectFeatureCurves(protectedResult.mesh, circularFeatureOptions());
        const int protectedCircularLoops = countCircularLoops(protectedFeatures);
        EXPECT_GT(countCircularLoops(lineFeatures), 0);
        EXPECT_GE(protectedCircularLoops, 1);
        EXPECT_LT(maxCircularRelativeError(protectedFeatures), 0.08);
    }
}

TEST(ManuMeshDataset, ClosedTopologyCasesStayClosedAfterLineSimplify) {
    for (const CaseLine& testCase : readCaseLines("closed_topology/cases.txt")) {
        SCOPED_TRACE(testCase.relativePath.generic_string());
        const manumesh::Mesh input = loadCaseMesh(testCase.relativePath);
        ASSERT_FALSE(input.empty());

        const manumesh::simplification::MeshStats inputStats = manumesh::simplification::computeMeshStats(input);
        ASSERT_EQ(inputStats.boundaryEdges, 0);
        ASSERT_EQ(inputStats.nonManifoldEdges, 0);

        const SimplifiedMesh result = simplifyWithReport(input, lineOptions(0.80));
        expectBudget(result, input, 0.80);

        const manumesh::simplification::MeshStats outputStats = manumesh::simplification::computeMeshStats(result.mesh);
        EXPECT_EQ(outputStats.boundaryEdges, 0);
        EXPECT_EQ(outputStats.nonManifoldEdges, 0);
    }
}

TEST(ManuMeshDataset, NonCircularHardFeatureCasesExerciseProtection) {
    for (const CaseLine& testCase : readCaseLines("non_circular_features/cases.txt")) {
        SCOPED_TRACE(testCase.relativePath.generic_string());
        const manumesh::Mesh input = loadCaseMesh(testCase.relativePath);
        ASSERT_FALSE(input.empty());

        const manumesh::feature::FeatureAnalysis originalFeatures =
            manumesh::feature::detectFeatureCurves(input, circularFeatureOptions());
        ASSERT_GT(originalFeatures.featureEdges, 0);

        manumesh::simplification::SimplifyOptions primitiveOptions = protectedOptions(0.85);
        primitiveOptions.featureProtectionMode = manumesh::simplification::FeatureProtectionMode::PrimitiveCurves;
        const SimplifiedMesh primitiveResult = simplifyWithReport(input, primitiveOptions);
        expectBudget(primitiveResult, input, 0.85);
        EXPECT_GT(primitiveResult.report.featureLoops, 0);
        EXPECT_GT(primitiveResult.report.featureVertices, 0);

        manumesh::simplification::SimplifyOptions strictOptions = primitiveOptions;
        strictOptions.featureProtectionMode = manumesh::simplification::FeatureProtectionMode::AllFeatureEdges;
        const SimplifiedMesh strictResult = simplifyWithReport(input, strictOptions);
        EXPECT_FALSE(strictResult.mesh.empty());
        EXPECT_EQ(strictResult.report.initialFaces, static_cast<int>(input.faces.size()));
        EXPECT_EQ(strictResult.report.finalFaces, static_cast<int>(strictResult.mesh.faces.size()));
        EXPECT_LT(strictResult.report.finalFaces, strictResult.report.initialFaces);
        EXPECT_GE(strictResult.report.finalFaces, primitiveResult.report.finalFaces);
        EXPECT_GT(strictResult.report.featureLoops, 0);
        EXPECT_GT(strictResult.report.featureVertices, 0);
        EXPECT_GT(strictResult.report.featureRejectedCollapses, 0);
        EXPECT_GT(strictResult.report.genericFeatureRejectedCollapses, 0);
    }
}
