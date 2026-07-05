#include "TestSupport.h"
#include "line_quadrics_qem/algorithms/simplification/Metrics.h"
#include "line_quadrics_qem/features/FeatureDetection.h"

#include <algorithm>
#include <gtest/gtest.h>
#include <vector>

namespace {

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
using lq::test::standardOptions;

std::vector<lq::FeatureLoop> innerEllipseLoops(const lq::FeatureAnalysis& analysis) {
  std::vector<lq::FeatureLoop> loops;
  for (const lq::FeatureLoop& loop : analysis.loops) {
    if (loop.primitive == lq::FeaturePrimitiveType::Ellipse && loop.majorRadius < 1.0 &&
        loop.minorRadius > 0.0) {
      loops.push_back(loop);
    }
  }
  return loops;
}

} // namespace

TEST(LineQuadricsQemParameters, TargetFacesOverridesRatioOnRealStlFixtures) {
  for (const CaseLine& testCase : readCaseLines("parameter_sensitivity/cases.txt")) {
    SCOPED_TRACE(testCase.relativePath.generic_string());
    const lq::Mesh input = loadCaseMesh(testCase.relativePath);
    ASSERT_FALSE(input.empty());

    lq::SimplifyOptions options = lineOptions(0.98);
    options.targetFaces = std::max(4, static_cast<int>(input.faces.size() * 0.82));

    const SimplifiedMesh result = simplifyWithReport(input, options);
    EXPECT_FALSE(result.mesh.empty());
    EXPECT_EQ(result.report.initialFaces, static_cast<int>(input.faces.size()));
    EXPECT_EQ(result.report.finalFaces, static_cast<int>(result.mesh.faces.size()));
    EXPECT_LT(result.report.finalFaces, result.report.initialFaces);
    EXPECT_LE(result.report.finalFaces, options.targetFaces + 2);
  }
}

TEST(LineQuadricsQemParameters, LineQuadricsExposeWeightDiagnosticsOnRealStlFixtures) {
  bool sawSpatiallyVaryingLineWeight = false;
  for (const CaseLine& testCase : readCaseLines("parameter_sensitivity/cases.txt")) {
    SCOPED_TRACE(testCase.relativePath.generic_string());
    const lq::Mesh input = loadCaseMesh(testCase.relativePath);
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
        sawSpatiallyVaryingLineWeight ||
        line.report.maxAppliedLineWeight > line.report.minAppliedLineWeight;
  }
  EXPECT_TRUE(sawSpatiallyVaryingLineWeight);
}

TEST(LineQuadricsQemParameters, FeatureProtectionChangesCircularHoleDiagnostics) {
  const std::vector<CaseLine> cases = readCaseLines("circular_holes/cases.txt");
  ASSERT_FALSE(cases.empty());

  const lq::Mesh input = loadCaseMesh(cases.front().relativePath);
  ASSERT_FALSE(input.empty());

  lq::SimplifyOptions permissive = protectedOptions(0.80);
  permissive.minFeatureLoopVertices = 8;
  const SimplifiedMesh lowThreshold = simplifyWithReport(input, permissive);

  lq::SimplifyOptions strict = protectedOptions(0.80);
  strict.minFeatureLoopVertices = 100000;
  const SimplifiedMesh highThreshold = simplifyWithReport(input, strict);

  EXPECT_GT(lowThreshold.report.circularFeatureLoops, 0);
  EXPECT_GT(lowThreshold.report.projectedFeaturePlacements, 0);
  EXPECT_GT(lowThreshold.report.featureRejectedCollapses, 0);
  EXPECT_GT(highThreshold.report.featureLoops, 0);
  EXPECT_EQ(0, highThreshold.report.circularFeatureLoops);
  EXPECT_EQ(0, highThreshold.report.projectedFeaturePlacements);

  const lq::FeatureAnalysis lowFeatures =
      lq::detectFeatureCurves(lowThreshold.mesh, circularFeatureOptions());
  EXPECT_GT(countCircularLoops(lowFeatures), 0);
}

