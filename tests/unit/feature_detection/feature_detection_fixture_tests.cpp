#include "FeatureDetectionTestSupport.h"
#include "TestSupport.h"

#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <vector>

namespace {

namespace feature = manumesh::feature;

using FeatureAnalysis = feature::FeatureAnalysis;
using FeatureLoop = feature::FeatureLoop;
using FeatureOptions = feature::FeatureOptions;
using FeaturePrimitiveType = feature::FeaturePrimitiveType;
using Mesh = manumesh::Mesh;
using Vec3 = manumesh::Vec3;
using VertexFeature = feature::VertexFeature;
using manumesh::test::loadFixtureMesh;
using manumesh::test::feature_detection::circularLoopsNearRadius;
using manumesh::test::feature_detection::clusterCoplanarFaces;
using manumesh::test::feature_detection::countLoopsOfType;
using manumesh::test::feature_detection::discreteOnlyOptions;
using manumesh::test::feature_detection::hasPlaneCluster;
using manumesh::test::feature_detection::parallelError;
using manumesh::test::feature_detection::PlaneCluster;
using manumesh::test::feature_detection::radialCenterOffsetBetweenLoops;

} // namespace

TEST(FeatureDetection, FixtureDetectsCoaxialHoleLoopsAndPlanarFaces) {
  const Mesh mesh = loadFixtureMesh("feature_fixtures/coaxial_hole_plate.obj");
  ASSERT_FALSE(mesh.empty());

  FeatureOptions options = discreteOnlyOptions();
  options.featureAngleDeg = 25.0;
  options.circleFitRelativeThreshold = 0.03;
  options.minFeatureLoopVertices = 16;
  const FeatureAnalysis features = feature::detectFeatureCurves(mesh, options);

  const std::vector<FeatureLoop> innerHoleLoops =
      circularLoopsNearRadius(features, 0.6, 1e-6);

  ASSERT_EQ(2u, innerHoleLoops.size());
  for (const FeatureLoop& loop : innerHoleLoops) {
    EXPECT_EQ(24, static_cast<int>(loop.vertices.size()));
    EXPECT_NEAR(loop.center.x(), 0.0, 1e-10);
    EXPECT_NEAR(loop.center.y(), 0.0, 1e-10);
    EXPECT_NEAR(std::abs(loop.center.z()), 0.5, 1e-10);
    EXPECT_NEAR(loop.radius, 0.6, 1e-10);
    EXPECT_LT(loop.rmsRadialError, 1e-10);
    EXPECT_LT(loop.rmsPlaneError, 1e-10);
  }

  const Vec3 axisA = innerHoleLoops[0].normal.normalized();
  const Vec3 axisB = innerHoleLoops[1].normal.normalized();
  const double coaxialAngleError = 1.0 - std::abs(axisA.dot(axisB));
  const double radialCenterOffset =
      radialCenterOffsetBetweenLoops(innerHoleLoops[0], innerHoleLoops[1]);
  EXPECT_LT(coaxialAngleError, 1e-12);
  EXPECT_LT(radialCenterOffset, 1e-10);

  const std::vector<PlaneCluster> planes =
      clusterCoplanarFaces(mesh, 1.0 - 1e-12, 1e-10);
  const auto hasLargeHorizontalPlane = [&](double z) {
    return std::any_of(planes.begin(), planes.end(), [&](const PlaneCluster& plane) {
      return std::abs(std::abs(plane.normal.z()) - 1.0) < 1e-12 &&
             std::abs(plane.offset - plane.normal.z() * z) < 1e-10 && plane.area > 10.0;
    });
  };
  EXPECT_TRUE(hasLargeHorizontalPlane(0.5));
  EXPECT_TRUE(hasLargeHorizontalPlane(-0.5));
}

TEST(FeatureDetection, FixtureDetectsTiltedCoaxialHoleAxis) {
  const Mesh mesh = loadFixtureMesh("feature_fixtures/tilted_coaxial_hole_plate.obj");
  ASSERT_FALSE(mesh.empty());

  FeatureOptions options = discreteOnlyOptions();
  options.featureAngleDeg = 25.0;
  options.circleFitRelativeThreshold = 0.03;
  options.minFeatureLoopVertices = 16;
  const FeatureAnalysis features = feature::detectFeatureCurves(mesh, options);

  const std::vector<FeatureLoop> holeLoops =
      circularLoopsNearRadius(features, 0.5, 1e-6);
  ASSERT_EQ(2u, holeLoops.size());

  const Vec3 expectedAxis(0.35, 0.2, 1.0);
  const Vec3 centerDelta = holeLoops[1].center - holeLoops[0].center;
  EXPECT_LT(parallelError(centerDelta, expectedAxis), 1e-12);
  EXPECT_LT(parallelError(holeLoops[0].normal, expectedAxis), 1e-12);
  EXPECT_LT(parallelError(holeLoops[1].normal, expectedAxis), 1e-12);
  EXPECT_LT(radialCenterOffsetBetweenLoops(holeLoops[0], holeLoops[1]), 1e-10);
}

