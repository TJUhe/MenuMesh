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
