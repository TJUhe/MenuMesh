#include "TestSupport.h"
#include "line_quadrics_qem/core/MeshGenerators.h"
#include "line_quadrics_qem/algorithms/feature_detection/FeatureDetector.h"

#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <limits>
#include <vector>

namespace {

struct PlaneCluster {
  lq::Vec3 normal = lq::Vec3::Zero();
  double offset = 0.0;
  double area = 0.0;
};

using lq::test::countCircularLoops;
using lq::test::loadFixtureMesh;

int countClosedLoops(const lq::FeatureAnalysis& analysis) {
  return static_cast<int>(
      std::count_if(analysis.loops.begin(), analysis.loops.end(),
                    [](const lq::FeatureLoop& loop) { return loop.closed; }));
}

int countLoopsOfType(const lq::FeatureAnalysis& analysis,
                     lq::FeaturePrimitiveType primitive) {
  return static_cast<int>(std::count_if(
      analysis.loops.begin(), analysis.loops.end(),
      [&](const lq::FeatureLoop& loop) { return loop.primitive == primitive; }));
}

lq::FeatureOptions discreteOnlyOptions() {
  lq::FeatureOptions options;
  options.useNormalTensorFeatures = false;
  return options;
}

std::vector<PlaneCluster> clusterCoplanarFaces(const lq::Mesh& mesh,
                                               double normalTolerance,
                                               double offsetTolerance) {
  std::vector<PlaneCluster> clusters;
  for (const lq::Face& face : mesh.faces) {
    const lq::Vec3& a = mesh.vertices[face.v[0]];
    const lq::Vec3& b = mesh.vertices[face.v[1]];
    const lq::Vec3& c = mesh.vertices[face.v[2]];
    lq::Vec3 normal = lq::triangleNormal(a, b, c);
    const double area = lq::triangleArea(a, b, c);
    if (area <= 1e-12 || normal.norm() <= 1e-12) {
      continue;
    }
    double offset = normal.dot(a);
    if (normal.z() < 0.0 ||
        (std::abs(normal.z()) <= 1e-12 &&
         (normal.y() < 0.0 || (std::abs(normal.y()) <= 1e-12 && normal.x() < 0.0)))) {
      normal = -normal;
      offset = -offset;
    }

    auto it = std::find_if(
        clusters.begin(), clusters.end(), [&](const PlaneCluster& cluster) {
          return normal.dot(cluster.normal) >= normalTolerance &&
                 std::abs(offset - cluster.offset) <= offsetTolerance;
        });
    if (it == clusters.end()) {
      clusters.push_back(PlaneCluster{normal, offset, area});
    } else {
      const double oldArea = it->area;
      it->area += area;
      it->normal = (it->normal + area * normal).normalized();
      it->offset = (oldArea * it->offset + area * offset) / it->area;
    }
  }
  return clusters;
}

std::vector<lq::FeatureLoop>
circularLoopsNearRadius(const lq::FeatureAnalysis& analysis, double radius,
                        double tolerance) {
  std::vector<lq::FeatureLoop> result;
  for (const lq::FeatureLoop& loop : analysis.loops) {
    if (loop.circular && std::abs(loop.radius - radius) <= tolerance) {
      result.push_back(loop);
    }
  }
  return result;
}

double radialCenterOffsetBetweenLoops(const lq::FeatureLoop& a,
                                      const lq::FeatureLoop& b) {
  lq::Vec3 axis = a.normal;
  if (axis.norm() <= 1e-20) {
    return std::numeric_limits<double>::infinity();
  }
  axis.normalize();
  const lq::Vec3 centerDelta = b.center - a.center;
  return (centerDelta - axis * centerDelta.dot(axis)).norm();
}

double parallelError(const lq::Vec3& a, const lq::Vec3& b) {
  if (a.norm() <= 1e-20 || b.norm() <= 1e-20) {
    return std::numeric_limits<double>::infinity();
  }
  return 1.0 - std::abs(a.normalized().dot(b.normalized()));
}

bool hasPlaneCluster(const std::vector<PlaneCluster>& planes, const lq::Vec3& normal,
                     double offset, double minArea) {
  lq::Vec3 n = normal.normalized();
  return std::any_of(planes.begin(), planes.end(), [&](const PlaneCluster& plane) {
    return plane.normal.dot(n) > 1.0 - 1e-12 &&
           std::abs(plane.offset - offset) < 1e-10 && plane.area > minArea;
  });
}

lq::Mesh makeBranchedCircularBoundaryMesh() {
  constexpr int kSegments = 16;
  constexpr double kPi = 3.141592653589793238462643383279502884;
  lq::Mesh mesh;
  for (int i = 0; i < kSegments; ++i) {
    const double angle =
        2.0 * kPi * static_cast<double>(i) / static_cast<double>(kSegments);
    mesh.vertices.push_back(lq::Vec3(std::cos(angle), std::sin(angle), 0.0));
  }
  const int center = static_cast<int>(mesh.vertices.size());
  mesh.vertices.push_back(lq::Vec3(0.0, 0.0, 0.0));
  for (int i = 0; i < kSegments; ++i) {
    mesh.faces.push_back({{center, i, (i + 1) % kSegments}});
  }

  const int a = static_cast<int>(mesh.vertices.size());
  mesh.vertices.push_back(lq::Vec3(2.2, 0.6, 0.0));
  const int b = static_cast<int>(mesh.vertices.size());
  mesh.vertices.push_back(lq::Vec3(1.8, 1.4, 0.0));
  mesh.faces.push_back({{0, a, b}});
  mesh.faces.push_back({{0, b, 4}});
  return mesh;
}

lq::Mesh makeMultiJunctionPolygonalBoundaryMesh() {
  lq::Mesh mesh;
  mesh.vertices = {
      lq::Vec3(0.0, 0.0, 0.0),   lq::Vec3(2.0, 0.0, 0.0),   lq::Vec3(3.0, 0.5, 0.0),
      lq::Vec3(3.0, 1.7, 0.0),   lq::Vec3(2.0, 2.2, 0.0),   lq::Vec3(0.5, 2.0, 0.0),
      lq::Vec3(-0.2, 1.2, 0.0),  lq::Vec3(-0.1, 0.4, 0.0),  lq::Vec3(1.2, 1.0, 0.0),
      lq::Vec3(-0.8, -0.3, 0.0), lq::Vec3(-0.3, -0.8, 0.0), lq::Vec3(3.6, 0.2, 0.0),
      lq::Vec3(3.8, 0.9, 0.0),   lq::Vec3(0.0, 2.7, 0.0),   lq::Vec3(0.8, 2.8, 0.0),
  };
  constexpr int center = 8;
  for (int i = 0; i < 8; ++i) {
    mesh.faces.push_back({{center, i, (i + 1) % 8}});
  }
  mesh.faces.push_back({{0, 9, 10}});
  mesh.faces.push_back({{2, 11, 12}});
  mesh.faces.push_back({{5, 13, 14}});
  return mesh;
}

bool hasClosedLoopWithVertices(const lq::FeatureAnalysis& features,
                               const std::vector<int>& expectedVertices) {
  std::vector<int> expected = expectedVertices;
  std::sort(expected.begin(), expected.end());
  return std::any_of(features.loops.begin(), features.loops.end(),
                     [&](const lq::FeatureLoop& loop) {
                       if (!loop.closed || loop.vertices.size() != expected.size()) {
                         return false;
                       }
                       std::vector<int> actual = loop.vertices;
                       std::sort(actual.begin(), actual.end());
                       return actual == expected;
                     });
}

} // namespace

