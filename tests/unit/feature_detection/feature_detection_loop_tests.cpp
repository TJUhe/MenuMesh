#include "FeatureDetectionTestSupport.h"
#include "TestSupport.h"
#include "core/MeshGenerators.h"

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
using manumesh::test::feature_detection::makeFragmentedCircleWithTensorRidgeMesh;
using manumesh::test::feature_detection::makeMultiJunctionPolygonalBoundaryMesh;
using manumesh::test::feature_detection::makeShallowDihedralLoopMesh;

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

TEST(FeatureDetection, TracesShallowDihedralLoopAtRequestedAngle) {
  const Mesh mesh = makeShallowDihedralLoopMesh(0.27);

  FeatureOptions options = discreteOnlyOptions();
  options.featureAngleDeg = 20.0;
  options.minFeatureLoopVertices = 4;
  const FeatureAnalysis features = feature::detectFeatureCurves(mesh, options);

  EXPECT_GT(features.dihedralFeatureEdges, 0);
  EXPECT_EQ(features.featureEdges, features.tracedFeatureEdges);
  EXPECT_EQ(0, features.untracedFeatureEdges);
  EXPECT_FALSE(features.loops.empty());
  for (int id : {2, 3, 4, 5}) {
    ASSERT_LT(id, static_cast<int>(features.vertices.size()));
    EXPECT_TRUE(features.vertices[id].isFeature);
  }
}

TEST(FeatureDetection, ReportsUntracedDihedralEdgesWhenTraceAngleIsStricter) {
  const Mesh mesh = makeShallowDihedralLoopMesh(0.27);

  FeatureOptions options = discreteOnlyOptions();
  options.featureAngleDeg = 20.0;
  options.loopTraceAngleDeg = 179.0;
  options.minFeatureLoopVertices = 4;
  const FeatureAnalysis features = feature::detectFeatureCurves(mesh, options);

  EXPECT_GT(features.dihedralFeatureEdges, 0);
  EXPECT_EQ(features.featureEdges, features.untracedFeatureEdges);
  EXPECT_EQ(0, features.tracedFeatureEdges);
  EXPECT_TRUE(features.loops.empty());
  for (int id : {2, 3, 4, 5}) {
    ASSERT_LT(id, static_cast<int>(features.vertices.size()));
    EXPECT_FALSE(features.vertices[id].isFeature);
  }
}

TEST(FeatureDetection, TensorComponentDoesNotBlockSeparateCircularFallback) {
  const Mesh mesh = makeFragmentedCircleWithTensorRidgeMesh();

  FeatureOptions options;
  options.featureAngleDeg = 179.0;
  options.normalTensorFeatureThreshold = 0.06;
  options.normalTensorMinEdgeAlignment = 0.2;
  options.normalTensorSmoothingIterations = 1;
  options.minFeatureLoopVertices = 12;
  options.circleFitRelativeThreshold = 0.04;
  const FeatureAnalysis features = feature::detectFeatureCurves(mesh, options);

  EXPECT_GT(features.normalTensorFeatureEdges, 0);
  EXPECT_GE(countCircularLoops(features), 1);
  for (int i = 0; i < static_cast<int>(features.loops.size()); ++i) {
    EXPECT_EQ(i, features.loops[i].id);
  }
}
