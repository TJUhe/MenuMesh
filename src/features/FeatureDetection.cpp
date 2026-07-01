#include "line_quadrics_qem/features/FeatureDetection.h"

#include <Eigen/Eigenvalues>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <numeric>
#include <queue>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace lq {
namespace {

constexpr double kPi = 3.141592653589793238462643383279502884;

struct EdgeInfo {
  std::vector<int> faces;
};

struct CandidateEdge {
  int a = -1;
  int b = -1;
  bool boundary = false;
  bool dihedral = false;
  bool nonManifold = false;
};

std::uint64_t edgeKey(int a, int b) {
  if (a > b) std::swap(a, b);
  return (static_cast<std::uint64_t>(static_cast<uint32_t>(a)) << 32u) |
         static_cast<uint32_t>(b);
}

std::pair<int, int> unpackEdgeKey(std::uint64_t key) {
  return {static_cast<int>(key >> 32u), static_cast<int>(key & 0xffffffffu)};
}

std::vector<Vec3> computeFaceNormals(const Mesh& mesh) {
  std::vector<Vec3> normals(mesh.faces.size(), Vec3::Zero());
  for (int fi = 0; fi < static_cast<int>(mesh.faces.size()); ++fi) {
    const Face& f = mesh.faces[fi];
    normals[fi] = triangleNormal(mesh.vertices[f.v[0]], mesh.vertices[f.v[1]],
                                 mesh.vertices[f.v[2]]);
  }
  return normals;
}

std::unordered_map<std::uint64_t, EdgeInfo> buildEdgeInfo(const Mesh& mesh) {
  std::unordered_map<std::uint64_t, EdgeInfo> edges;
  edges.reserve(mesh.faces.size() * 3);
  for (int fi = 0; fi < static_cast<int>(mesh.faces.size()); ++fi) {
    const Face& face = mesh.faces[fi];
    for (int e = 0; e < 3; ++e) {
      edges[edgeKey(face.v[e], face.v[(e + 1) % 3])].faces.push_back(fi);
    }
  }
  return edges;
}

std::vector<CandidateEdge> collectFeatureEdges(const Mesh& mesh,
                                               const FeatureOptions& options,
                                               FeatureAnalysis& analysis) {
  std::vector<CandidateEdge> result;
  const std::vector<Vec3> normals = computeFaceNormals(mesh);
  const auto edges = buildEdgeInfo(mesh);
  const double threshold = options.featureAngleDeg * kPi / 180.0;

  for (const auto& [key, info] : edges) {
    CandidateEdge edge;
    const auto [a, b] = unpackEdgeKey(key);
    edge.a = a;
    edge.b = b;

    if (info.faces.size() == 1) {
      edge.boundary = true;
    } else if (info.faces.size() == 2) {
      const double dot = std::clamp(
          std::abs(normals[info.faces[0]].dot(normals[info.faces[1]])), -1.0, 1.0);
      edge.dihedral = std::acos(dot) >= threshold;
    } else if (info.faces.size() > 2) {
      edge.nonManifold = true;
    }

    if (edge.boundary || edge.dihedral || edge.nonManifold) {
      result.push_back(edge);
      ++analysis.featureEdges;
      if (edge.boundary) ++analysis.boundaryFeatureEdges;
      if (edge.dihedral) ++analysis.dihedralFeatureEdges;
      if (edge.nonManifold) ++analysis.nonManifoldFeatureEdges;
    }
  }
  return result;
}

bool fitCircle(const Mesh& mesh, FeatureLoop& loop, double relThreshold) {
  if (loop.vertices.size() < 5) {
    return false;
  }

  Vec3 mean = Vec3::Zero();
  for (int id : loop.vertices) {
    mean += mesh.vertices[id];
  }
  mean /= static_cast<double>(loop.vertices.size());

  Eigen::Matrix3d covariance = Eigen::Matrix3d::Zero();
  for (int id : loop.vertices) {
    const Vec3 d = mesh.vertices[id] - mean;
    covariance += d * d.transpose();
  }

  Eigen::SelfAdjointEigenSolver<Eigen::Matrix3d> eig(covariance);
  if (eig.info() != Eigen::Success) {
    return false;
  }

  Vec3 normal = eig.eigenvectors().col(0).normalized();
  Vec3 u = eig.eigenvectors().col(2).normalized();
  Vec3 v = normal.cross(u).normalized();
  if (u.norm() <= 1e-20 || v.norm() <= 1e-20) {
    return false;
  }

  Eigen::Matrix3d ata = Eigen::Matrix3d::Zero();
  Eigen::Vector3d atb = Eigen::Vector3d::Zero();
  for (int id : loop.vertices) {
    const Vec3 d = mesh.vertices[id] - mean;
    const double x = d.dot(u);
    const double y = d.dot(v);
    Eigen::Vector3d row(2.0 * x, 2.0 * y, 1.0);
    ata += row * row.transpose();
    atb += row * (x * x + y * y);
  }

  Eigen::LDLT<Eigen::Matrix3d> ldlt(ata);
  if (ldlt.info() != Eigen::Success) {
    return false;
  }

  const Eigen::Vector3d solution = ldlt.solve(atb);
  if (!solution.allFinite()) {
    return false;
  }

  const double cx = solution.x();
  const double cy = solution.y();
  const double radiusSq = solution.z() + cx * cx + cy * cy;
  if (radiusSq <= 1e-24 || !std::isfinite(radiusSq)) {
    return false;
  }

  const Vec3 center = mean + cx * u + cy * v;
  const double radius = std::sqrt(radiusSq);
  double radialSq = 0.0;
  double planeSq = 0.0;
  double radialMax = 0.0;
  double planeMax = 0.0;
  for (int id : loop.vertices) {
    const Vec3 d = mesh.vertices[id] - center;
    const double plane = d.dot(normal);
    const Vec3 inPlane = d - plane * normal;
    const double radial = inPlane.norm() - radius;
    radialSq += radial * radial;
    planeSq += plane * plane;
    radialMax = std::max(radialMax, std::abs(radial));
    planeMax = std::max(planeMax, std::abs(plane));
  }

  const double invN = 1.0 / static_cast<double>(loop.vertices.size());
  loop.center = center;
  loop.normal = normal;
  loop.radius = radius;
  loop.rmsRadialError = std::sqrt(radialSq * invN);
  loop.maxRadialError = radialMax;
  loop.rmsPlaneError = std::sqrt(planeSq * invN);
  loop.maxPlaneError = planeMax;

  const double relRms =
      (loop.rmsRadialError + loop.rmsPlaneError) / std::max(1e-12, radius);
  const double relMax =
      std::max(loop.maxRadialError, loop.maxPlaneError) / std::max(1e-12, radius);
  return relRms <= relThreshold && relMax <= std::max(3.0 * relThreshold, 0.08);
}

Vec3 fallbackTangentFromNeighbors(int id, const std::vector<int>& neighbors,
                                  const Mesh& mesh) {
  Vec3 t = Vec3::Zero();
  for (int nb : neighbors) {
    Vec3 d = mesh.vertices[nb] - mesh.vertices[id];
    if (d.norm() > 1e-20) {
      t += d.normalized();
    }
  }
  if (t.norm() <= 1e-20 && neighbors.size() >= 2) {
    t = mesh.vertices[neighbors[1]] - mesh.vertices[neighbors[0]];
  }
  if (t.norm() <= 1e-20) {
    return Vec3(1.0, 0.0, 0.0);
  }
  return t.normalized();
}

} // namespace

