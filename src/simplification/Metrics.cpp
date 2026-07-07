#include "manumesh/algorithms/simplification/Metrics.h"

#include "manumesh/core/MeshTopology.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <limits>
#include <numeric>
#include <sstream>
#include <vector>

namespace manumesh::simplification {
namespace {

double triangleQuality(const Vec3& a, const Vec3& b, const Vec3& c) {
  const double l0 = (b - a).squaredNorm();
  const double l1 = (c - b).squaredNorm();
  const double l2 = (a - c).squaredNorm();
  const double denom = l0 + l1 + l2;
  if (denom <= 1e-30) {
    return 0.0;
  }
  return 4.0 * std::sqrt(3.0) * triangleArea(a, b, c) / denom;
}

double pointTriangleDistanceSquared(const Vec3& p, const Vec3& a, const Vec3& b,
                                    const Vec3& c) {
  const Vec3 ab = b - a;
  const Vec3 ac = c - a;
  const Vec3 ap = p - a;
  const double d1 = ab.dot(ap);
  const double d2 = ac.dot(ap);
  if (d1 <= 0.0 && d2 <= 0.0) return (p - a).squaredNorm();

  const Vec3 bp = p - b;
  const double d3 = ab.dot(bp);
  const double d4 = ac.dot(bp);
  if (d3 >= 0.0 && d4 <= d3) return (p - b).squaredNorm();

  const double vc = d1 * d4 - d3 * d2;
  if (vc <= 0.0 && d1 >= 0.0 && d3 <= 0.0) {
    const double v = d1 / (d1 - d3);
    return (p - (a + v * ab)).squaredNorm();
  }

  const Vec3 cp = p - c;
  const double d5 = ab.dot(cp);
  const double d6 = ac.dot(cp);
  if (d6 >= 0.0 && d5 <= d6) return (p - c).squaredNorm();

  const double vb = d5 * d2 - d1 * d6;
  if (vb <= 0.0 && d2 >= 0.0 && d6 <= 0.0) {
    const double w = d2 / (d2 - d6);
    return (p - (a + w * ac)).squaredNorm();
  }

  const double va = d3 * d6 - d5 * d4;
  if (va <= 0.0 && (d4 - d3) >= 0.0 && (d5 - d6) >= 0.0) {
    const double w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
    return (p - (b + w * (c - b))).squaredNorm();
  }

  const Vec3 n = ab.cross(ac);
  const double nn = n.squaredNorm();
  if (nn <= 1e-30) {
    return std::min(
        {(p - a).squaredNorm(), (p - b).squaredNorm(), (p - c).squaredNorm()});
  }
  const double distance = n.dot(ap);
  return distance * distance / nn;
}

double pointAabbDistanceSquared(const Vec3& p, const Vec3& lo, const Vec3& hi) {
  double d2 = 0.0;
  for (int axis = 0; axis < 3; ++axis) {
    const double value = p[axis];
    if (value < lo[axis]) {
      const double d = lo[axis] - value;
      d2 += d * d;
    } else if (value > hi[axis]) {
      const double d = value - hi[axis];
      d2 += d * d;
    }
  }
  return d2;
}

struct TriangleRef {
  int face = -1;
  Vec3 lo = Vec3::Zero();
  Vec3 hi = Vec3::Zero();
  Vec3 centroid = Vec3::Zero();
  double area = 0.0;
};

struct BvhNode {
  Vec3 lo = Vec3::Zero();
  Vec3 hi = Vec3::Zero();
  int left = -1;
  int right = -1;
  int begin = 0;
  int end = 0;
};

class MeshDistanceIndex {
public:
  explicit MeshDistanceIndex(const Mesh& mesh) : mesh_(mesh) {
    triangles_.reserve(mesh.faces.size());
    for (int fi = 0; fi < static_cast<int>(mesh.faces.size()); ++fi) {
      const Face& face = mesh.faces[fi];
      const Vec3& a = mesh.vertices[face.v[0]];
      const Vec3& b = mesh.vertices[face.v[1]];
      const Vec3& c = mesh.vertices[face.v[2]];
      const double area = triangleArea(a, b, c);
      if (area <= 1e-24) {
        continue;
      }
      TriangleRef ref;
      ref.face = fi;
      ref.lo = a.cwiseMin(b).cwiseMin(c);
      ref.hi = a.cwiseMax(b).cwiseMax(c);
      ref.centroid = (a + b + c) / 3.0;
      ref.area = area;
      order_.push_back(static_cast<int>(triangles_.size()));
      triangles_.push_back(ref);
    }
    if (!order_.empty()) {
      buildRecursive(0, static_cast<int>(order_.size()));
    }
  }

  bool empty() const { return nodes_.empty(); }

