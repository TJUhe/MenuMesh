#include "line_quadrics_qem/core/MeshGenerators.h"
#include "line_quadrics_qem/features/FeatureDetection.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <gtest/gtest.h>
#include <limits>
#include <string>
#include <vector>

namespace {

struct PlaneCluster {
  lq::Vec3 normal = lq::Vec3::Zero();
  double offset = 0.0;
  double area = 0.0;
};

int countCircularLoops(const lq::FeatureAnalysis& analysis) {
  return static_cast<int>(
      std::count_if(analysis.loops.begin(), analysis.loops.end(),
                    [](const lq::FeatureLoop& loop) { return loop.circular; }));
}

int countClosedLoops(const lq::FeatureAnalysis& analysis) {
  return static_cast<int>(
      std::count_if(analysis.loops.begin(), analysis.loops.end(),
                    [](const lq::FeatureLoop& loop) { return loop.closed; }));
}

int countLoopsOfType(const lq::FeatureAnalysis& analysis,
                     lq::FeaturePrimitiveType primitive) {
  return static_cast<int>(
      std::count_if(analysis.loops.begin(), analysis.loops.end(),
                    [&](const lq::FeatureLoop& loop) {
                      return loop.primitive == primitive;
                    }));
}

lq::FeatureOptions discreteOnlyOptions() {
  lq::FeatureOptions options;
  options.useNormalTensorFeatures = false;
  return options;
}

std::filesystem::path dataRoot() {
#ifdef LQ_TEST_DATA_DIR
  return std::filesystem::path(LQ_TEST_DATA_DIR);
#else
  return std::filesystem::path(__FILE__).parent_path() / "data";
#endif
}

lq::Mesh loadFixtureMesh(const std::string& relativePath) {
  lq::Mesh mesh;
  std::string error;
  const std::filesystem::path path = dataRoot() / relativePath;
  if (!lq::loadMesh(path.string(), mesh, &error)) {
    ADD_FAILURE() << "Failed to load fixture " << path.string() << ": " << error;
  }
  return mesh;
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
         (normal.y() < 0.0 ||
          (std::abs(normal.y()) <= 1e-12 && normal.x() < 0.0)))) {
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

std::vector<lq::FeatureLoop> circularLoopsNearRadius(
    const lq::FeatureAnalysis& analysis, double radius, double tolerance) {
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

  const lq::Mesh nearCircleMesh =
      loadFixtureMesh("feature_fixtures/near_circular_hole_plate.obj");
  ASSERT_FALSE(nearCircleMesh.empty());
  const lq::FeatureAnalysis nearCircleFeatures =
      lq::detectFeatureCurves(nearCircleMesh, options);
  const int innerNearCircleLoops = static_cast<int>(std::count_if(
      nearCircleFeatures.loops.begin(), nearCircleFeatures.loops.end(),
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
  EXPECT_TRUE(std::any_of(features.loops.begin(), features.loops.end(),
                          [](const lq::FeatureLoop& loop) {
                            return loop.convexEdges > 0;
                          }));
  EXPECT_TRUE(std::any_of(features.loops.begin(), features.loops.end(),
                          [](const lq::FeatureLoop& loop) {
                            return loop.concaveEdges > 0;
                          }));

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
  ASSERT_LT(0u, features.vertices.size());
  EXPECT_TRUE(features.vertices[0].junction);
}