FeatureAnalysis detectFeatureCurves(const Mesh& mesh, const FeatureOptions& options) {
  FeatureAnalysis analysis;
  analysis.vertices.assign(mesh.vertices.size(), VertexFeature{});
  if (mesh.empty()) {
    return analysis;
  }

  const std::vector<CandidateEdge> featureEdges =
      collectFeatureEdges(mesh, options, analysis);

  std::vector<std::vector<int>> adjacency(mesh.vertices.size());
  std::unordered_map<std::uint64_t, bool> edgeIsBoundary;
  edgeIsBoundary.reserve(featureEdges.size());
  for (const CandidateEdge& edge : featureEdges) {
    adjacency[edge.a].push_back(edge.b);
    adjacency[edge.b].push_back(edge.a);
    edgeIsBoundary[edgeKey(edge.a, edge.b)] = edge.boundary;
  }

  std::vector<char> visited(mesh.vertices.size(), 0);
  int loopId = 0;
  for (int seed = 0; seed < static_cast<int>(mesh.vertices.size()); ++seed) {
    if (visited[seed] || adjacency[seed].empty()) {
      continue;
    }

    std::vector<int> component;
    std::queue<int> queue;
    queue.push(seed);
    visited[seed] = 1;
    while (!queue.empty()) {
      const int v = queue.front();
      queue.pop();
      component.push_back(v);
      for (int nb : adjacency[v]) {
        if (!visited[nb]) {
          visited[nb] = 1;
          queue.push(nb);
        }
      }
    }

    int degreeTwo = 0;
    int edgeCount2x = 0;
    int boundaryEdges = 0;
    for (int v : component) {
      const int degree = static_cast<int>(adjacency[v].size());
      if (degree == 2) {
        ++degreeTwo;
      }
      edgeCount2x += degree;
      for (int nb : adjacency[v]) {
        if (v < nb) {
          const auto it = edgeIsBoundary.find(edgeKey(v, nb));
          if (it != edgeIsBoundary.end() && it->second) {
            ++boundaryEdges;
          }
        }
      }
    }

    FeatureLoop loop;
    loop.id = loopId++;
    loop.vertices = component;
    loop.edgeCount = edgeCount2x / 2;
    loop.closed = !component.empty() &&
                  degreeTwo == static_cast<int>(component.size()) &&
                  loop.edgeCount == static_cast<int>(component.size());
    loop.mostlyBoundary =
        loop.edgeCount > 0 &&
        boundaryEdges >= static_cast<int>(0.6 * static_cast<double>(loop.edgeCount));
    if (loop.closed &&
        static_cast<int>(loop.vertices.size()) >= options.minFeatureLoopVertices) {
      loop.circular = fitCircle(mesh, loop, options.circleFitRelativeThreshold);
    }

    for (int id : loop.vertices) {
      VertexFeature& vf = analysis.vertices[id];
      vf.isFeature = true;
      vf.loopId = loop.id;
      vf.circular = loop.circular;
      vf.junction = adjacency[id].size() != 2;
      if (loop.circular) {
        vf.circleCenter = loop.center;
        vf.circleNormal = loop.normal;
        vf.circleRadius = loop.radius;
        Vec3 radial = mesh.vertices[id] - loop.center;
        radial -= loop.normal * radial.dot(loop.normal);
        if (radial.norm() > 1e-20) {
          vf.tangent = loop.normal.cross(radial).normalized();
        } else {
          vf.tangent = fallbackTangentFromNeighbors(id, adjacency[id], mesh);
        }
      } else {
        vf.tangent = fallbackTangentFromNeighbors(id, adjacency[id], mesh);
      }
    }

    analysis.loops.push_back(loop);
  }

  return analysis;
}