  double distanceSquared(const Vec3& point) const {
    if (nodes_.empty()) {
      return std::numeric_limits<double>::infinity();
    }
    double best = std::numeric_limits<double>::infinity();
    queryRecursive(0, point, best);
    return best;
  }

private:
  int buildRecursive(int begin, int end) {
    BvhNode node;
    node.begin = begin;
    node.end = end;
    node.lo = Vec3(std::numeric_limits<double>::infinity(),
                   std::numeric_limits<double>::infinity(),
                   std::numeric_limits<double>::infinity());
    node.hi = Vec3(-std::numeric_limits<double>::infinity(),
                   -std::numeric_limits<double>::infinity(),
                   -std::numeric_limits<double>::infinity());
    for (int i = begin; i < end; ++i) {
      const TriangleRef& tri = triangles_[order_[i]];
      node.lo = node.lo.cwiseMin(tri.lo);
      node.hi = node.hi.cwiseMax(tri.hi);
    }

    const int nodeId = static_cast<int>(nodes_.size());
    nodes_.push_back(node);
    if (end - begin <= 8) {
      return nodeId;
    }

    const Vec3 extent = node.hi - node.lo;
    int axis = 0;
    if (extent.y() > extent.x() && extent.y() >= extent.z()) {
      axis = 1;
    } else if (extent.z() > extent.x() && extent.z() > extent.y()) {
      axis = 2;
    }
    const int mid = begin + (end - begin) / 2;
    std::nth_element(order_.begin() + begin, order_.begin() + mid, order_.begin() + end,
                     [&](int lhs, int rhs) {
                       return triangles_[lhs].centroid[axis] <
                              triangles_[rhs].centroid[axis];
                     });
    nodes_[nodeId].left = buildRecursive(begin, mid);
    nodes_[nodeId].right = buildRecursive(mid, end);
    return nodeId;
  }

  void queryRecursive(int nodeId, const Vec3& point, double& best) const {
    const BvhNode& node = nodes_[nodeId];
    if (pointAabbDistanceSquared(point, node.lo, node.hi) >= best) {
      return;
    }
    if (node.left < 0 && node.right < 0) {
      for (int i = node.begin; i < node.end; ++i) {
        const Face& face = mesh_.faces[triangles_[order_[i]].face];
        best = std::min(best,
                        pointTriangleDistanceSquared(point, mesh_.vertices[face.v[0]],
                                                     mesh_.vertices[face.v[1]],
                                                     mesh_.vertices[face.v[2]]));
      }
      return;
    }

    const int first = node.left;
    const int second = node.right;
    const double leftDistance =
        pointAabbDistanceSquared(point, nodes_[first].lo, nodes_[first].hi);
    const double rightDistance =
        pointAabbDistanceSquared(point, nodes_[second].lo, nodes_[second].hi);
    if (leftDistance < rightDistance) {
      queryRecursive(first, point, best);
      queryRecursive(second, point, best);
    } else {
      queryRecursive(second, point, best);
      queryRecursive(first, point, best);
    }
  }

