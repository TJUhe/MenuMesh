#include "FeatureDetectionTestSupport.h"
#include "TestSupport.h"
#include "manumesh/core/MeshGenerators.h"

#include <algorithm>
#include <gtest/gtest.h>
#include <vector>

namespace {

namespace feature = manumesh::feature;

using FeatureAnalysis = feature::FeatureAnalysis;
using FeatureLoop = feature::FeatureLoop;
using FeatureOptions = feature::FeatureOptions;
using Mesh = manumesh::Mesh;
using manumesh::test::countCircularLoops;
using manumesh::test::feature_detection::circularLoopsNearRadius;
using manumesh::test::feature_detection::discreteOnlyOptions;
using manumesh::test::feature_detection::hasClosedLoopWithVertices;
using manumesh::test::feature_detection::makeBranchedCircularBoundaryMesh;
using manumesh::test::feature_detection::makeMultiJunctionPolygonalBoundaryMesh;

} // namespace

TEST(FeatureDetection, DetectsCircularCylinderBoundaryLoops) {
  const Mesh mesh = manumesh::generateCylinderGrid(32, 4, 1.0, 2.0);

  FeatureOptions options = discreteOnlyOptions();
  options.featureAngleDeg = 25.0;
  options.circleFitRelativeThreshold = 0.04;
  options.minFeatureLoopVertices = 8;
  const FeatureAnalysis features = feature::detectFeatureCurves(mesh, options);

  EXPECT_GE(countCircularLoops(features), 2);
  const auto loopIt =
      std::find_if(features.loops.begin(), features.loops.end(),
                   [](const FeatureLoop& loop) { return loop.circular; });
  ASSERT_NE(loopIt, features.loops.end());
  EXPECT_NEAR(loopIt->radius, 1.0, 1e-10);
  EXPECT_LT(loopIt->rmsRadialError, 1e-10);
  EXPECT_LT(loopIt->rmsPlaneError, 1e-10);
}

TEST(FeatureDetection, RecoversCircularLoopFromBranchedFeatureGraph) {
  const Mesh mesh = makeBranchedCircularBoundaryMesh();

  FeatureOptions options = discreteOnlyOptions();
  options.circleFitRelativeThreshold = 0.03;
  options.minFeatureLoopVertices = 12;
  const FeatureAnalysis features = feature::detectFeatureCurves(mesh, options);

  const std::vector<FeatureLoop> unitLoops =
      circularLoopsNearRadius(features, 1.0, 1e-8);
  ASSERT_EQ(1u, unitLoops.size());
  EXPECT_EQ(16, static_cast<int>(unitLoops.front().vertices.size()));
  EXPECT_LT(unitLoops.front().rmsRadialError, 1e-10);
  EXPECT_LT(unitLoops.front().rmsPlaneError, 1e-10);
  EXPECT_TRUE(features.vertices[0].circular);
  EXPECT_TRUE(features.vertices[4].circular);
}

TEST(FeatureDetection, RecoversPolygonalCycleThroughMultipleJunctions) {
  const Mesh mesh = makeMultiJunctionPolygonalBoundaryMesh();

  FeatureOptions options = discreteOnlyOptions();
  options.minFeatureLoopVertices = 8;
  options.circleFitRelativeThreshold = 0.005;
  options.ellipseFitRelativeThreshold = 0.005;
  const FeatureAnalysis features = feature::detectFeatureCurves(mesh, options);

  EXPECT_TRUE(hasClosedLoopWithVertices(features, {0, 1, 2, 3, 4, 5, 6, 7}));
  EXPECT_TRUE(features.vertices[0].junction);
  EXPECT_TRUE(features.vertices[2].junction);
  EXPECT_TRUE(features.vertices[5].junction);
}
