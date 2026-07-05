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
  bool normalTensor = false;
  bool nonManifold = false;
  int signedKind = 0;
  double angleRad = 0.0;
};

struct PrimitiveFit {
  bool valid = false;
  FeaturePrimitiveType primitive = FeaturePrimitiveType::Unknown;
  Vec3 center = Vec3::Zero();
  Vec3 normal = Vec3(0.0, 0.0, 1.0);
  Vec3 majorAxis = Vec3(1.0, 0.0, 0.0);
  Vec3 minorAxis = Vec3(0.0, 1.0, 0.0);
  double radius = 0.0;
  double majorRadius = 0.0;
  double minorRadius = 0.0;
  double axisRatio = 0.0;
  double rmsRadialError = 0.0;
  double maxRadialError = 0.0;
  double rmsEllipseError = 0.0;
  double maxEllipseError = 0.0;
  double rmsPlaneError = 0.0;
  double maxPlaneError = 0.0;
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

Vec3 faceCentroid(const Mesh& mesh, const Face& face) {
  return (mesh.vertices[face.v[0]] + mesh.vertices[face.v[1]] +
          mesh.vertices[face.v[2]]) /
         3.0;
}

int signedDihedralKind(const Mesh& mesh, const std::vector<Vec3>& normals,
                       const EdgeInfo& info, int a, int b) {
  if (info.faces.size() != 2) {
    return 0;
  }

  const int f0 = info.faces[0];
  const int f1 = info.faces[1];
  Vec3 edge = mesh.vertices[b] - mesh.vertices[a];
  if (edge.norm() <= 1e-20 || normals[f0].norm() <= 1e-20 ||
      normals[f1].norm() <= 1e-20) {
    return 0;
  }
  edge.normalize();

  const Vec3 c0 = faceCentroid(mesh, mesh.faces[f0]);
  const Vec3 c1 = faceCentroid(mesh, mesh.faces[f1]);
  const double side0 = edge.cross(normals[f0]).dot(c1 - c0);
  const double side1 = edge.cross(normals[f1]).dot(c0 - c1);
  if (std::abs(side0) <= 1e-12 || std::abs(side1) <= 1e-12) {
    return 0;
  }
  const bool normalsPointTowardEachOther = side0 > 0.0 && side1 > 0.0;
  return normalsPointTowardEachOther ? -1 : 1;
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

std::vector<std::vector<int>> buildVertexNeighbors(const Mesh& mesh) {
  std::vector<std::unordered_set<int>> neighborSets(mesh.vertices.size());
  for (const Face& face : mesh.faces) {
    for (int i = 0; i < 3; ++i) {
      const int a = face.v[i];
      const int b = face.v[(i + 1) % 3];
      if (a >= 0 && b >= 0 && a < static_cast<int>(neighborSets.size()) &&
          b < static_cast<int>(neighborSets.size()) && a != b) {
        neighborSets[a].insert(b);
        neighborSets[b].insert(a);
      }
    }
  }

  std::vector<std::vector<int>> neighbors(mesh.vertices.size());
  for (int i = 0; i < static_cast<int>(neighborSets.size()); ++i) {
    neighbors[i].assign(neighborSets[i].begin(), neighborSets[i].end());
  }
  return neighbors;
}

NormalTensorVertex analyzeNormalTensor(const Eigen::Matrix3d& tensor) {
  NormalTensorVertex result;
  Eigen::SelfAdjointEigenSolver<Eigen::Matrix3d> eig(tensor);
  if (eig.info() != Eigen::Success) {
    return result;
  }

  const double l0 = std::max(0.0, eig.eigenvalues()(2));
  const double l1 = std::max(0.0, eig.eigenvalues()(1));
  const double l2 = std::max(0.0, eig.eigenvalues()(0));
  result.normal = eig.eigenvectors().col(2).normalized();
  result.creaseTangent = eig.eigenvectors().col(0).normalized();
  result.surfaceSaliency = std::max(0.0, l0 - l1);
  result.creaseSaliency = std::max(0.0, l1 - l2);
  result.cornerSaliency = l2;
  result.featureScore = std::max(result.creaseSaliency, result.cornerSaliency);
  return result;
}

bool normalTensorEdgeCandidate(const CandidateEdge& edge,
                               const std::vector<NormalTensorVertex>& tensor,
                               const std::vector<char>& discreteFeatureVertex,
                               const Mesh& mesh, const FeatureOptions& options,
                               FeatureAnalysis& analysis) {
  if (!options.useNormalTensorFeatures || edge.a < 0 || edge.b < 0 ||
      edge.a >= static_cast<int>(tensor.size()) ||
      edge.b >= static_cast<int>(tensor.size())) {
    return false;
  }
  if (edge.a < static_cast<int>(discreteFeatureVertex.size()) &&
      edge.b < static_cast<int>(discreteFeatureVertex.size()) &&
      (discreteFeatureVertex[edge.a] || discreteFeatureVertex[edge.b])) {
    return false;
  }

  const double score =
      0.5 * (tensor[edge.a].featureScore + tensor[edge.b].featureScore);
  analysis.maxNormalTensorFeatureScore =
      std::max(analysis.maxNormalTensorFeatureScore, score);
  const double minEndpointScore =
      std::min(tensor[edge.a].featureScore, tensor[edge.b].featureScore);
  if (minEndpointScore < options.normalTensorFeatureThreshold) {
    return false;
  }
  if (tensor[edge.a].creaseSaliency < tensor[edge.a].cornerSaliency ||
      tensor[edge.b].creaseSaliency < tensor[edge.b].cornerSaliency) {
    return false;
  }

  Vec3 direction = mesh.vertices[edge.b] - mesh.vertices[edge.a];
  const double length = direction.norm();
  if (length <= 1e-20) {
    return false;
  }
  direction /= length;
  const double alignA = std::abs(direction.dot(tensor[edge.a].creaseTangent));
  const double alignB = std::abs(direction.dot(tensor[edge.b].creaseTangent));
  return std::max(alignA, alignB) >= options.normalTensorMinEdgeAlignment;
}

std::vector<CandidateEdge> collectFeatureEdges(const Mesh& mesh,
                                               const FeatureOptions& options,
                                               FeatureAnalysis& analysis) {
  std::vector<CandidateEdge> result;
  const std::vector<Vec3> normals = computeFaceNormals(mesh);
  const auto edges = buildEdgeInfo(mesh);
  const double threshold = options.featureAngleDeg * kPi / 180.0;
  const std::vector<NormalTensorVertex> tensor =
      options.useNormalTensorFeatures
          ? computeNormalTensorFeatures(
                mesh, NormalTensorOptions{options.normalTensorSmoothingIterations,
                                          options.normalTensorScaleCount})
          : std::vector<NormalTensorVertex>();
  std::vector<char> discreteFeatureVertex(mesh.vertices.size(), 0);
  for (const auto& [key, info] : edges) {
    bool discrete = false;
    if (info.faces.size() == 1 || info.faces.size() > 2) {
      discrete = true;
    } else if (info.faces.size() == 2) {
      const double dot = std::clamp(
          std::abs(normals[info.faces[0]].dot(normals[info.faces[1]])), -1.0, 1.0);
      discrete = std::acos(dot) >= threshold;
    }
    if (discrete) {
      const auto [a, b] = unpackEdgeKey(key);
      discreteFeatureVertex[a] = 1;
      discreteFeatureVertex[b] = 1;
    }
  }

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
      edge.angleRad = std::acos(dot);
      edge.dihedral = edge.angleRad >= threshold;
      if (edge.dihedral) {
        edge.signedKind = signedDihedralKind(mesh, normals, info, a, b);
      }
    } else if (info.faces.size() > 2) {
      edge.nonManifold = true;
    }
    edge.normalTensor = !edge.boundary && !edge.dihedral && !edge.nonManifold &&
                        normalTensorEdgeCandidate(edge, tensor, discreteFeatureVertex,
                                                  mesh, options, analysis);

    if (edge.boundary || edge.dihedral || edge.normalTensor || edge.nonManifold) {
      result.push_back(edge);
      ++analysis.featureEdges;
      if (edge.boundary) ++analysis.boundaryFeatureEdges;
      if (edge.dihedral) ++analysis.dihedralFeatureEdges;
      if (edge.normalTensor) ++analysis.normalTensorFeatureEdges;
      if (edge.nonManifold) ++analysis.nonManifoldFeatureEdges;
      if (edge.signedKind > 0) ++analysis.convexFeatureEdges;
      if (edge.signedKind < 0) ++analysis.concaveFeatureEdges;
      if (edge.dihedral && edge.signedKind == 0) ++analysis.unknownSignedFeatureEdges;
    }
  }
  return result;
}