  const Mesh& mesh_;
  std::vector<TriangleRef> triangles_;
  std::vector<int> order_;
  std::vector<BvhNode> nodes_;
};

std::vector<Vec3> sampleSurfacePoints(const Mesh& mesh, int maxSamples) {
  std::vector<double> cumulative;
  cumulative.reserve(mesh.faces.size());
  double totalArea = 0.0;
  for (const Face& face : mesh.faces) {
    const double area = triangleArea(mesh.vertices[face.v[0]], mesh.vertices[face.v[1]],
                                     mesh.vertices[face.v[2]]);
    if (area > 1e-24) {
      totalArea += area;
    }
    cumulative.push_back(totalArea);
  }
  if (totalArea <= 1e-24 || maxSamples <= 0) {
    return {};
  }

  const int samples = maxSamples;
  std::vector<Vec3> points;
  points.reserve(samples);
  for (int i = 0; i < samples; ++i) {
    const double target =
        (static_cast<double>(i) + 0.5) * totalArea / static_cast<double>(samples);
    auto it = std::lower_bound(cumulative.begin(), cumulative.end(), target);
    int faceId = static_cast<int>(std::distance(cumulative.begin(), it));
    faceId = std::min(faceId, static_cast<int>(mesh.faces.size()) - 1);
    while (faceId > 0 && cumulative[faceId] == cumulative[faceId - 1]) {
      --faceId;
    }

    const Face& face = mesh.faces[faceId];
    const double uSeed = std::fmod((static_cast<double>(i) + 0.5) * 0.7548776662, 1.0);
    const double vSeed = std::fmod((static_cast<double>(i) + 0.5) * 0.5698402967, 1.0);
    const double su = std::sqrt(uSeed);
    const double b0 = 1.0 - su;
    const double b1 = su * (1.0 - vSeed);
    const double b2 = su * vSeed;
    points.push_back(b0 * mesh.vertices[face.v[0]] + b1 * mesh.vertices[face.v[1]] +
                     b2 * mesh.vertices[face.v[2]]);
  }
  return points;
}

} // namespace

MeshStats computeMeshStats(const Mesh& mesh) {
  MeshStats stats;
  stats.vertices = static_cast<int>(mesh.vertices.size());
  stats.faces = static_cast<int>(mesh.faces.size());

  const Result<MeshTopology> topologyResult = MeshTopology::build(mesh);
  if (!topologyResult.ok()) {
    return stats;
  }
  const MeshTopology& topology = topologyResult.value();

  std::vector<double> edgeLengths;
  edgeLengths.reserve(topology.edges().size());

  double qualitySum = 0.0;
  stats.minTriangleQuality =
      mesh.faces.empty() ? 0.0 : std::numeric_limits<double>::infinity();

  for (const Face& face : mesh.faces) {
    const Vec3& a = mesh.vertices[face.v[0]];
    const Vec3& b = mesh.vertices[face.v[1]];
    const Vec3& c = mesh.vertices[face.v[2]];
    stats.area += triangleArea(a, b, c);
    const double q = triangleQuality(a, b, c);
    qualitySum += q;
    stats.minTriangleQuality = std::min(stats.minTriangleQuality, q);
  }

  for (const TopologyEdge& edge : topology.edges()) {
    const int a = edge.vertices[0];
    const int b = edge.vertices[1];
    edgeLengths.push_back((mesh.vertices[a] - mesh.vertices[b]).norm());
  }

  stats.edges = topology.edgeCount();
  stats.boundaryEdges = topology.boundaryEdgeCount();
  stats.nonManifoldEdges = topology.nonManifoldEdgeCount();
  stats.meanTriangleQuality =
      mesh.faces.empty() ? 0.0 : qualitySum / static_cast<double>(mesh.faces.size());
  if (mesh.faces.empty()) {
    stats.minTriangleQuality = 0.0;
  }

  if (!edgeLengths.empty()) {
    stats.meanEdgeLength =
        std::accumulate(edgeLengths.begin(), edgeLengths.end(), 0.0) /
        static_cast<double>(edgeLengths.size());
    double variance = 0.0;
    for (double value : edgeLengths) {
      const double d = value - stats.meanEdgeLength;
      variance += d * d;
    }
    variance /= static_cast<double>(edgeLengths.size());
    stats.edgeLengthCv =
        stats.meanEdgeLength > 1e-30 ? std::sqrt(variance) / stats.meanEdgeLength : 0.0;
  }

  return stats;
}

DistanceStats compareMeshesBySampledDistance(const Mesh& original,
                                             const Mesh& simplified, int maxSamples) {
  DistanceStats stats;
  if (original.empty() || simplified.empty()) {
    return stats;
  }

  auto accumulate = [&](const Mesh& from, const Mesh& to, double& mean,
                        double& maxValue) {
    const std::vector<Vec3> points = sampleSurfacePoints(from, maxSamples);
    const MeshDistanceIndex index(to);
    if (points.empty() || index.empty()) {
      mean = 0.0;
      maxValue = 0.0;
      return;
    }
    double sum = 0.0;
    double maxSq = 0.0;
    for (const Vec3& point : points) {
      const double d2 = index.distanceSquared(point);
      sum += std::sqrt(d2);
      maxSq = std::max(maxSq, d2);
    }
    mean = sum / static_cast<double>(points.size());
    maxValue = std::sqrt(maxSq);
  };

  accumulate(original, simplified, stats.meanOriginalToSimplified,
             stats.maxOriginalToSimplified);
  accumulate(simplified, original, stats.meanSimplifiedToOriginal,
             stats.maxSimplifiedToOriginal);
  return stats;
}

std::string statsHeaderCsv() {
  return "label,vertices,faces,edges,boundary_edges,non_manifold_edges,area,"
         "mean_triangle_quality,min_triangle_quality,mean_edge_length,"
         "edge_length_cv,mean_orig_to_simp,max_orig_to_simp,"
         "mean_simp_to_orig,max_simp_to_orig";
}

std::string statsRowCsv(const std::string& label, const MeshStats& stats,
                        const DistanceStats* distance) {
  std::ostringstream out;
  out << std::setprecision(12);
  out << label << "," << stats.vertices << "," << stats.faces << "," << stats.edges
      << "," << stats.boundaryEdges << "," << stats.nonManifoldEdges << ","
      << stats.area << "," << stats.meanTriangleQuality << ","
      << stats.minTriangleQuality << "," << stats.meanEdgeLength << ","
      << stats.edgeLengthCv;
  if (distance) {
    out << "," << distance->meanOriginalToSimplified << ","
        << distance->maxOriginalToSimplified << ","
        << distance->meanSimplifiedToOriginal << ","
        << distance->maxSimplifiedToOriginal;
  } else {
    out << ",,,,";
  }
  return out.str();
}

} // namespace manumesh::simplification
