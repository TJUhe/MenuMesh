#include "TestSupport.h"
#include "manumesh/algorithms/feature_detection/FeatureDetector.h"
#include "manumesh/algorithms/simplification/Metrics.h"

#include <algorithm>
#include <gtest/gtest.h>
#include <vector>

namespace {

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

std::vector<manumesh::feature::FeatureLoop>
innerEllipseLoops(const manumesh::feature::FeatureAnalysis& analysis) {
  std::vector<manumesh::feature::FeatureLoop> loops;
  for (const manumesh::feature::FeatureLoop& loop : analysis.loops) {
    if (loop.primitive == manumesh::feature::FeaturePrimitiveType::Ellipse &&
        loop.majorRadius < 1.0 && loop.minorRadius > 0.0) {
      loops.push_back(loop);
    }
  }
  return loops;
}

} // namespace

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
        sawSpatiallyVaryingLineWeight ||
        line.report.maxAppliedLineWeight > line.report.minAppliedLineWeight;
  }
  EXPECT_TRUE(sawSpatiallyVaryingLineWeight);
}

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
      manumesh::feature::detectFeatureCurves(lowThreshold.mesh,
                                             circularFeatureOptions());
  EXPECT_GT(countCircularLoops(lowFeatures), 0);
}

TEST(ManuMeshParameters,
     FeatureProtectedCircularLoopsRemainDetectableAfterAggressiveSimplify) {
  const manumesh::Mesh input = loadCaseMesh("feature_fixtures/coaxial_hole_plate.obj");
  ASSERT_FALSE(input.empty());

  manumesh::simplification::SimplifyOptions options = protectedOptions(0.25);
  options.circleFitRelativeThreshold = 0.04;
  options.minFeatureLoopVertices = 12;
  const SimplifiedMesh result = simplifyWithReport(input, options);

  expectBudget(result, input, 0.25);
  EXPECT_GT(result.report.circularFeatureLoops, 0);
  EXPECT_GT(result.report.projectedFeaturePlacements, 0);

  manumesh::feature::FeatureOptions outputFeatureOptions = circularFeatureOptions();
  outputFeatureOptions.circleFitRelativeThreshold = 0.16;
  outputFeatureOptions.minFeatureLoopVertices = options.minCircularFeatureLoopVertices;
  const manumesh::feature::FeatureAnalysis outputFeatures =
      manumesh::feature::detectFeatureCurves(result.mesh, outputFeatureOptions);

  EXPECT_GE(countCircularLoops(outputFeatures), 3);
}

TEST(ManuMeshParameters, EllipsePrimitiveUsesPrimitiveFeatureProjection) {
  const manumesh::Mesh input =
      loadCaseMesh("feature_fixtures/elliptical_hole_plate.obj");
  ASSERT_FALSE(input.empty());

  manumesh::feature::FeatureOptions featureOptions = circularFeatureOptions();
  featureOptions.ellipseFitRelativeThreshold = 0.03;
  const manumesh::feature::FeatureAnalysis features =
      manumesh::feature::detectFeatureCurves(input, featureOptions);
  ASSERT_GE(innerEllipseLoops(features).size(), 2u);

  manumesh::simplification::SimplifyOptions options = protectedOptions(0.85);
  options.ellipseFitRelativeThreshold = 0.03;
  options.minFeatureLoopVertices = 8;
  const SimplifiedMesh result = simplifyWithReport(input, options);

  expectBudget(result, input, 0.85);
  EXPECT_GT(result.report.featureLoops, 0);
  EXPECT_GT(result.report.projectedFeaturePlacements, 0);
  EXPECT_GT(result.report.featureRejectedCollapses, 0);

  manumesh::feature::FeatureOptions outputFeatureOptions = circularFeatureOptions();
  outputFeatureOptions.ellipseFitRelativeThreshold = 0.12;
  outputFeatureOptions.minFeatureLoopVertices = 6;
  const manumesh::feature::FeatureAnalysis outputFeatures =
      manumesh::feature::detectFeatureCurves(result.mesh, outputFeatureOptions);
  const std::vector<manumesh::feature::FeatureLoop> outputEllipses =
      innerEllipseLoops(outputFeatures);

  ASSERT_GE(outputEllipses.size(), 2u);
  for (const manumesh::feature::FeatureLoop& loop : outputEllipses) {
    EXPECT_NEAR(loop.axisRatio, 0.45 / 0.8, 0.10);
    EXPECT_LT(loop.rmsEllipseError, 0.08);
    EXPECT_LT(loop.rmsPlaneError, 0.05);
  }
}