TEST(FeatureDetection, ClassifiesBoundaryEdgesOnOpenTriangle) {
  lq::Mesh mesh;
  mesh.vertices = {
      lq::Vec3(0.0, 0.0, 0.0),
      lq::Vec3(1.0, 0.0, 0.0),
      lq::Vec3(0.0, 1.0, 0.0),
  };
  mesh.faces = {{{0, 1, 2}}};

  const lq::FeatureAnalysis features =
      lq::detectFeatureCurves(mesh, discreteOnlyOptions());

  EXPECT_EQ(3, features.featureEdges);
  EXPECT_EQ(3, features.boundaryFeatureEdges);
  EXPECT_EQ(0, features.dihedralFeatureEdges);
  EXPECT_EQ(0, features.nonManifoldFeatureEdges);
  EXPECT_EQ(1, countClosedLoops(features));
  ASSERT_EQ(mesh.vertices.size(), features.vertices.size());
  for (const lq::VertexFeature& vertex : features.vertices) {
    EXPECT_TRUE(vertex.isFeature);
  }
}

TEST(FeatureDetection, FeatureDetectorObjectStoresOptions) {
  lq::FeatureOptions options = discreteOnlyOptions();
  options.minFeatureLoopVertices = 3;
  lq::FeatureDetector detector(options);
  EXPECT_EQ(3, detector.options().minFeatureLoopVertices);

  lq::Mesh mesh;
  mesh.vertices = {
      lq::Vec3(0.0, 0.0, 0.0),
      lq::Vec3(1.0, 0.0, 0.0),
      lq::Vec3(0.0, 1.0, 0.0),
  };
  mesh.faces = {{{0, 1, 2}}};
  const lq::FeatureAnalysis first = detector.analyze(mesh);
  EXPECT_EQ(3, first.featureEdges);

  options.minFeatureLoopVertices = 100;
  detector.setOptions(options);
  EXPECT_EQ(100, detector.options().minFeatureLoopVertices);
  const lq::FeatureAnalysis second = detector.analyze(mesh);
  const lq::FeatureAnalysis direct = lq::detectFeatureCurves(mesh, options);
  EXPECT_EQ(first.featureEdges, second.featureEdges);
  EXPECT_EQ(direct.loops.size(), second.loops.size());
  EXPECT_EQ(direct.featureEdges, second.featureEdges);
}