TEST(FeatureDetection, FixtureExposesEccentricHoleNonCoaxiality) {
  const Mesh mesh = loadFixtureMesh("feature_fixtures/eccentric_hole_plate.obj");
  ASSERT_FALSE(mesh.empty());

  FeatureOptions options = discreteOnlyOptions();
  options.featureAngleDeg = 25.0;
  options.circleFitRelativeThreshold = 0.03;
  options.minFeatureLoopVertices = 16;
  const FeatureAnalysis features = feature::detectFeatureCurves(mesh, options);

  const std::vector<FeatureLoop> holeLoops =
      circularLoopsNearRadius(features, 0.55, 1e-6);
  ASSERT_EQ(2u, holeLoops.size());
  EXPECT_NEAR(holeLoops[0].radius, 0.55, 1e-10);
  EXPECT_NEAR(holeLoops[1].radius, 0.55, 1e-10);
  EXPECT_GT(radialCenterOffsetBetweenLoops(holeLoops[0], holeLoops[1]), 0.25);
}

TEST(FeatureDetection, FixtureClassifiesEllipseAndNearCircleLoops) {
  FeatureOptions options = discreteOnlyOptions();
  options.featureAngleDeg = 25.0;
  options.circleFitRelativeThreshold = 0.03;
  options.ellipseFitRelativeThreshold = 0.03;
  options.nearCircleAxisRatioTolerance = 0.08;
  options.minFeatureLoopVertices = 16;

  const Mesh ellipseMesh =
      loadFixtureMesh("feature_fixtures/elliptical_hole_plate.obj");
  ASSERT_FALSE(ellipseMesh.empty());
  const FeatureAnalysis ellipseFeatures =
      feature::detectFeatureCurves(ellipseMesh, options);
  EXPECT_GE(countLoopsOfType(ellipseFeatures, FeaturePrimitiveType::Ellipse), 2);
  const auto ellipseIt =
      std::find_if(ellipseFeatures.loops.begin(), ellipseFeatures.loops.end(),
                   [](const FeatureLoop& loop) {
                     return loop.primitive == FeaturePrimitiveType::Ellipse &&
                            loop.majorRadius < 1.0;
                   });
  ASSERT_NE(ellipseIt, ellipseFeatures.loops.end());
  EXPECT_NEAR(ellipseIt->majorRadius, 0.8, 1e-10);
  EXPECT_NEAR(ellipseIt->minorRadius, 0.45, 1e-10);
  EXPECT_LT(ellipseIt->axisRatio, 0.65);
  EXPECT_LT(ellipseIt->rmsEllipseError, 1e-10);
  ASSERT_FALSE(ellipseIt->vertices.empty());
  const VertexFeature& ellipseVertex =
      ellipseFeatures.vertices[ellipseIt->vertices.front()];
  EXPECT_EQ(FeaturePrimitiveType::Ellipse, ellipseVertex.primitive);
  EXPECT_NEAR(ellipseVertex.ellipseMajorRadius, 0.8, 1e-10);
  EXPECT_NEAR(ellipseVertex.ellipseMinorRadius, 0.45, 1e-10);
  EXPECT_GT(ellipseVertex.tangent.norm(), 0.9);

  const Mesh nearCircleMesh =
      loadFixtureMesh("feature_fixtures/near_circular_hole_plate.obj");
  ASSERT_FALSE(nearCircleMesh.empty());
  const FeatureAnalysis nearCircleFeatures =
      feature::detectFeatureCurves(nearCircleMesh, options);
  const int innerNearCircleLoops = static_cast<int>(
      std::count_if(nearCircleFeatures.loops.begin(), nearCircleFeatures.loops.end(),
                    [](const FeatureLoop& loop) {
                      return loop.primitive == FeaturePrimitiveType::NearCircle &&
                             loop.majorRadius < 1.0;
                    }));
  EXPECT_GE(innerNearCircleLoops, 2);
}

TEST(FeatureDetection, FixtureDetectsBossPocketPlanesAndHardEdges) {
  const Mesh mesh = loadFixtureMesh("feature_fixtures/boss_pocket_plate.obj");
  ASSERT_FALSE(mesh.empty());

  FeatureOptions options = discreteOnlyOptions();
  options.featureAngleDeg = 25.0;
  options.minFeatureLoopVertices = 4;
  const FeatureAnalysis features = feature::detectFeatureCurves(mesh, options);

  EXPECT_EQ(0, features.boundaryFeatureEdges);
  EXPECT_GE(features.dihedralFeatureEdges, 20);
  EXPECT_GT(features.convexFeatureEdges, 0);
  EXPECT_GT(features.concaveFeatureEdges, 0);
  EXPECT_GT(features.loops.size(), 0u);
  EXPECT_TRUE(
      std::any_of(features.loops.begin(), features.loops.end(),
                  [](const FeatureLoop& loop) { return loop.convexEdges > 0; }));
  EXPECT_TRUE(
      std::any_of(features.loops.begin(), features.loops.end(),
                  [](const FeatureLoop& loop) { return loop.concaveEdges > 0; }));

  const std::vector<PlaneCluster> planes =
      clusterCoplanarFaces(mesh, 1.0 - 1e-12, 1e-10);
  EXPECT_TRUE(hasPlaneCluster(planes, Vec3(0.0, 0.0, 1.0), 0.7, 0.35));
  EXPECT_TRUE(hasPlaneCluster(planes, Vec3(0.0, 0.0, 1.0), -0.25, 0.35));
  EXPECT_TRUE(hasPlaneCluster(planes, Vec3(0.0, 0.0, 1.0), 0.2, 2.0));
}
