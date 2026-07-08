#include "SimplificationTestSupport.h"
#include "algorithms/feature_detection/FeatureDetector.h"
#include "algorithms/simplification/Metrics.h"
#include "algorithms/simplification/PlainSimplifier.h"
#include "algorithms/simplification/QEMSimplifier.h"
#include "core/MeshGenerators.h"
#include "core/MeshTopology.h"
#include "core/PlainMesh.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

using manumesh::test::countCircularLoops;
using manumesh::test::loadExternalMesh;
using manumesh::test::loadExternalStl;
using manumesh::test::SimplifiedMesh;
using manumesh::test::simplifyWithReport;
using namespace manumesh::test::simplification;

namespace simplification = manumesh::simplification;
TEST(ManuMesh, NormalTensorScoresSeparatePlaneFromRidge) {
  const manumesh::Mesh plane = manumesh::generatePlaneGrid(24, 2.0, false);
  const manumesh::Mesh ridge = manumesh::generateRidgeGrid(24, 2.0, 0.5);

  const std::vector<manumesh::feature::NormalTensorVertex> planeTensor =
      manumesh::feature::computeNormalTensorFeatures(plane);
  const std::vector<manumesh::feature::NormalTensorVertex> ridgeTensor =
      manumesh::feature::computeNormalTensorFeatures(ridge);

  double planeMax = 0.0;
  double ridgeMax = 0.0;
  for (const manumesh::feature::NormalTensorVertex& vertex : planeTensor) {
    planeMax = std::max(planeMax, vertex.featureScore);
  }
  for (const manumesh::feature::NormalTensorVertex& vertex : ridgeTensor) {
    ridgeMax = std::max(ridgeMax, vertex.featureScore);
  }

  EXPECT_LT(planeMax, 1e-8);
  EXPECT_GT(ridgeMax, 0.08);
}

TEST(ManuMesh, NormalTensorAddsFeatureEdgesWhenDihedralThresholdIsStrict) {
  const manumesh::Mesh input = manumesh::generateRidgeGrid(32, 2.0, 0.6);
  manumesh::feature::FeatureOptions options;
  options.featureAngleDeg = 179.0;
  options.normalTensorFeatureThreshold = 0.06;
  options.normalTensorMinEdgeAlignment = 0.2;

  const manumesh::feature::FeatureAnalysis features =
      manumesh::feature::detectFeatureCurves(input, options);

  EXPECT_EQ(0, features.dihedralFeatureEdges);
  EXPECT_GT(features.normalTensorFeatureEdges, 0);
  EXPECT_GT(features.maxNormalTensorFeatureScore, 0.06);
}

TEST(ManuMesh, NormalTensorWeightModeAppliesSpatiallyVaryingWeights) {
  const manumesh::Mesh input = manumesh::generateRidgeGrid(32, 2.0, 0.6);
  manumesh::simplification::SimplifyOptions options = paperLineQuadricsOptions(0.70);
  options.weightMode = manumesh::simplification::WeightMode::NormalTensor;
  options.normalTensorSmoothingIterations = 1;

  const SimplifiedMesh result = simplifyWithReport(input, options);

  expectBudgetedSimplification(result, input, 0.70);
  EXPECT_GT(result.report.maxAppliedLineWeight, result.report.minAppliedLineWeight);
}