TEST(FeatureDetection, ClassifiesNonManifoldEdgesSeparately) {
  lq::Mesh mesh;
  mesh.vertices = {
      lq::Vec3(0.0, 0.0, 0.0), lq::Vec3(1.0, 0.0, 0.0),  lq::Vec3(0.0, 1.0, 0.0),
      lq::Vec3(0.0, 0.0, 1.0), lq::Vec3(0.0, 0.0, -1.0),
  };
  mesh.faces = {
      {{0, 1, 2}},
      {{1, 0, 3}},
      {{0, 1, 4}},
  };

  const lq::FeatureAnalysis features =
      lq::detectFeatureCurves(mesh, discreteOnlyOptions());

  EXPECT_EQ(1, features.nonManifoldFeatureEdges);
  EXPECT_GT(features.boundaryFeatureEdges, 0);
  EXPECT_GE(features.featureEdges, features.nonManifoldFeatureEdges);
  EXPECT_TRUE(features.vertices[0].isFeature);
  EXPECT_TRUE(features.vertices[1].isFeature);
}

TEST(FeatureDetection, DihedralThresholdControlsCreaseEdges) {
  lq::Mesh mesh;
  mesh.vertices = {
      lq::Vec3(0.0, 0.0, 0.0),
      lq::Vec3(1.0, 0.0, 0.0),
      lq::Vec3(0.0, 1.0, 0.0),
      lq::Vec3(0.0, 0.0, 1.0),
  };
  mesh.faces = {
      {{0, 1, 2}},
      {{1, 0, 3}},
  };

  lq::FeatureOptions strict = discreteOnlyOptions();
  strict.featureAngleDeg = 120.0;
  const lq::FeatureAnalysis strictFeatures = lq::detectFeatureCurves(mesh, strict);

  lq::FeatureOptions permissive = discreteOnlyOptions();
  permissive.featureAngleDeg = 30.0;
  const lq::FeatureAnalysis permissiveFeatures =
      lq::detectFeatureCurves(mesh, permissive);

  EXPECT_EQ(0, strictFeatures.dihedralFeatureEdges);
  EXPECT_EQ(1, permissiveFeatures.dihedralFeatureEdges);
  EXPECT_GT(permissiveFeatures.featureEdges, strictFeatures.featureEdges);
}