PrimitiveFit fitPrimitive(const Mesh& mesh, const FeatureLoop& loop,
                          const FeatureOptions& options) {
  PrimitiveFit fit;
  if (loop.vertices.size() < 5) {
    return fit;
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
    return fit;
  }

  Vec3 normal = eig.eigenvectors().col(0).normalized();
  Vec3 u = eig.eigenvectors().col(2).normalized();
  Vec3 v = normal.cross(u).normalized();
  if (u.norm() <= 1e-20 || v.norm() <= 1e-20) {
    return fit;
  }

  Eigen::Matrix3d ata = Eigen::Matrix3d::Zero();
  Eigen::Vector3d atb = Eigen::Vector3d::Zero();
  double majorSq = 0.0;
  double minorSq = 0.0;
  for (int id : loop.vertices) {
    const Vec3 d = mesh.vertices[id] - mean;
    const double x = d.dot(u);
    const double y = d.dot(v);
    majorSq += x * x;
    minorSq += y * y;
    Eigen::Vector3d row(2.0 * x, 2.0 * y, 1.0);
    ata += row * row.transpose();
    atb += row * (x * x + y * y);
  }

  Eigen::LDLT<Eigen::Matrix3d> ldlt(ata);
  if (ldlt.info() != Eigen::Success) {
    return fit;
  }

  const Eigen::Vector3d solution = ldlt.solve(atb);
  if (!solution.allFinite()) {
    return fit;
  }

  const double cx = solution.x();
  const double cy = solution.y();
  const double radiusSq = solution.z() + cx * cx + cy * cy;
  if (radiusSq <= 1e-24 || !std::isfinite(radiusSq)) {
    return fit;
  }

  const Vec3 center = mean + cx * u + cy * v;
  const double radius = std::sqrt(radiusSq);
  const double invN = 1.0 / static_cast<double>(loop.vertices.size());
  const double majorRadius = std::sqrt(std::max(0.0, 2.0 * majorSq * invN));
  const double minorRadius = std::sqrt(std::max(0.0, 2.0 * minorSq * invN));
  if (majorRadius <= 1e-20 || minorRadius <= 1e-20) {
    return fit;
  }

  double radialSq = 0.0;
  double planeSq = 0.0;
  double ellipseSq = 0.0;
  double radialMax = 0.0;
  double planeMax = 0.0;
  double ellipseMax = 0.0;
  for (int id : loop.vertices) {
    const Vec3 d = mesh.vertices[id] - center;
    const double plane = d.dot(normal);
    const Vec3 inPlane = d - plane * normal;
    const double radial = inPlane.norm() - radius;
    radialSq += radial * radial;
    planeSq += plane * plane;
    radialMax = std::max(radialMax, std::abs(radial));
    planeMax = std::max(planeMax, std::abs(plane));

    const Vec3 de = mesh.vertices[id] - mean;
    const double ex = de.dot(u);
    const double ey = de.dot(v);
    const double ellipse = (std::sqrt((ex * ex) / (majorRadius * majorRadius) +
                                      (ey * ey) / (minorRadius * minorRadius)) -
                            1.0) *
                           std::sqrt(majorRadius * minorRadius);
    ellipseSq += ellipse * ellipse;
    ellipseMax = std::max(ellipseMax, std::abs(ellipse));
  }

  fit.valid = true;
  fit.center = center;
  fit.normal = normal;
  fit.majorAxis = u;
  fit.minorAxis = v;
  fit.radius = radius;
  fit.majorRadius = majorRadius;
  fit.minorRadius = minorRadius;
  fit.axisRatio = minorRadius / std::max(1e-12, majorRadius);
  fit.rmsRadialError = std::sqrt(radialSq * invN);
  fit.maxRadialError = radialMax;
  fit.rmsEllipseError = std::sqrt(ellipseSq * invN);
  fit.maxEllipseError = ellipseMax;
  fit.rmsPlaneError = std::sqrt(planeSq * invN);
  fit.maxPlaneError = planeMax;

  const double relRms =
      (fit.rmsRadialError + fit.rmsPlaneError) / std::max(1e-12, fit.radius);
  const double relMax =
      std::max(fit.maxRadialError, fit.maxPlaneError) / std::max(1e-12, fit.radius);
  const bool circleFit =
      relRms <= options.circleFitRelativeThreshold &&
      relMax <= std::max(3.0 * options.circleFitRelativeThreshold, 0.08);

  const double ellipseScale = std::max(1e-12, std::sqrt(majorRadius * minorRadius));
  const double ellipseRelRms = (fit.rmsEllipseError + fit.rmsPlaneError) / ellipseScale;
  const double ellipseRelMax =
      std::max(fit.maxEllipseError, fit.maxPlaneError) / ellipseScale;
  const bool ellipseFit =
      ellipseRelRms <= options.ellipseFitRelativeThreshold &&
      ellipseRelMax <= std::max(3.0 * options.ellipseFitRelativeThreshold, 0.08);

  if (circleFit) {
    const double axisError = std::abs(1.0 - fit.axisRatio);
    if (axisError <= std::min(0.01, 0.25 * options.nearCircleAxisRatioTolerance)) {
      fit.primitive = FeaturePrimitiveType::Circle;
    } else if (axisError <= options.nearCircleAxisRatioTolerance) {
      fit.primitive = FeaturePrimitiveType::NearCircle;
    } else {
      fit.primitive = FeaturePrimitiveType::Ellipse;
    }
  } else if (ellipseFit) {
    fit.primitive =
        std::abs(1.0 - fit.axisRatio) <= options.nearCircleAxisRatioTolerance
            ? FeaturePrimitiveType::NearCircle
            : FeaturePrimitiveType::Ellipse;
  } else {
    fit.primitive = FeaturePrimitiveType::PolygonalLoop;
  }
  return fit;
}

