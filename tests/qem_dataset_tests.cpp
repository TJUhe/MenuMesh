#include "TestSupport.h"
#include "line_quadrics_qem/algorithms/simplification/Metrics.h"
#include "line_quadrics_qem/features/FeatureDetection.h"

#include <algorithm>
#include <chrono>
#include <gtest/gtest.h>
#include <iomanip>
#include <iostream>
#include <vector>

namespace {

using lq::test::caseFieldInt;
using lq::test::CaseLine;
using lq::test::circularFeatureOptions;
using lq::test::countCircularLoops;
using lq::test::expectBudget;
using lq::test::lineOptions;
using lq::test::loadCaseMesh;
using lq::test::maxCircularRelativeError;
using lq::test::protectedOptions;
using lq::test::readCaseLines;
using lq::test::SimplifiedMesh;
using lq::test::simplifyWithReport;

double elapsedMs(std::chrono::steady_clock::time_point start,
                 std::chrono::steady_clock::time_point end) {
  return std::chrono::duration<double, std::milli>(end - start).count();
}

} // namespace

TEST(LineQuadricsQemDataset, AllQemTestStlInputsSimplifyAtSmokeBudget) {
  const std::vector<CaseLine> cases = readCaseLines("all_stl/cases.txt");
  ASSERT_GE(cases.size(), 100u);

  std::cout << "\nqem_test all_stl smoke simplify, ratio=0.90\n";
  std::cout << "model,vertices,input_faces,output_faces,load_ms,simplify_ms\n";

  for (const CaseLine& testCase : cases) {
    SCOPED_TRACE(testCase.relativePath.generic_string());

    const auto loadStart = std::chrono::steady_clock::now();
    const lq::Mesh input = loadCaseMesh(testCase.relativePath);
    const auto loadEnd = std::chrono::steady_clock::now();
    ASSERT_FALSE(input.empty());

    const lq::SimplifyOptions options = lineOptions(0.90);
    const auto simplifyStart = std::chrono::steady_clock::now();
    const SimplifiedMesh result = simplifyWithReport(input, options);
    const auto simplifyEnd = std::chrono::steady_clock::now();

    expectBudget(result, input, 0.90);
    const lq::MeshStats inputStats = lq::computeMeshStats(input);
    const lq::MeshStats outputStats = lq::computeMeshStats(result.mesh);
    EXPECT_LE(outputStats.nonManifoldEdges, inputStats.nonManifoldEdges)
        << "simplification should not add non-manifold edges";

    std::cout << testCase.relativePath.generic_string() << "," << input.vertices.size()
              << "," << input.faces.size() << "," << result.mesh.faces.size() << ","
              << std::fixed << std::setprecision(2) << elapsedMs(loadStart, loadEnd)
              << "," << elapsedMs(simplifyStart, simplifyEnd) << "\n";
  }
}

TEST(LineQuadricsQemDataset, CircularHoleCasesHaveAuditableFeaturePreservation) {
  for (const CaseLine& testCase : readCaseLines("circular_holes/cases.txt")) {
    SCOPED_TRACE(testCase.relativePath.generic_string());
    const lq::Mesh input = loadCaseMesh(testCase.relativePath);
    ASSERT_FALSE(input.empty());

    const int minCircularLoops = caseFieldInt(testCase, 0, 1);
    const int minFeatureEdges = caseFieldInt(testCase, 1, 1);
    const lq::FeatureAnalysis originalFeatures =
        lq::detectFeatureCurves(input, circularFeatureOptions());
    ASSERT_GE(originalFeatures.featureEdges, minFeatureEdges);
    ASSERT_GE(countCircularLoops(originalFeatures), minCircularLoops);

    const SimplifiedMesh line = simplifyWithReport(input, lineOptions(0.80));
    const SimplifiedMesh protectedResult =
        simplifyWithReport(input, protectedOptions(0.80));

    expectBudget(line, input, 0.80);
    expectBudget(protectedResult, input, 0.80);
    EXPECT_GT(protectedResult.report.featureLoops, 0);
    EXPECT_GT(protectedResult.report.circularFeatureLoops, 0);
    EXPECT_GT(protectedResult.report.projectedFeaturePlacements, 0);
    EXPECT_GT(protectedResult.report.featureRejectedCollapses, 0);

    const lq::FeatureAnalysis lineFeatures =
        lq::detectFeatureCurves(line.mesh, circularFeatureOptions());
    const lq::FeatureAnalysis protectedFeatures =
        lq::detectFeatureCurves(protectedResult.mesh, circularFeatureOptions());
    const int protectedCircularLoops = countCircularLoops(protectedFeatures);
    EXPECT_GE(protectedCircularLoops, std::max(1, minCircularLoops / 2));
    EXPECT_GE(protectedCircularLoops + 2, countCircularLoops(lineFeatures));
    EXPECT_LT(maxCircularRelativeError(protectedFeatures), 0.08);
  }
}

TEST(LineQuadricsQemDataset, ClosedTopologyCasesStayClosedAfterLineSimplify) {
  for (const CaseLine& testCase : readCaseLines("closed_topology/cases.txt")) {
    SCOPED_TRACE(testCase.relativePath.generic_string());
    const lq::Mesh input = loadCaseMesh(testCase.relativePath);
    ASSERT_FALSE(input.empty());

    const lq::MeshStats inputStats = lq::computeMeshStats(input);
    ASSERT_EQ(inputStats.boundaryEdges, 0);
    ASSERT_EQ(inputStats.nonManifoldEdges, 0);

    const SimplifiedMesh result = simplifyWithReport(input, lineOptions(0.80));
    expectBudget(result, input, 0.80);

    const lq::MeshStats outputStats = lq::computeMeshStats(result.mesh);
    EXPECT_EQ(outputStats.boundaryEdges, 0);
    EXPECT_EQ(outputStats.nonManifoldEdges, 0);
  }
}

TEST(LineQuadricsQemDataset, NonCircularHardFeatureCasesExerciseProtection) {
  for (const CaseLine& testCase : readCaseLines("non_circular_features/cases.txt")) {
    SCOPED_TRACE(testCase.relativePath.generic_string());
    const lq::Mesh input = loadCaseMesh(testCase.relativePath);
    ASSERT_FALSE(input.empty());

    const lq::FeatureAnalysis originalFeatures =
        lq::detectFeatureCurves(input, circularFeatureOptions());
    ASSERT_GT(originalFeatures.featureEdges, 0);

    const SimplifiedMesh protectedResult =
        simplifyWithReport(input, protectedOptions(0.85));
    expectBudget(protectedResult, input, 0.85);
    EXPECT_GT(protectedResult.report.featureLoops, 0);
    EXPECT_GT(protectedResult.report.featureVertices, 0);
    EXPECT_GT(protectedResult.report.featureRejectedCollapses, 0);
  }
}