TEST(FeatureDetection, DetectsCircularCylinderBoundaryLoops) {
  const lq::Mesh mesh = lq::generateCylinderGrid(32, 4, 1.0, 2.0);

  lq::FeatureOptions options = discreteOnlyOptions();
  options.featureAngleDeg = 25.0;
  options.circleFitRelativeThreshold = 0.04;
  options.minFeatureLoopVertices = 8;
  const lq::FeatureAnalysis features = lq::detectFeatureCurves(mesh, options);

  EXPECT_GE(countCircularLoops(features), 2);
  const auto loopIt =
      std::find_if(features.loops.begin(), features.loops.end(),
                   [](const lq::FeatureLoop& loop) { return loop.circular; });
  ASSERT_NE(loopIt, features.loops.end());
  EXPECT_NEAR(loopIt->radius, 1.0, 1e-10);
  EXPECT_LT(loopIt->rmsRadialError, 1e-10);
  EXPECT_LT(loopIt->rmsPlaneError, 1e-10);
}

TEST(FeatureDetection, RecoversCircularLoopFromBranchedFeatureGraph) {
  const lq::Mesh mesh = makeBranchedCircularBoundaryMesh();

  lq::FeatureOptions options = discreteOnlyOptions();
  options.circleFitRelativeThreshold = 0.03;
  options.minFeatureLoopVertices = 12;
  const lq::FeatureAnalysis features = lq::detectFeatureCurves(mesh, options);

  const std::vector<lq::FeatureLoop> unitLoops =
      circularLoopsNearRadius(features, 1.0, 1e-8);
  ASSERT_EQ(1u, unitLoops.size());
  EXPECT_EQ(16, static_cast<int>(unitLoops.front().vertices.size()));
  EXPECT_LT(unitLoops.front().rmsRadialError, 1e-10);
  EXPECT_LT(unitLoops.front().rmsPlaneError, 1e-10);
  EXPECT_TRUE(features.vertices[0].circular);
  EXPECT_TRUE(features.vertices[4].circular);
}

TEST(FeatureDetection, RecoversPolygonalCycleThroughMultipleJunctions) {
  const lq::Mesh mesh = makeMultiJunctionPolygonalBoundaryMesh();

  lq::FeatureOptions options = discreteOnlyOptions();
  options.minFeatureLoopVertices = 8;
  options.circleFitRelativeThreshold = 0.005;
  options.ellipseFitRelativeThreshold = 0.005;
  const lq::FeatureAnalysis features = lq::detectFeatureCurves(mesh, options);

  EXPECT_TRUE(hasClosedLoopWithVertices(features, {0, 1, 2, 3, 4, 5, 6, 7}));
  EXPECT_TRUE(features.vertices[0].junction);
  EXPECT_TRUE(features.vertices[2].junction);
  EXPECT_TRUE(features.vertices[5].junction);
}

TEST(FeatureDetection, FixtureDetectsCoaxialHoleLoopsAndPlanarFaces) {
  const lq::Mesh mesh = loadFixtureMesh("feature_fixtures/coaxial_hole_plate.obj");
  ASSERT_FALSE(mesh.empty());

  lq::FeatureOptions options = discreteOnlyOptions();
  options.featureAngleDeg = 25.0;
  options.circleFitRelativeThreshold = 0.03;
  options.minFeatureLoopVertices = 16;
  const lq::FeatureAnalysis features = lq::detectFeatureCurves(mesh, options);

  const std::vector<lq::FeatureLoop> innerHoleLoops =
      circularLoopsNearRadius(features, 0.6, 1e-6);

  ASSERT_EQ(2u, innerHoleLoops.size());
  for (const lq::FeatureLoop& loop : innerHoleLoops) {
    EXPECT_EQ(24, static_cast<int>(loop.vertices.size()));
    EXPECT_NEAR(loop.center.x(), 0.0, 1e-10);
    EXPECT_NEAR(loop.center.y(), 0.0, 1e-10);
    EXPECT_NEAR(std::abs(loop.center.z()), 0.5, 1e-10);
    EXPECT_NEAR(loop.radius, 0.6, 1e-10);
    EXPECT_LT(loop.rmsRadialError, 1e-10);
    EXPECT_LT(loop.rmsPlaneError, 1e-10);
  }

  const lq::Vec3 axisA = innerHoleLoops[0].normal.normalized();
  const lq::Vec3 axisB = innerHoleLoops[1].normal.normalized();
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
  const lq::Mesh mesh =
      loadFixtureMesh("feature_fixtures/tilted_coaxial_hole_plate.obj");
  ASSERT_FALSE(mesh.empty());

  lq::FeatureOptions options = discreteOnlyOptions();
  options.featureAngleDeg = 25.0;
  options.circleFitRelativeThreshold = 0.03;
  options.minFeatureLoopVertices = 16;
  const lq::FeatureAnalysis features = lq::detectFeatureCurves(mesh, options);

  const std::vector<lq::FeatureLoop> holeLoops =
      circularLoopsNearRadius(features, 0.5, 1e-6);
  ASSERT_EQ(2u, holeLoops.size());

  const lq::Vec3 expectedAxis(0.35, 0.2, 1.0);
  const lq::Vec3 centerDelta = holeLoops[1].center - holeLoops[0].center;
  EXPECT_LT(parallelError(centerDelta, expectedAxis), 1e-12);
  EXPECT_LT(parallelError(holeLoops[0].normal, expectedAxis), 1e-12);
  EXPECT_LT(parallelError(holeLoops[1].normal, expectedAxis), 1e-12);
  EXPECT_LT(radialCenterOffsetBetweenLoops(holeLoops[0], holeLoops[1]), 1e-10);
}