void applyPrimitiveFit(const PrimitiveFit& fit, FeatureLoop& loop) {
  if (!fit.valid) {
    return;
  }
  loop.primitive = fit.primitive;
  loop.circular = fit.primitive == FeaturePrimitiveType::Circle ||
                  fit.primitive == FeaturePrimitiveType::NearCircle;
  loop.center = fit.center;
  loop.normal = fit.normal;
  loop.majorAxis = fit.majorAxis;
  loop.minorAxis = fit.minorAxis;
  loop.radius = fit.radius;
  loop.majorRadius = fit.majorRadius;
  loop.minorRadius = fit.minorRadius;
  loop.axisRatio = fit.axisRatio;
  loop.rmsRadialError = fit.rmsRadialError;
  loop.maxRadialError = fit.maxRadialError;
  loop.rmsEllipseError = fit.rmsEllipseError;
  loop.maxEllipseError = fit.maxEllipseError;
  loop.rmsPlaneError = fit.rmsPlaneError;
  loop.maxPlaneError = fit.maxPlaneError;
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

std::vector<NormalTensorVertex>
computeNormalTensorFeatures(const Mesh& mesh, const NormalTensorOptions& options) {
  std::vector<NormalTensorVertex> result(mesh.vertices.size());
  if (mesh.empty()) {
    return result;
  }

  std::vector<Eigen::Matrix3d> tensors(mesh.vertices.size(), Eigen::Matrix3d::Zero());
  std::vector<double> weights(mesh.vertices.size(), 0.0);
  for (const Face& face : mesh.faces) {
    const Vec3& a = mesh.vertices[face.v[0]];
    const Vec3& b = mesh.vertices[face.v[1]];
    const Vec3& c = mesh.vertices[face.v[2]];
    const Vec3 normal = triangleNormal(a, b, c);
    const double area = triangleArea(a, b, c);
    if (normal.norm() <= 1e-20 || area <= 1e-24) {
      continue;
    }
    const Eigen::Matrix3d tensor = normal * normal.transpose();
    for (int id : face.v) {
      tensors[id] += area * tensor;
      weights[id] += area;
    }
  }

  for (int i = 0; i < static_cast<int>(tensors.size()); ++i) {
    if (weights[i] > 1e-24) {
      tensors[i] /= weights[i];
    }
  }

  const std::vector<std::vector<int>> neighbors = buildVertexNeighbors(mesh);
  const int baseIterations = std::clamp(options.smoothingIterations, 0, 8);
  const int scaleCount = std::clamp(options.scaleCount, 1, 8);

  auto smoothOnce = [&](std::vector<Eigen::Matrix3d>& current) {
    std::vector<Eigen::Matrix3d> next = tensors;
    next = current;
    for (int i = 0; i < static_cast<int>(current.size()); ++i) {
      if (neighbors[i].empty()) {
        continue;
      }
      Eigen::Matrix3d sum = current[i];
      double count = 1.0;
      for (int nb : neighbors[i]) {
        sum += current[nb];
        count += 1.0;
      }
      next[i] = sum / count;
    }
    current.swap(next);
  };

  for (int iter = 0; iter < baseIterations; ++iter) {
    smoothOnce(tensors);
  }

  for (int scale = 0; scale < scaleCount; ++scale) {
    for (int i = 0; i < static_cast<int>(tensors.size()); ++i) {
      const NormalTensorVertex candidate = analyzeNormalTensor(tensors[i]);
      if (scale == 0 || candidate.featureScore > result[i].featureScore) {
        result[i] = candidate;
      }
    }
    if (scale + 1 < scaleCount) {
      smoothOnce(tensors);
    }
  }
  return result;
}

FeatureAnalysis detectFeatureCurves(const Mesh& mesh, const FeatureOptions& options) {
  FeatureAnalysis analysis;
  analysis.vertices.assign(mesh.vertices.size(), VertexFeature{});
  if (mesh.empty()) {
    return analysis;
  }

  const std::vector<CandidateEdge> featureEdges =
      collectFeatureEdges(mesh, options, analysis);
  analysis.graph.vertices.assign(mesh.vertices.size(), FeatureGraphVertex{});
  analysis.graph.edges.reserve(featureEdges.size());
  for (const CandidateEdge& edge : featureEdges) {
    FeatureGraphEdge graphEdge;
    graphEdge.a = edge.a;
    graphEdge.b = edge.b;
    graphEdge.boundary = edge.boundary;
    graphEdge.dihedral = edge.dihedral;
    graphEdge.normalTensor = edge.normalTensor;
    graphEdge.nonManifold = edge.nonManifold;
    graphEdge.signedKind = edge.signedKind;
    const int edgeId = static_cast<int>(analysis.graph.edges.size());
    analysis.graph.edges.push_back(graphEdge);
    if (edge.a >= 0 && edge.a < static_cast<int>(analysis.graph.vertices.size())) {
      analysis.graph.vertices[edge.a].incidentEdges.push_back(edgeId);
    }
    if (edge.b >= 0 && edge.b < static_cast<int>(analysis.graph.vertices.size())) {
      analysis.graph.vertices[edge.b].incidentEdges.push_back(edgeId);
    }
  }
  const double loopTraceAngle =
      std::max(options.featureAngleDeg * kPi / 180.0, 40.0 * kPi / 180.0);

  std::vector<std::vector<int>> adjacency(mesh.vertices.size());
  std::vector<char> traceVertex(mesh.vertices.size(), 0);
  std::unordered_map<std::uint64_t, bool> edgeIsBoundary;
  edgeIsBoundary.reserve(featureEdges.size());
  std::unordered_map<std::uint64_t, int> edgeSignedKind;
  edgeSignedKind.reserve(featureEdges.size());
  std::vector<std::pair<int, int>> graphEdges;
  graphEdges.reserve(featureEdges.size());
  for (const CandidateEdge& edge : featureEdges) {
    const bool traceEdge = edge.boundary || edge.nonManifold || edge.normalTensor ||
                           (edge.dihedral && edge.angleRad >= loopTraceAngle);
    if (!traceEdge) {
      continue;
    }
    adjacency[edge.a].push_back(edge.b);
    adjacency[edge.b].push_back(edge.a);
    traceVertex[edge.a] = 1;
    traceVertex[edge.b] = 1;
    edgeIsBoundary[edgeKey(edge.a, edge.b)] = edge.boundary;
    edgeSignedKind[edgeKey(edge.a, edge.b)] = edge.signedKind;
    graphEdges.emplace_back(edge.a, edge.b);
  }

  std::unordered_set<std::uint64_t> visitedEdges;
  visitedEdges.reserve(featureEdges.size());
  int loopId = 0;

  auto edgeBoundary = [&](int a, int b) {
    const auto it = edgeIsBoundary.find(edgeKey(a, b));
    return it != edgeIsBoundary.end() && it->second;
  };

  auto edgeSign = [&](int a, int b) {
    const auto it = edgeSignedKind.find(edgeKey(a, b));
    return it == edgeSignedKind.end() ? 0 : it->second;
  };

  auto markEdge = [&](int a, int b) { visitedEdges.insert(edgeKey(a, b)); };

  auto edgeVisited = [&](int a, int b) {
    return visitedEdges.find(edgeKey(a, b)) != visitedEdges.end();
  };

  auto assignLoopToVertices = [&](const FeatureLoop& loop) {
    for (int id : loop.vertices) {
      VertexFeature& vf = analysis.vertices[id];
      if (id >= 0 && id < static_cast<int>(analysis.graph.vertices.size())) {
        FeatureGraphVertex& gv = analysis.graph.vertices[id];
        if (std::find(gv.loopIds.begin(), gv.loopIds.end(), loop.id) ==
            gv.loopIds.end()) {
          gv.loopIds.push_back(loop.id);
        }
      }
      const bool alreadyFeature = vf.isFeature;
      if (!alreadyFeature) {
        vf.isFeature = true;
        vf.loopId = loop.id;
        vf.circular = loop.circular;
        vf.primitive = loop.primitive;
      }
      vf.junction = vf.junction || alreadyFeature || adjacency[id].size() != 2;
      if (loop.circular && (!alreadyFeature || !vf.circular)) {
        vf.loopId = loop.id;
        vf.circular = true;
        vf.primitive = loop.primitive;
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
      } else if (loop.primitive == FeaturePrimitiveType::Ellipse &&
                 (!alreadyFeature || vf.primitive == FeaturePrimitiveType::Unknown ||
                  vf.tangent.norm() <= 1e-20)) {
        vf.loopId = loop.id;
        vf.primitive = loop.primitive;
        vf.ellipseCenter = loop.center;
        vf.ellipseNormal = loop.normal;
        vf.ellipseMajorAxis = loop.majorAxis;
        vf.ellipseMinorAxis = loop.minorAxis;
        vf.ellipseMajorRadius = loop.majorRadius;
        vf.ellipseMinorRadius = loop.minorRadius;
        Vec3 major = loop.majorAxis;
        Vec3 minor = loop.minorAxis;
        if (major.norm() > 1e-20 && minor.norm() > 1e-20 && loop.majorRadius > 1e-20 &&
            loop.minorRadius > 1e-20) {
          major.normalize();
          minor.normalize();
          const Vec3 delta = mesh.vertices[id] - loop.center;
          const double theta = std::atan2(delta.dot(minor) / loop.minorRadius,
                                          delta.dot(major) / loop.majorRadius);
          Vec3 tangent = -loop.majorRadius * std::sin(theta) * major +
                         loop.minorRadius * std::cos(theta) * minor;
          vf.tangent = tangent.norm() > 1e-20
                           ? tangent.normalized()
                           : fallbackTangentFromNeighbors(id, adjacency[id], mesh);
        } else {
          vf.tangent = fallbackTangentFromNeighbors(id, adjacency[id], mesh);
        }
      } else if (!alreadyFeature || vf.tangent.norm() <= 1e-20) {
        vf.primitive = loop.primitive;
        vf.tangent = fallbackTangentFromNeighbors(id, adjacency[id], mesh);
      }
    }
  };

  struct TraceLoopStats {
    int edgeCount = 0;
    int boundaryEdges = 0;
    int convexEdges = 0;
    int concaveEdges = 0;
    int unknownSignedEdges = 0;
    bool closed = false;
  };

  auto addLoop = [&](std::vector<int> vertices, const TraceLoopStats& stats) {
    if (vertices.empty() || stats.edgeCount <= 0) {
      return;
    }

    FeatureLoop loop;
    loop.id = loopId++;
    loop.vertices = std::move(vertices);
    loop.edgeCount = stats.edgeCount;
    loop.closed = stats.closed;
    loop.mostlyBoundary =
        loop.edgeCount > 0 &&
        stats.boundaryEdges >=
            static_cast<int>(0.6 * static_cast<double>(loop.edgeCount));
    loop.convexEdges = stats.convexEdges;
    loop.concaveEdges = stats.concaveEdges;
    loop.unknownSignedEdges = stats.unknownSignedEdges;
    if (loop.closed &&
        static_cast<int>(loop.vertices.size()) >= options.minFeatureLoopVertices) {
      applyPrimitiveFit(fitPrimitive(mesh, loop, options), loop);
    }

    assignLoopToVertices(loop);
    analysis.loops.push_back(std::move(loop));
  };

  auto cycleSignature = [](const std::vector<int>& vertices) {
    std::vector<std::uint64_t> keys;
    keys.reserve(vertices.size());
    for (int i = 0; i < static_cast<int>(vertices.size()); ++i) {
      keys.push_back(edgeKey(vertices[i], vertices[(i + 1) % vertices.size()]));
    }
    std::sort(keys.begin(), keys.end());
    std::ostringstream out;
    for (std::uint64_t key : keys) {
      out << key << ';';
    }
    return out.str();
  };

  auto cycleHasUniqueVertices = [](const std::vector<int>& vertices) {
    std::unordered_set<int> seen;
    seen.reserve(vertices.size());
    for (int id : vertices) {
      if (!seen.insert(id).second) {
        return false;
      }
    }
    return true;
  };

  auto cycleEdgesFollowCircle = [&](const std::vector<int>& vertices,
                                    const PrimitiveFit& fit) {
    if (!fit.valid ||
        (fit.primitive != FeaturePrimitiveType::Circle &&
         fit.primitive != FeaturePrimitiveType::NearCircle) ||
        fit.radius <= 1e-20 || fit.normal.norm() <= 1e-20) {
      return false;
    }

    Vec3 normal = fit.normal.normalized();
    const double allowed =
        std::max(3.0 * options.circleFitRelativeThreshold, 0.08) * fit.radius;
    for (int i = 0; i < static_cast<int>(vertices.size()); ++i) {
      const Vec3 midpoint = 0.5 * (mesh.vertices[vertices[i]] +
                                   mesh.vertices[vertices[(i + 1) % vertices.size()]]);
      const Vec3 delta = midpoint - fit.center;
      const double plane = std::abs(delta.dot(normal));
      const Vec3 inPlane = delta - normal * delta.dot(normal);
      const double radial = std::abs(inPlane.norm() - fit.radius);
      if (std::max(radial, plane) > allowed) {
        return false;
      }
    }
    return true;
  };

  auto addCircularCycle = [&](std::vector<int> vertices,
                              std::unordered_set<std::string>& seenCycles) {
    if (static_cast<int>(vertices.size()) < options.minFeatureLoopVertices ||
        !cycleHasUniqueVertices(vertices)) {
      return;
    }
    const std::string signature = cycleSignature(vertices);
    if (!seenCycles.insert(signature).second) {
      return;
    }

    FeatureLoop loop;
    loop.id = loopId++;
    loop.vertices = std::move(vertices);
    loop.edgeCount = static_cast<int>(loop.vertices.size());
    loop.closed = true;
    int boundaryEdges = 0;
    for (int i = 0; i < static_cast<int>(loop.vertices.size()); ++i) {
      const int a = loop.vertices[i];
      const int b = loop.vertices[(i + 1) % loop.vertices.size()];
      if (edgeBoundary(a, b)) {
        ++boundaryEdges;
      }
      const int sign = edgeSign(a, b);
      if (sign > 0) ++loop.convexEdges;
      if (sign < 0) ++loop.concaveEdges;
      if (sign == 0 && !edgeBoundary(a, b)) ++loop.unknownSignedEdges;
    }
    loop.mostlyBoundary =
        boundaryEdges >= static_cast<int>(0.6 * static_cast<double>(loop.edgeCount));

    const PrimitiveFit fit = fitPrimitive(mesh, loop, options);
    applyPrimitiveFit(fit, loop);
    if (!loop.circular || !cycleEdgesFollowCircle(loop.vertices, fit)) {
      --loopId;
      return;
    }
    assignLoopToVertices(loop);
    analysis.loops.push_back(std::move(loop));
  };

  auto addGraphCycle = [&](std::vector<int> vertices,
                           std::unordered_set<std::string>& seenCycles) {
    if (static_cast<int>(vertices.size()) < options.minFeatureLoopVertices ||
        !cycleHasUniqueVertices(vertices)) {
      return;
    }
    const std::string signature = cycleSignature(vertices);
    if (!seenCycles.insert(signature).second) {
      return;
    }

    FeatureLoop loop;
    loop.id = loopId++;
    loop.vertices = std::move(vertices);
    loop.edgeCount = static_cast<int>(loop.vertices.size());
    loop.closed = true;
    int boundaryEdges = 0;
    for (int i = 0; i < static_cast<int>(loop.vertices.size()); ++i) {
      const int a = loop.vertices[i];
      const int b = loop.vertices[(i + 1) % loop.vertices.size()];
      if (edgeBoundary(a, b)) {
        ++boundaryEdges;
      }
      const int sign = edgeSign(a, b);
      if (sign > 0) ++loop.convexEdges;
      if (sign < 0) ++loop.concaveEdges;
      if (sign == 0 && !edgeBoundary(a, b)) ++loop.unknownSignedEdges;
    }
    loop.mostlyBoundary =
        boundaryEdges >= static_cast<int>(0.6 * static_cast<double>(loop.edgeCount));

    const PrimitiveFit fit = fitPrimitive(mesh, loop, options);
    applyPrimitiveFit(fit, loop);
    if (!fit.valid || loop.primitive != FeaturePrimitiveType::PolygonalLoop) {
      --loopId;
      return;
    }
    assignLoopToVertices(loop);
    analysis.loops.push_back(std::move(loop));
  };

  struct FeatureChain {
    std::vector<int> vertices;
    int loEndpoint = -1;
    int hiEndpoint = -1;
  };

  auto traceJunctionChains = [&]() {
    std::vector<FeatureChain> chains;
    std::vector<char> isJunction(adjacency.size(), 0);
    for (int i = 0; i < static_cast<int>(adjacency.size()); ++i) {
      isJunction[i] = !adjacency[i].empty() && adjacency[i].size() != 2;
    }

    std::unordered_set<std::string> seenChains;
    for (int seed = 0; seed < static_cast<int>(adjacency.size()); ++seed) {
      if (!isJunction[seed]) {
        continue;
      }
      for (int nb : adjacency[seed]) {
        std::vector<int> chain;
        chain.push_back(seed);
        int previous = seed;
        int current = nb;
        while (true) {
          chain.push_back(current);
          if (current != seed && isJunction[current]) {
            break;
          }
          if (adjacency[current].size() != 2) {
            break;
          }
          int next = -1;
          for (int candidate : adjacency[current]) {
            if (candidate != previous) {
              next = candidate;
              break;
            }
          }
          if (next < 0) {
            break;
          }
          previous = current;
          current = next;
        }

        const int end = chain.back();
        if (end == seed || !isJunction[end]) {
          continue;
        }
        const std::string signature = cycleSignature(chain);
        if (!seenChains.insert(signature).second) {
          continue;
        }
        FeatureChain featureChain;
        featureChain.loEndpoint = std::min(seed, end);
        featureChain.hiEndpoint = std::max(seed, end);
        if (seed == featureChain.loEndpoint) {
          featureChain.vertices = std::move(chain);
        } else {
          featureChain.vertices.assign(chain.rbegin(), chain.rend());
        }
        chains.push_back(std::move(featureChain));
      }
    }
    return chains;
  };

  auto recoverCircularCyclesThroughJunctions = [&]() {
    const std::vector<FeatureChain> chains = traceJunctionChains();
    std::unordered_set<std::string> seenCycles;
    for (int i = 0; i < static_cast<int>(chains.size()); ++i) {
      for (int j = i + 1; j < static_cast<int>(chains.size()); ++j) {
        if (chains[i].loEndpoint != chains[j].loEndpoint ||
            chains[i].hiEndpoint != chains[j].hiEndpoint) {
          continue;
        }
        std::vector<int> cycle = chains[i].vertices;
        for (int k = static_cast<int>(chains[j].vertices.size()) - 2; k > 0; --k) {
          cycle.push_back(chains[j].vertices[k]);
        }
        addCircularCycle(std::move(cycle), seenCycles);
      }
    }
  };

  recoverCircularCyclesThroughJunctions();

  auto recoverSmallCycleBasis = [&]() {
    if (analysis.normalTensorFeatureEdges > 0) {
      return;
    }

    std::vector<char> componentVisited(mesh.vertices.size(), 0);
    std::unordered_set<std::string> seenCycles;
    for (const FeatureLoop& loop : analysis.loops) {
      if (loop.closed) {
        seenCycles.insert(cycleSignature(loop.vertices));
      }
    }

    constexpr int kMaxCycleComponentVertices = 160;
    constexpr int kMaxCycleComponentEdges = 240;
    constexpr int kMaxCycleRank = 32;
    constexpr int kMaxCycleVertices = 80;
    std::vector<int> parent(mesh.vertices.size(), -1);
    std::vector<int> depth(mesh.vertices.size(), 0);
    for (int seed = 0; seed < static_cast<int>(adjacency.size()); ++seed) {
      if (componentVisited[seed] || adjacency[seed].empty()) {
        continue;
      }

      std::vector<int> component;
      std::queue<int> queue;
      queue.push(seed);
      componentVisited[seed] = 1;
      parent[seed] = seed;
      depth[seed] = 0;
      int edgeCount2x = 0;
      std::unordered_set<std::uint64_t> treeEdges;
      while (!queue.empty()) {
        const int v = queue.front();
        queue.pop();
        component.push_back(v);
        edgeCount2x += static_cast<int>(adjacency[v].size());
        for (int nb : adjacency[v]) {
          if (!componentVisited[nb]) {
            componentVisited[nb] = 1;
            parent[nb] = v;
            depth[nb] = depth[v] + 1;
            treeEdges.insert(edgeKey(v, nb));
            queue.push(nb);
          }
        }
      }

      const int edgeCount = edgeCount2x / 2;
      const int cycleRank = edgeCount - static_cast<int>(component.size()) + 1;
      if (static_cast<int>(component.size()) < options.minFeatureLoopVertices ||
          static_cast<int>(component.size()) > kMaxCycleComponentVertices ||
          edgeCount > kMaxCycleComponentEdges || cycleRank <= 0 ||
          cycleRank > kMaxCycleRank) {
        continue;
      }

      std::sort(component.begin(), component.end());
      auto treePathCycle = [&](int u, int v) {
        std::vector<int> uPath;
        std::vector<int> vPath;
        int a = u;
        int b = v;
        while (a != b) {
          if (depth[a] >= depth[b]) {
            uPath.push_back(a);
            a = parent[a];
          } else {
            vPath.push_back(b);
            b = parent[b];
          }
          if (a < 0 || b < 0) {
            return std::vector<int>{};
          }
        }
        uPath.push_back(a);
        std::vector<int> cycle = std::move(uPath);
        for (auto it = vPath.rbegin(); it != vPath.rend(); ++it) {
          cycle.push_back(*it);
        }
        return cycle;
      };

      for (int v : component) {
        std::vector<int> neighbors = adjacency[v];
        std::sort(neighbors.begin(), neighbors.end());
        for (int nb : neighbors) {
          if (v >= nb || treeEdges.find(edgeKey(v, nb)) != treeEdges.end()) {
            continue;
          }
          std::vector<int> cycle = treePathCycle(v, nb);
          if (static_cast<int>(cycle.size()) <= kMaxCycleVertices) {
            addGraphCycle(std::move(cycle), seenCycles);
          }
        }
      }
    }
  };

  recoverSmallCycleBasis();

  auto recoverCircularVertexClusters = [&]() {
    if (analysis.normalTensorFeatureEdges > 0) {
      return;
    }

    const bool alreadyHasCircularLoop =
        std::any_of(analysis.loops.begin(), analysis.loops.end(),
                    [](const FeatureLoop& loop) { return loop.circular; });
    if (alreadyHasCircularLoop) {
      return;
    }

    std::vector<int> candidates;
    for (int id = 0; id < static_cast<int>(traceVertex.size()); ++id) {
      if (traceVertex[id]) {
        candidates.push_back(id);
      }
    }
    if (static_cast<int>(candidates.size()) < options.minFeatureLoopVertices ||
        candidates.size() > 120) {
      return;
    }

    auto vertexSetSignature = [](std::vector<int> ids) {
      std::sort(ids.begin(), ids.end());
      std::ostringstream out;
      for (int id : ids) {
        out << id << ';';
      }
      return out.str();
    };

    struct ThreePointCircle {
      bool valid = false;
      Vec3 center = Vec3::Zero();
      Vec3 normal = Vec3(0.0, 0.0, 1.0);
      double radius = 0.0;
    };

    auto fitCircleFromThree = [&](int ia, int ib, int ic) {
      ThreePointCircle result;
      const Vec3& a = mesh.vertices[ia];
      const Vec3& b = mesh.vertices[ib];
      const Vec3& c = mesh.vertices[ic];
      const Vec3 ab = b - a;
      const Vec3 ac = c - a;
      result.normal = ab.cross(ac);
      const double n2 = result.normal.squaredNorm();
      if (n2 <= 1e-20) {
        return result;
      }
      const Vec3 term1 = ac.squaredNorm() * result.normal.cross(ab);
      const Vec3 term2 = ab.squaredNorm() * ac.cross(result.normal);
      result.center = a + (term1 + term2) / (2.0 * n2);
      result.radius = (result.center - a).norm();
      result.normal.normalize();
      result.valid = std::isfinite(result.radius) && result.radius > 1e-20 &&
                     result.center.allFinite();
      return result;
    };

    auto sortAroundCircle = [&](std::vector<int> ids, const Vec3& center,
                                const Vec3& normal) {
      Vec3 axisX = mesh.vertices[ids.front()] - center;
      axisX -= normal * axisX.dot(normal);
      if (axisX.norm() <= 1e-20) {
        axisX = std::abs(normal.x()) < 0.9 ? Vec3(1.0, 0.0, 0.0) : Vec3(0.0, 1.0, 0.0);
        axisX -= normal * axisX.dot(normal);
      }
      axisX.normalize();
      const Vec3 axisY = normal.cross(axisX).normalized();
      std::sort(ids.begin(), ids.end(), [&](int lhs, int rhs) {
        const Vec3 dl = mesh.vertices[lhs] - center;
        const Vec3 dr = mesh.vertices[rhs] - center;
        const double al = std::atan2(dl.dot(axisY), dl.dot(axisX));
        const double ar = std::atan2(dr.dot(axisY), dr.dot(axisX));
        return al < ar;
      });
      return ids;
    };

    auto angularCoverage = [&](const std::vector<int>& ids, const Vec3& center,
                               const Vec3& normal) {
      Vec3 axisX = mesh.vertices[ids.front()] - center;
      axisX -= normal * axisX.dot(normal);
      if (axisX.norm() <= 1e-20) {
        return 0.0;
      }
      axisX.normalize();
      const Vec3 axisY = normal.cross(axisX).normalized();
      std::vector<double> angles;
      angles.reserve(ids.size());
      for (int id : ids) {
        const Vec3 d = mesh.vertices[id] - center;
        double angle = std::atan2(d.dot(axisY), d.dot(axisX));
        if (angle < 0.0) {
          angle += 2.0 * kPi;
        }
        angles.push_back(angle);
      }
      std::sort(angles.begin(), angles.end());
      double maxGap = 0.0;
      for (int i = 0; i < static_cast<int>(angles.size()); ++i) {
        const double a = angles[i];
        const double b = i + 1 < static_cast<int>(angles.size())
                             ? angles[i + 1]
                             : angles.front() + 2.0 * kPi;
        maxGap = std::max(maxGap, b - a);
      }
      return 2.0 * kPi - maxGap;
    };

    std::unordered_set<std::string> seenClusters;
    for (int i = 0; i < static_cast<int>(candidates.size()); ++i) {
      for (int j = i + 1; j < static_cast<int>(candidates.size()); ++j) {
        for (int k = j + 1; k < static_cast<int>(candidates.size()); ++k) {
          const ThreePointCircle circle =
              fitCircleFromThree(candidates[i], candidates[j], candidates[k]);
          if (!circle.valid) {
            continue;
          }
          const double allowed =
              std::max(3.0 * options.circleFitRelativeThreshold, 0.08) *
              circle.radius;
          std::vector<int> cluster;
          for (int id : candidates) {
            const Vec3 delta = mesh.vertices[id] - circle.center;
            const double plane = std::abs(delta.dot(circle.normal));
            const Vec3 inPlane = delta - circle.normal * delta.dot(circle.normal);
            const double radial = std::abs(inPlane.norm() - circle.radius);
            if (std::max(plane, radial) <= allowed) {
              cluster.push_back(id);
            }
          }
          if (static_cast<int>(cluster.size()) < options.minFeatureLoopVertices ||
              angularCoverage(cluster, circle.center, circle.normal) < 1.5 * kPi) {
            continue;
          }
          const std::string signature = vertexSetSignature(cluster);
          if (!seenClusters.insert(signature).second) {
            continue;
          }

          cluster = sortAroundCircle(std::move(cluster), circle.center,
                                     circle.normal);
          FeatureLoop loop;
          loop.id = loopId++;
          loop.vertices = std::move(cluster);
          loop.edgeCount = static_cast<int>(loop.vertices.size());
          loop.closed = true;
          const PrimitiveFit fit = fitPrimitive(mesh, loop, options);
          applyPrimitiveFit(fit, loop);
          if (!loop.circular) {
            --loopId;
            continue;
          }
          assignLoopToVertices(loop);
          analysis.loops.push_back(std::move(loop));
        }
      }
    }
  };

  auto traceOpenChain = [&](int seed, int firstNeighbor) {
    std::vector<int> vertices;
    vertices.push_back(seed);
    TraceLoopStats stats;

    int previous = seed;
    int current = firstNeighbor;
    while (true) {
      if (edgeVisited(previous, current)) {
        break;
      }
      markEdge(previous, current);
      ++stats.edgeCount;
      if (edgeBoundary(previous, current)) {
        ++stats.boundaryEdges;
      }
      const int sign = edgeSign(previous, current);
      if (sign > 0) ++stats.convexEdges;
      if (sign < 0) ++stats.concaveEdges;
      if (sign == 0 && !edgeBoundary(previous, current)) {
        ++stats.unknownSignedEdges;
      }
      vertices.push_back(current);
      if (current == seed) {
        vertices.pop_back();
        stats.closed = true;
        break;
      }

      if (adjacency[current].size() != 2) {
        break;
      }

      int next = -1;
      for (int candidate : adjacency[current]) {
        if (candidate != previous && !edgeVisited(current, candidate)) {
          next = candidate;
          break;
        }
      }
      if (next < 0) {
        break;
      }
      previous = current;
      current = next;
    }

    addLoop(std::move(vertices), stats);
  };

  auto traceClosedLoop = [&](int seed, int firstNeighbor) {
    std::vector<int> vertices;
    vertices.push_back(seed);
    TraceLoopStats stats;
    int previous = seed;
    int current = firstNeighbor;

    while (true) {
      if (edgeVisited(previous, current)) {
        break;
      }
      markEdge(previous, current);
      ++stats.edgeCount;
      if (edgeBoundary(previous, current)) {
        ++stats.boundaryEdges;
      }
      const int sign = edgeSign(previous, current);
      if (sign > 0) ++stats.convexEdges;
      if (sign < 0) ++stats.concaveEdges;
      if (sign == 0 && !edgeBoundary(previous, current)) {
        ++stats.unknownSignedEdges;
      }
      if (current == seed) {
        stats.closed = true;
        break;
      }
      vertices.push_back(current);

      int next = -1;
      for (int candidate : adjacency[current]) {
        if (candidate != previous) {
          next = candidate;
          break;
        }
      }
      if (next < 0) {
        break;
      }
      previous = current;
      current = next;
    }

    addLoop(std::move(vertices), stats);
  };

  for (int seed = 0; seed < static_cast<int>(adjacency.size()); ++seed) {
    if (adjacency[seed].empty() || adjacency[seed].size() == 2) {
      continue;
    }
    for (int nb : adjacency[seed]) {
      if (!edgeVisited(seed, nb)) {
        traceOpenChain(seed, nb);
      }
    }
  }

  for (const auto& [a, b] : graphEdges) {
    if (!edgeVisited(a, b)) {
      traceClosedLoop(a, b);
    }
  }

  std::vector<char> componentVisited(mesh.vertices.size(), 0);
  for (int seed = 0; seed < static_cast<int>(adjacency.size()); ++seed) {
    if (componentVisited[seed] || adjacency[seed].empty()) {
      continue;
    }

    std::vector<int> component;
    std::queue<int> queue;
    queue.push(seed);
    componentVisited[seed] = 1;
    while (!queue.empty()) {
      const int v = queue.front();
      queue.pop();
      component.push_back(v);
      for (int nb : adjacency[v]) {
        if (!componentVisited[nb]) {
          componentVisited[nb] = 1;
          queue.push(nb);
        }
      }
    }

    bool alreadyHasCircular = false;
    int edgeCount2x = 0;
    int boundaryEdges = 0;
    int convexEdges = 0;
    int concaveEdges = 0;
    int unknownSignedEdges = 0;
    for (int v : component) {
      alreadyHasCircular = alreadyHasCircular || analysis.vertices[v].circular;
      edgeCount2x += static_cast<int>(adjacency[v].size());
      for (int nb : adjacency[v]) {
        if (v < nb && edgeBoundary(v, nb)) {
          ++boundaryEdges;
        }
        if (v < nb) {
          const int sign = edgeSign(v, nb);
          if (sign > 0) ++convexEdges;
          if (sign < 0) ++concaveEdges;
          if (sign == 0 && !edgeBoundary(v, nb)) ++unknownSignedEdges;
        }
      }
    }
    const int edgeCount = edgeCount2x / 2;
    if (alreadyHasCircular ||
        static_cast<int>(component.size()) < options.minFeatureLoopVertices ||
        edgeCount < static_cast<int>(component.size()) ||
        edgeCount > static_cast<int>(component.size()) * 3) {
      continue;
    }

    FeatureLoop loop;
    loop.id = loopId++;
    loop.vertices = std::move(component);
    loop.edgeCount = edgeCount;
    loop.closed = true;
    loop.mostlyBoundary =
        loop.edgeCount > 0 &&
        boundaryEdges >= static_cast<int>(0.6 * static_cast<double>(loop.edgeCount));
    loop.convexEdges = convexEdges;
    loop.concaveEdges = concaveEdges;
    loop.unknownSignedEdges = unknownSignedEdges;
    applyPrimitiveFit(fitPrimitive(mesh, loop, options), loop);
    if (loop.primitive != FeaturePrimitiveType::Circle &&
        loop.primitive != FeaturePrimitiveType::NearCircle &&
        loop.primitive != FeaturePrimitiveType::Ellipse) {
      continue;
    }
    assignLoopToVertices(loop);
    analysis.loops.push_back(std::move(loop));
  }

  recoverCircularVertexClusters();

  analysis.graph.junctionVertices.clear();
  analysis.graph.sharedVertices.clear();
  for (int id = 0; id < static_cast<int>(analysis.graph.vertices.size()); ++id) {
    FeatureGraphVertex& vertex = analysis.graph.vertices[id];
    vertex.junction = vertex.incidentEdges.size() != 2 || vertex.loopIds.size() > 1 ||
                      (id < static_cast<int>(analysis.vertices.size()) &&
                       analysis.vertices[id].junction);
    vertex.shared = vertex.loopIds.size() > 1;
    if (vertex.junction && !vertex.incidentEdges.empty()) {
      analysis.graph.junctionVertices.push_back(id);
    }
    if (vertex.shared) {
      analysis.graph.sharedVertices.push_back(id);
    }
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
  return "loop_id,vertices,edges,closed,primitive,circular,mostly_boundary,cx,cy,cz,"
         "nx,ny,nz,major_axis_x,major_axis_y,major_axis_z,minor_axis_x,"
         "minor_axis_y,minor_axis_z,radius,major_radius,minor_radius,axis_ratio,"
         "rms_radial,max_radial,rms_ellipse,max_ellipse,rms_plane,max_plane,"
         "convex_edges,concave_edges,unknown_signed_edges";
}

std::string featureLoopRowCsv(const FeatureLoop& loop) {
  std::ostringstream out;
  out << std::setprecision(12);
  out << loop.id << "," << loop.vertices.size() << "," << loop.edgeCount << ","
      << (loop.closed ? 1 : 0) << "," << toString(loop.primitive) << ","
      << (loop.circular ? 1 : 0) << "," << (loop.mostlyBoundary ? 1 : 0) << ","
      << loop.center.x() << "," << loop.center.y() << "," << loop.center.z() << ","
      << loop.normal.x() << "," << loop.normal.y() << "," << loop.normal.z() << ","
      << loop.majorAxis.x() << "," << loop.majorAxis.y() << "," << loop.majorAxis.z()
      << "," << loop.minorAxis.x() << "," << loop.minorAxis.y() << ","
      << loop.minorAxis.z() << "," << loop.radius << "," << loop.majorRadius << ","
      << loop.minorRadius << "," << loop.axisRatio << "," << loop.rmsRadialError << ","
      << loop.maxRadialError << "," << loop.rmsEllipseError << ","
      << loop.maxEllipseError << "," << loop.rmsPlaneError << "," << loop.maxPlaneError
      << "," << loop.convexEdges << "," << loop.concaveEdges << ","
      << loop.unknownSignedEdges;
  return out.str();
}

std::string toString(FeaturePrimitiveType primitive) {
  switch (primitive) {
  case FeaturePrimitiveType::Unknown:
    return "unknown";
  case FeaturePrimitiveType::Circle:
    return "circle";
  case FeaturePrimitiveType::NearCircle:
    return "near-circle";
  case FeaturePrimitiveType::Ellipse:
    return "ellipse";
  case FeaturePrimitiveType::PolygonalLoop:
    return "polygonal-loop";
  }
  return "unknown";
}

} // namespace lq