TEST(ManuMeshParameters, EllipsePrimitiveIsProtectedByPrimitiveMode) {
  const manumesh::Mesh input =
      loadCaseMesh("feature_fixtures/elliptical_hole_plate.obj");
  ASSERT_FALSE(input.empty());

  manumesh::simplification::SimplifyOptions options = protectedOptions(0.85);
  options.ellipseFitRelativeThreshold = 0.03;
  const SimplifiedMesh result = simplifyWithReport(input, options);

  expectBudget(result, input, 0.85);
  EXPECT_GT(result.report.featureRejectedCollapses, 0);
  EXPECT_GT(result.report.projectedFeaturePlacements, 0);

  manumesh::feature::FeatureOptions outputFeatureOptions = circularFeatureOptions();
  outputFeatureOptions.ellipseFitRelativeThreshold = 0.12;
  outputFeatureOptions.minFeatureLoopVertices = 6;
  const manumesh::feature::FeatureAnalysis outputFeatures =
      manumesh::feature::detectFeatureCurves(result.mesh, outputFeatureOptions);

  EXPECT_GE(innerEllipseLoops(outputFeatures).size(), 2u);
}

TEST(ManuMeshParameters, PrimitiveModeSoftensGenericCreasesOnExternalFandisk) {
  const manumesh::Mesh input = loadCaseMesh("external/fandisk_2014.stl");
  ASSERT_FALSE(input.empty());

  manumesh::simplification::SimplifyOptions primitive = protectedOptions(0.25);
  primitive.featureProtectionMode =
      manumesh::simplification::FeatureProtectionMode::PrimitiveCurves;
  primitive.useNormalTensorFeatures = false;
  primitive.featureAngleDeg = 30.0;
  primitive.maxNormalDeviationDeg = 85.0;
  primitive.minTriangleQuality = 1e-6;
  const SimplifiedMesh primitiveResult = simplifyWithReport(input, primitive);

  manumesh::simplification::SimplifyOptions strict = primitive;
  strict.featureProtectionMode =
      manumesh::simplification::FeatureProtectionMode::AllFeatureEdges;
  const SimplifiedMesh strictResult = simplifyWithReport(input, strict);

  expectBudget(primitiveResult, input, 0.25);
  EXPECT_EQ(manumesh::simplification::SimplifyTerminationReason::ReachedTarget,
            primitiveResult.report.terminationReason);
  EXPECT_EQ(0, primitiveResult.report.genericFeatureRejectedCollapses);
  EXPECT_GT(strictResult.report.genericFeatureRejectedCollapses, 0);
  EXPECT_LT(primitiveResult.report.featureRejectedCollapses,
            strictResult.report.featureRejectedCollapses);
}

TEST(ManuMeshParameters, StrictQualityModeImprovesWorstThingi10kFixture) {
  const manumesh::Mesh input = loadCaseMesh(
      "external/thingi10k/thingi10k_104188_iphone_tank_case_gen_4_and_4s.stl");
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
  EXPECT_EQ(result.report.rejectedCollapses,
            result.report.featureRejectedCollapses +
                result.report.boundaryRejectedCollapses +
                result.report.topologyRejectedCollapses +
                result.report.normalFlipRejectedCollapses +
                result.report.qualityRejectedCollapses +
                result.report.selfIntersectionRejectedCollapses +
                result.report.curveBudgetRejectedCollapses +
                result.report.errorRejectedCollapses);

  const manumesh::feature::FeatureAnalysis outputFeatures =
      manumesh::feature::detectFeatureCurves(result.mesh, circularFeatureOptions());
  EXPECT_GE(countCircularLoops(outputFeatures), 2);
  EXPECT_LT(maxCircularRelativeError(outputFeatures), 0.10);

  const manumesh::simplification::MeshStats inputStats =
      manumesh::simplification::computeMeshStats(input);
  const manumesh::simplification::MeshStats outputStats =
      manumesh::simplification::computeMeshStats(result.mesh);
  EXPECT_LE(outputStats.nonManifoldEdges, inputStats.nonManifoldEdges);
  EXPECT_GE(outputStats.minTriangleQuality, 0.0);

  const manumesh::simplification::DistanceStats distance =
      manumesh::simplification::compareMeshesBySampledDistance(input, result.mesh, 512);
  EXPECT_LT(distance.maxOriginalToSimplified, 0.20 * input.bboxDiag());
}