TEST(FeatureDetection, FixtureExposesEccentricHoleNonCoaxiality) {
  const lq::Mesh mesh = loadFixtureMesh("feature_fixtures/eccentric_hole_plate.obj");
  ASSERT_FALSE(mesh.empty());

  lq::FeatureOptions options = discreteOnlyOptions();
  options.featureAngleDeg = 25.0;
  options.circleFitRelativeThreshold = 0.03;
  options.minFeatureLoopVertices = 16;
  const lq::FeatureAnalysis features = lq::detectFeatureCurves(mesh, options);

  const std::vector<lq::FeatureLoop> holeLoops =
      circularLoopsNearRadius(features, 0.55, 1e-6);
  ASSERT_EQ(2u, holeLoops.size());
  EXPECT_NEAR(holeLoops[0].radius, 0.55, 1e-10);
  EXPECT_NEAR(holeLoops[1].radius, 0.55, 1e-10);
  EXPECT_GT(radialCenterOffsetBetweenLoops(holeLoops[0], holeLoops[1]), 0.25);
}

TEST(FeatureDetection, FixtureClassifiesEllipseAndNearCircleLoops) {
  lq::FeatureOptions options = discreteOnlyOptions();
  options.featureAngleDeg = 25.0;
  options.circleFitRelativeThreshold = 0.03;
  options.ellipseFitRelativeThreshold = 0.03;
  options.nearCircleAxisRatioTolerance = 0.08;
  options.minFeatureLoopVertices = 16;

  const lq::Mesh ellipseMesh =
      loadFixtureMesh("feature_fixtures/elliptical_hole_plate.obj");
  ASSERT_FALSE(ellipseMesh.empty());
  const lq::FeatureAnalysis ellipseFeatures =
      lq::detectFeatureCurves(ellipseMesh, options);
  EXPECT_GE(countLoopsOfType(ellipseFeatures, lq::FeaturePrimitiveType::Ellipse), 2);
  const auto ellipseIt =
      std::find_if(ellipseFeatures.loops.begin(), ellipseFeatures.loops.end(),
                   [](const lq::FeatureLoop& loop) {
                     return loop.primitive == lq::FeaturePrimitiveType::Ellipse &&
                            loop.majorRadius < 1.0;
                   });
  ASSERT_NE(ellipseIt, ellipseFeatures.loops.end());
  EXPECT_NEAR(ellipseIt->majorRadius, 0.8, 1e-10);
  EXPECT_NEAR(ellipseIt->minorRadius, 0.45, 1e-10);
  EXPECT_LT(ellipseIt->axisRatio, 0.65);
  EXPECT_LT(ellipseIt->rmsEllipseError, 1e-10);
  ASSERT_FALSE(ellipseIt->vertices.empty());
  const lq::VertexFeature& ellipseVertex =
      ellipseFeatures.vertices[ellipseIt->vertices.front()];
  EXPECT_EQ(lq::FeaturePrimitiveType::Ellipse, ellipseVertex.primitive);
  EXPECT_NEAR(ellipseVertex.ellipseMajorRadius, 0.8, 1e-10);
  EXPECT_NEAR(ellipseVertex.ellipseMinorRadius, 0.45, 1e-10);
  EXPECT_GT(ellipseVertex.tangent.norm(), 0.9);

  const lq::Mesh nearCircleMesh =
      loadFixtureMesh("feature_fixtures/near_circular_hole_plate.obj");
  ASSERT_FALSE(nearCircleMesh.empty());
  const lq::FeatureAnalysis nearCircleFeatures =
      lq::detectFeatureCurves(nearCircleMesh, options);
  const int innerNearCircleLoops = static_cast<int>(
      std::count_if(nearCircleFeatures.loops.begin(), nearCircleFeatures.loops.end(),
                    [](const lq::FeatureLoop& loop) {
                      return loop.primitive == lq::FeaturePrimitiveType::NearCircle &&
                             loop.majorRadius < 1.0;
                    }));
  EXPECT_GE(innerNearCircleLoops, 2);
}

