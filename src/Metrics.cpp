#include "Metrics.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <numeric>
#include <sstream>
#include <unordered_map>

namespace lq {
namespace {

std::uint64_t edgeKey(int a, int b) {
  if (a > b) std::swap(a, b);
  return (static_cast<std::uint64_t>(static_cast<uint32_t>(a)) << 32u) |
         static_cast<uint32_t>(b);
}

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

double pointTriangleDistanceSquared(const Vec3& p, const Vec3& a,
                                    const Vec3& b, const Vec3& c) {
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
    return std::min({(p - a).squaredNorm(), (p - b).squaredNorm(),
                     (p - c).squaredNorm()});
  }
  const double distance = n.dot(ap);
  return distance * distance / nn;
}

double pointMeshDistanceSquared(const Vec3& p, const Mesh& mesh) {
  double best = std::numeric_limits<double>::infinity();
  for (const Face& face : mesh.faces) {
    const Vec3& a = mesh.vertices[face.v[0]];
    const Vec3& b = mesh.vertices[face.v[1]];
    const Vec3& c = mesh.vertices[face.v[2]];
    best = std::min(best, pointTriangleDistanceSquared(p, a, b, c));
  }
  return best;
}

std::vector<int> sampleVertexIds(int count, int maxSamples) {
  std::vector<int> ids;
  if (count <= 0 || maxSamples <= 0) {
    return ids;
  }
  const int samples = std::min(count, maxSamples);
  ids.reserve(samples);
  for (int i = 0; i < samples; ++i) {
    const int id = static_cast<int>(
        std::llround((static_cast<double>(i) * (count - 1)) /
                     std::max(1, samples - 1)));
    ids.push_back(std::min(count - 1, id));
  }
  return ids;
}

}  // namespace

MeshStats computeMeshStats(const Mesh& mesh) {
  MeshStats stats;
  stats.vertices = static_cast<int>(mesh.vertices.size());
  stats.faces = static_cast<int>(mesh.faces.size());

  std::unordered_map<std::uint64_t, int> edgeCounts;
  std::vector<double> edgeLengths;
  edgeCounts.reserve(mesh.faces.size() * 3);
  edgeLengths.reserve(mesh.faces.size() * 3 / 2);

  double qualitySum = 0.0;
  stats.minTriangleQuality = mesh.faces.empty()
                                 ? 0.0
                                 : std::numeric_limits<double>::infinity();

  for (const Face& face : mesh.faces) {
    const Vec3& a = mesh.vertices[face.v[0]];
    const Vec3& b = mesh.vertices[face.v[1]];
    const Vec3& c = mesh.vertices[face.v[2]];
    stats.area += triangleArea(a, b, c);
    const double q = triangleQuality(a, b, c);
    qualitySum += q;
    stats.minTriangleQuality = std::min(stats.minTriangleQuality, q);

    for (int e = 0; e < 3; ++e) {
      edgeCounts[edgeKey(face.v[e], face.v[(e + 1) % 3])] += 1;
    }
  }

  for (const auto& [key, count] : edgeCounts) {
    const int a = static_cast<int>(key >> 32u);
    const int b = static_cast<int>(key & 0xffffffffu);
    edgeLengths.push_back((mesh.vertices[a] - mesh.vertices[b]).norm());
    if (count == 1) {
      ++stats.boundaryEdges;
    } else if (count > 2) {
      ++stats.nonManifoldEdges;
    }
  }

  stats.edges = static_cast<int>(edgeCounts.size());
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
        stats.meanEdgeLength > 1e-30 ? std::sqrt(variance) / stats.meanEdgeLength
                                     : 0.0;
  }

  return stats;
}

DistanceStats compareMeshesBySampledDistance(const Mesh& original,
                                             const Mesh& simplified,
                                             int maxSamples) {
  DistanceStats stats;
  if (original.empty() || simplified.empty()) {
    return stats;
  }

  auto accumulate = [&](const Mesh& from, const Mesh& to, double& mean,
                        double& maxValue) {
    const std::vector<int> ids =
        sampleVertexIds(static_cast<int>(from.vertices.size()), maxSamples);
    if (ids.empty()) {
      mean = 0.0;
      maxValue = 0.0;
      return;
    }
    double sum = 0.0;
    double maxSq = 0.0;
    for (int id : ids) {
      const double d2 = pointMeshDistanceSquared(from.vertices[id], to);
      sum += std::sqrt(d2);
      maxSq = std::max(maxSq, d2);
    }
    mean = sum / static_cast<double>(ids.size());
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
  out << label << "," << stats.vertices << "," << stats.faces << ","
      << stats.edges << "," << stats.boundaryEdges << ","
      << stats.nonManifoldEdges << "," << stats.area << ","
      << stats.meanTriangleQuality << "," << stats.minTriangleQuality << ","
      << stats.meanEdgeLength << "," << stats.edgeLengthCv;
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

}  // namespace lq
