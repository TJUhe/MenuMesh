#include "FeatureDetectionTestSupport.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace manumesh::test::feature_detection {

int countClosedLoops(const FeatureAnalysis& analysis) {
  return static_cast<int>(
      std::count_if(analysis.loops.begin(), analysis.loops.end(),
                    [](const FeatureLoop& loop) { return loop.closed; }));
}

int countLoopsOfType(const FeatureAnalysis& analysis, FeaturePrimitiveType primitive) {
  return static_cast<int>(std::count_if(
      analysis.loops.begin(), analysis.loops.end(),
      [&](const FeatureLoop& loop) { return loop.primitive == primitive; }));
}

FeatureOptions discreteOnlyOptions() {
  FeatureOptions options;
  options.useNormalTensorFeatures = false;
  return options;
}

std::vector<PlaneCluster> clusterCoplanarFaces(const Mesh& mesh, double normalTolerance,
                                               double offsetTolerance) {
  std::vector<PlaneCluster> clusters;
  for (const Face& face : mesh.faces) {
    const Vec3& a = mesh.vertices[face.v[0]];
    const Vec3& b = mesh.vertices[face.v[1]];
    const Vec3& c = mesh.vertices[face.v[2]];
    Vec3 normal = manumesh::triangleNormal(a, b, c);
    const double area = manumesh::triangleArea(a, b, c);
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

std::vector<FeatureLoop> circularLoopsNearRadius(const FeatureAnalysis& analysis,
                                                 double radius, double tolerance) {
  std::vector<FeatureLoop> result;
  for (const FeatureLoop& loop : analysis.loops) {
    if (loop.circular && std::abs(loop.radius - radius) <= tolerance) {
      result.push_back(loop);
    }
  }
  return result;
}

double radialCenterOffsetBetweenLoops(const FeatureLoop& a, const FeatureLoop& b) {
  Vec3 axis = a.normal;
  if (axis.norm() <= 1e-20) {
    return std::numeric_limits<double>::infinity();
  }
  axis.normalize();
  const Vec3 centerDelta = b.center - a.center;
  return (centerDelta - axis * centerDelta.dot(axis)).norm();
}

double parallelError(const Vec3& a, const Vec3& b) {
  if (a.norm() <= 1e-20 || b.norm() <= 1e-20) {
    return std::numeric_limits<double>::infinity();
  }
  return 1.0 - std::abs(a.normalized().dot(b.normalized()));
}

bool hasPlaneCluster(const std::vector<PlaneCluster>& planes, const Vec3& normal,
                     double offset, double minArea) {
  Vec3 n = normal.normalized();
  return std::any_of(planes.begin(), planes.end(), [&](const PlaneCluster& plane) {
    return plane.normal.dot(n) > 1.0 - 1e-12 &&
           std::abs(plane.offset - offset) < 1e-10 && plane.area > minArea;
  });
}

Mesh makeBranchedCircularBoundaryMesh() {
  constexpr int kSegments = 16;
  const double pi = std::acos(-1.0);
  Mesh mesh;
  for (int i = 0; i < kSegments; ++i) {
    const double angle =
        2.0 * pi * static_cast<double>(i) / static_cast<double>(kSegments);
    mesh.vertices.push_back(Vec3(std::cos(angle), std::sin(angle), 0.0));
  }
  const int center = static_cast<int>(mesh.vertices.size());
  mesh.vertices.push_back(Vec3(0.0, 0.0, 0.0));
  for (int i = 0; i < kSegments; ++i) {
    mesh.faces.push_back({{center, i, (i + 1) % kSegments}});
  }

  const int a = static_cast<int>(mesh.vertices.size());
  mesh.vertices.push_back(Vec3(2.2, 0.6, 0.0));
  const int b = static_cast<int>(mesh.vertices.size());
  mesh.vertices.push_back(Vec3(1.8, 1.4, 0.0));
  mesh.faces.push_back({{0, a, b}});
  mesh.faces.push_back({{0, b, 4}});
  return mesh;
}

Mesh makeMultiJunctionPolygonalBoundaryMesh() {
  Mesh mesh;
  mesh.vertices = {
      Vec3(0.0, 0.0, 0.0),   Vec3(2.0, 0.0, 0.0),   Vec3(3.0, 0.5, 0.0),
      Vec3(3.0, 1.7, 0.0),   Vec3(2.0, 2.2, 0.0),   Vec3(0.5, 2.0, 0.0),
      Vec3(-0.2, 1.2, 0.0),  Vec3(-0.1, 0.4, 0.0),  Vec3(1.2, 1.0, 0.0),
      Vec3(-0.8, -0.3, 0.0), Vec3(-0.3, -0.8, 0.0), Vec3(3.6, 0.2, 0.0),
      Vec3(3.8, 0.9, 0.0),   Vec3(0.0, 2.7, 0.0),   Vec3(0.8, 2.8, 0.0),
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

Mesh makeMixedDiscreteEvidenceMesh() {
  Mesh mesh;
  mesh.vertices = {
      Vec3(0.0, 0.0, 0.0), Vec3(1.0, 0.0, 0.0),  Vec3(0.0, 1.0, 0.0),
      Vec3(0.0, 0.0, 1.0), Vec3(0.0, 0.0, -1.0), Vec3(3.0, 0.0, 0.0),
      Vec3(4.0, 0.0, 0.0), Vec3(3.0, 1.0, 0.0),  Vec3(3.0, 0.0, 1.0),
  };
  mesh.faces = {
      {{0, 1, 2}}, {{1, 0, 3}}, {{0, 1, 4}}, {{5, 6, 7}}, {{6, 5, 8}},
  };
  return mesh;
}

bool hasClosedLoopWithVertices(const FeatureAnalysis& features,
                               const std::vector<int>& expectedVertices) {
  std::vector<int> expected = expectedVertices;
  std::sort(expected.begin(), expected.end());
  return std::any_of(features.loops.begin(), features.loops.end(),
                     [&](const FeatureLoop& loop) {
                       if (!loop.closed || loop.vertices.size() != expected.size()) {
                         return false;
                       }
                       std::vector<int> actual = loop.vertices;
                       std::sort(actual.begin(), actual.end());
                       return actual == expected;
                     });
}

} // namespace manumesh::test::feature_detection