TEST(LineQuadricsQemParameters,
     FeatureProtectedCircularLoopsRemainDetectableAfterAggressiveSimplify) {
  const lq::Mesh input = loadCaseMesh("feature_fixtures/coaxial_hole_plate.obj");
  ASSERT_FALSE(input.empty());

  lq::SimplifyOptions options = protectedOptions(0.25);
  options.protectAllFeatureEdges = false;
  options.circleFitRelativeThreshold = 0.04;
  options.minFeatureLoopVertices = 12;
  const SimplifiedMesh result = simplifyWithReport(input, options);

  expectBudget(result, input, 0.25);
  EXPECT_GT(result.report.circularFeatureLoops, 0);
  EXPECT_GT(result.report.projectedFeaturePlacements, 0);

  lq::FeatureOptions outputFeatureOptions = circularFeatureOptions();
  outputFeatureOptions.circleFitRelativeThreshold = 0.16;
  outputFeatureOptions.minFeatureLoopVertices = options.minCircularFeatureLoopVertices;
  const lq::FeatureAnalysis outputFeatures =
      lq::detectFeatureCurves(result.mesh, outputFeatureOptions);

  EXPECT_GE(countCircularLoops(outputFeatures), 4);
}

TEST(LineQuadricsQemParameters, EllipsePrimitiveUsesPrimitiveFeatureProjection) {
  const lq::Mesh input = loadCaseMesh("feature_fixtures/elliptical_hole_plate.obj");
  ASSERT_FALSE(input.empty());

  lq::FeatureOptions featureOptions = circularFeatureOptions();
  featureOptions.ellipseFitRelativeThreshold = 0.03;
  const lq::FeatureAnalysis features = lq::detectFeatureCurves(input, featureOptions);
  ASSERT_GE(innerEllipseLoops(features).size(), 2u);

  lq::SimplifyOptions options = protectedOptions(0.85);
  options.ellipseFitRelativeThreshold = 0.03;
  options.minFeatureLoopVertices = 8;
  const SimplifiedMesh result = simplifyWithReport(input, options);

  expectBudget(result, input, 0.85);
  EXPECT_GT(result.report.featureLoops, 0);
  EXPECT_GT(result.report.projectedFeaturePlacements, 0);
  EXPECT_GT(result.report.featureRejectedCollapses, 0);

  lq::FeatureOptions outputFeatureOptions = circularFeatureOptions();
  outputFeatureOptions.ellipseFitRelativeThreshold = 0.12;
  outputFeatureOptions.minFeatureLoopVertices = 6;
  const lq::FeatureAnalysis outputFeatures =
      lq::detectFeatureCurves(result.mesh, outputFeatureOptions);
  const std::vector<lq::FeatureLoop> outputEllipses = innerEllipseLoops(outputFeatures);

  ASSERT_GE(outputEllipses.size(), 2u);
  for (const lq::FeatureLoop& loop : outputEllipses) {
    EXPECT_NEAR(loop.axisRatio, 0.45 / 0.8, 0.10);
    EXPECT_LT(loop.rmsEllipseError, 0.08);
    EXPECT_LT(loop.rmsPlaneError, 0.05);
  }
}

TEST(LineQuadricsQemParameters,
     EllipsePrimitiveIsProtectedWithoutProtectAllFeatureEdges) {
  const lq::Mesh input = loadCaseMesh("feature_fixtures/elliptical_hole_plate.obj");
  ASSERT_FALSE(input.empty());

  lq::SimplifyOptions options = protectedOptions(0.85);
  options.protectAllFeatureEdges = false;
  options.ellipseFitRelativeThreshold = 0.03;
  const SimplifiedMesh result = simplifyWithReport(input, options);

  expectBudget(result, input, 0.85);
  EXPECT_GT(result.report.featureRejectedCollapses, 0);
  EXPECT_GT(result.report.projectedFeaturePlacements, 0);

  lq::FeatureOptions outputFeatureOptions = circularFeatureOptions();
  outputFeatureOptions.ellipseFitRelativeThreshold = 0.12;
  outputFeatureOptions.minFeatureLoopVertices = 6;
  const lq::FeatureAnalysis outputFeatures =
      lq::detectFeatureCurves(result.mesh, outputFeatureOptions);

  EXPECT_GE(innerEllipseLoops(outputFeatures).size(), 2u);
}