TEST(FeatureDetection, FixtureDetectsBossPocketPlanesAndHardEdges) {
  const lq::Mesh mesh = loadFixtureMesh("feature_fixtures/boss_pocket_plate.obj");
  ASSERT_FALSE(mesh.empty());

  lq::FeatureOptions options = discreteOnlyOptions();
  options.featureAngleDeg = 25.0;
  options.minFeatureLoopVertices = 4;
  const lq::FeatureAnalysis features = lq::detectFeatureCurves(mesh, options);

  EXPECT_EQ(0, features.boundaryFeatureEdges);
  EXPECT_GE(features.dihedralFeatureEdges, 20);
  EXPECT_GT(features.convexFeatureEdges, 0);
  EXPECT_GT(features.concaveFeatureEdges, 0);
  EXPECT_GT(features.loops.size(), 0u);
  EXPECT_TRUE(
      std::any_of(features.loops.begin(), features.loops.end(),
                  [](const lq::FeatureLoop& loop) { return loop.convexEdges > 0; }));
  EXPECT_TRUE(
      std::any_of(features.loops.begin(), features.loops.end(),
                  [](const lq::FeatureLoop& loop) { return loop.concaveEdges > 0; }));

  const std::vector<PlaneCluster> planes =
      clusterCoplanarFaces(mesh, 1.0 - 1e-12, 1e-10);
  EXPECT_TRUE(hasPlaneCluster(planes, lq::Vec3(0.0, 0.0, 1.0), 0.7, 0.35));
  EXPECT_TRUE(hasPlaneCluster(planes, lq::Vec3(0.0, 0.0, 1.0), -0.25, 0.35));
  EXPECT_TRUE(hasPlaneCluster(planes, lq::Vec3(0.0, 0.0, 1.0), 0.2, 2.0));
}

TEST(FeatureDetection, SplitsBranchedFeatureGraphAndMarksJunctions) {
  lq::Mesh mesh;
  mesh.vertices = {
      lq::Vec3(0.0, 0.0, 0.0),  lq::Vec3(1.0, 0.0, 0.0),  lq::Vec3(0.5, 1.0, 0.0),
      lq::Vec3(-1.0, 0.0, 0.0), lq::Vec3(-0.5, 1.0, 0.0), lq::Vec3(0.0, -1.0, 0.0),
      lq::Vec3(1.0, -1.0, 0.0),
  };
  mesh.faces = {
      {{0, 1, 2}},
      {{0, 3, 4}},
      {{0, 5, 6}},
  };

  const lq::FeatureAnalysis features =
      lq::detectFeatureCurves(mesh, discreteOnlyOptions());

  EXPECT_GT(features.loops.size(), 1u);
  EXPECT_EQ(features.featureEdges, static_cast<int>(features.graph.edges.size()));
  EXPECT_EQ(mesh.vertices.size(), features.graph.vertices.size());
  EXPECT_FALSE(features.graph.junctionVertices.empty());
  ASSERT_LT(0u, features.vertices.size());
  EXPECT_TRUE(features.vertices[0].junction);
  EXPECT_TRUE(features.graph.vertices[0].junction);
}