DirectionalCurveError measureLoopAgainstCircle(const Mesh& mesh,
                                               const FeatureLoop& loop,
                                               const Vec3& center, const Vec3& normalIn,
                                               double radius) {
  DirectionalCurveError error;
  Vec3 normal = normalIn;
  if (normal.norm() <= 1e-20 || radius <= 1e-20) {
    return error;
  }
  normal.normalize();

  for (int id : loop.vertices) {
    if (id < 0 || id >= static_cast<int>(mesh.vertices.size())) {
      continue;
    }
    const Vec3 d = mesh.vertices[id] - center;
    const double plane = d.dot(normal);
    const Vec3 inPlane = d - plane * normal;
    const double radial = inPlane.norm() - radius;
    error.radialRms += radial * radial;
    error.planeRms += plane * plane;
    error.radialMax = std::max(error.radialMax, std::abs(radial));
    error.planeMax = std::max(error.planeMax, std::abs(plane));
    ++error.samples;
  }
  if (error.samples > 0) {
    const double inv = 1.0 / static_cast<double>(error.samples);
    error.radialRms = std::sqrt(error.radialRms * inv);
    error.planeRms = std::sqrt(error.planeRms * inv);
  }
  return error;
}

std::string featureReportHeaderCsv() {
  return "loop_id,vertices,edges,closed,circular,mostly_boundary,cx,cy,cz,nx,ny,nz,"
         "radius,rms_radial,max_radial,rms_plane,max_plane";
}

std::string featureLoopRowCsv(const FeatureLoop& loop) {
  std::ostringstream out;
  out << std::setprecision(12);
  out << loop.id << "," << loop.vertices.size() << "," << loop.edgeCount << ","
      << (loop.closed ? 1 : 0) << "," << (loop.circular ? 1 : 0) << ","
      << (loop.mostlyBoundary ? 1 : 0) << "," << loop.center.x() << ","
      << loop.center.y() << "," << loop.center.z() << "," << loop.normal.x() << ","
      << loop.normal.y() << "," << loop.normal.z() << "," << loop.radius << ","
      << loop.rmsRadialError << "," << loop.maxRadialError << "," << loop.rmsPlaneError
      << "," << loop.maxPlaneError;
  return out.str();
}

} // namespace lq