TEST(LineQuadricsQemParameters,
     PrimitiveModeSoftensGenericCreasesOnExternalFandisk) {
  const lq::Mesh input = loadCaseMesh("external/fandisk_2014.stl");
  ASSERT_FALSE(input.empty());

  lq::SimplifyOptions primitive = protectedOptions(0.25);
  primitive.protectAllFeatureEdges = false;
  primitive.featureProtectionMode = lq::FeatureProtectionMode::PrimitiveCurves;
  primitive.useNormalTensorFeatures = false;
  primitive.featureAngleDeg = 30.0;
  primitive.maxNormalDeviationDeg = 85.0;
  primitive.minTriangleQuality = 1e-6;
  const SimplifiedMesh primitiveResult = simplifyWithReport(input, primitive);

  lq::SimplifyOptions strict = primitive;
  strict.featureProtectionMode = lq::FeatureProtectionMode::AllFeatureEdges;
  const SimplifiedMesh strictResult = simplifyWithReport(input, strict);

  expectBudget(primitiveResult, input, 0.25);
  EXPECT_EQ(lq::SimplifyTerminationReason::ReachedTarget,
            primitiveResult.report.terminationReason);
  EXPECT_EQ(0, primitiveResult.report.genericFeatureRejectedCollapses);
  EXPECT_GT(strictResult.report.genericFeatureRejectedCollapses, 0);
  EXPECT_LT(primitiveResult.report.featureRejectedCollapses,
            strictResult.report.featureRejectedCollapses);
}

TEST(LineQuadricsQemParameters, StrictQualityModeImprovesWorstThingi10kFixture) {
  const lq::Mesh input = loadCaseMesh(
      "external/thingi10k/thingi10k_104188_iphone_tank_case_gen_4_and_4s.stl");
  ASSERT_FALSE(input.empty());

  lq::SimplifyOptions defaultOptions = lineOptions(0.50);
  const SimplifiedMesh defaultResult = simplifyWithReport(input, defaultOptions);

  lq::SimplifyOptions strictOptions = lineOptions(0.50);
  strictOptions.preserveBoundary = true;
  strictOptions.minTriangleQuality = 1e-4;
  strictOptions.maxNormalDeviationDeg = 75.0;
  const SimplifiedMesh strictResult = simplifyWithReport(input, strictOptions);

  expectBudget(defaultResult, input, 0.50);
  expectBudget(strictResult, input, 0.50);
  EXPECT_GT(strictResult.report.qualityRejectedCollapses, 0);

  const lq::MeshStats defaultStats = lq::computeMeshStats(defaultResult.mesh);
  const lq::MeshStats strictStats = lq::computeMeshStats(strictResult.mesh);
  EXPECT_GT(strictStats.minTriangleQuality, defaultStats.minTriangleQuality);
}

TEST(LineQuadricsQemParameters, IndustrialGateChecksFeatureDriftDistanceAndTopology) {
  const lq::Mesh input = loadCaseMesh("feature_fixtures/coaxial_hole_plate.obj");
  ASSERT_FALSE(input.empty());

  const lq::FeatureAnalysis originalFeatures =
      lq::detectFeatureCurves(input, circularFeatureOptions());
  ASSERT_GE(countCircularLoops(originalFeatures), 4);

  lq::SimplifyOptions options = protectedOptions(0.80);
  options.protectAllFeatureEdges = false;
  options.preserveBoundary = true;
  options.preventLocalIntersections = true;
  options.maxLocalErrorRatio = 0.05;
  options.maxFeatureCurveDeviationRatio = 0.10;
  options.minTriangleQuality = 1e-6;
  options.maxNormalDeviationDeg = 85.0;
  const SimplifiedMesh result = simplifyWithReport(input, options);

  expectBudget(result, input, 0.80);
  EXPECT_EQ(result.report.rejectedCollapses,
            result.report.featureRejectedCollapses +
                result.report.boundaryRejectedCollapses +
                result.report.topologyRejectedCollapses +
                result.report.normalFlipRejectedCollapses +
                result.report.qualityRejectedCollapses +
                result.report.selfIntersectionRejectedCollapses +
                result.report.curveBudgetRejectedCollapses +
                result.report.errorRejectedCollapses);

  const lq::FeatureAnalysis outputFeatures =
      lq::detectFeatureCurves(result.mesh, circularFeatureOptions());
  EXPECT_GE(countCircularLoops(outputFeatures), 2);
  EXPECT_LT(maxCircularRelativeError(outputFeatures), 0.10);

  const lq::MeshStats inputStats = lq::computeMeshStats(input);
  const lq::MeshStats outputStats = lq::computeMeshStats(result.mesh);
  EXPECT_LE(outputStats.nonManifoldEdges, inputStats.nonManifoldEdges);
  EXPECT_GE(outputStats.minTriangleQuality, 0.0);

  const lq::DistanceStats distance =
      lq::compareMeshesBySampledDistance(input, result.mesh, 512);
  EXPECT_LT(distance.maxOriginalToSimplified, 0.20 * input.bboxDiag());
}
