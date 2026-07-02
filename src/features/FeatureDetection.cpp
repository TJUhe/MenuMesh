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
                mesh, NormalTensorOptions{options.normalTensorSmoothingIterations})
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
      edge.dihedral = std::acos(dot) >= threshold;
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
    const double ellipse =
        (std::sqrt((ex * ex) / (majorRadius * majorRadius) +
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
  const int iterations = std::clamp(options.smoothingIterations, 0, 8);
  for (int iter = 0; iter < iterations; ++iter) {
    std::vector<Eigen::Matrix3d> next = tensors;
    for (int i = 0; i < static_cast<int>(tensors.size()); ++i) {
      if (neighbors[i].empty()) {
        continue;
      }
      Eigen::Matrix3d sum = tensors[i];
      double count = 1.0;
      for (int nb : neighbors[i]) {
        sum += tensors[nb];
        count += 1.0;
      }
      next[i] = sum / count;
    }
    tensors.swap(next);
  }

  for (int i = 0; i < static_cast<int>(tensors.size()); ++i) {
    result[i] = analyzeNormalTensor(tensors[i]);
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

  std::vector<std::vector<int>> adjacency(mesh.vertices.size());
  std::unordered_map<std::uint64_t, bool> edgeIsBoundary;
  edgeIsBoundary.reserve(featureEdges.size());
  std::unordered_map<std::uint64_t, int> edgeSignedKind;
  edgeSignedKind.reserve(featureEdges.size());
  std::vector<std::pair<int, int>> graphEdges;
  graphEdges.reserve(featureEdges.size());
  for (const CandidateEdge& edge : featureEdges) {
    adjacency[edge.a].push_back(edge.b);
    adjacency[edge.b].push_back(edge.a);
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
      const bool alreadyFeature = vf.isFeature;
      if (!alreadyFeature) {
        vf.isFeature = true;
        vf.loopId = loop.id;
        vf.circular = loop.circular;
      }
      vf.junction = vf.junction || alreadyFeature || adjacency[id].size() != 2;
      if (loop.circular && (!alreadyFeature || !vf.circular)) {
        vf.loopId = loop.id;
        vf.circular = true;
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
      } else if (!alreadyFeature || vf.tangent.norm() <= 1e-20) {
        vf.tangent = fallbackTangentFromNeighbors(id, adjacency[id], mesh);
      }
    }
  };

  auto addLoop = [&](std::vector<int> vertices, int edgeCount, int boundaryEdges,
                     int convexEdges, int concaveEdges, int unknownSignedEdges,
                     bool closed) {
    if (vertices.empty() || edgeCount <= 0) {
      return;
    }

    FeatureLoop loop;
    loop.id = loopId++;
    loop.vertices = std::move(vertices);
    loop.edgeCount = edgeCount;
    loop.closed = closed;
    loop.mostlyBoundary =
        loop.edgeCount > 0 &&
        boundaryEdges >= static_cast<int>(0.6 * static_cast<double>(loop.edgeCount));
    loop.convexEdges = convexEdges;
    loop.concaveEdges = concaveEdges;
    loop.unknownSignedEdges = unknownSignedEdges;
    if (loop.closed &&
        static_cast<int>(loop.vertices.size()) >= options.minFeatureLoopVertices) {
      applyPrimitiveFit(fitPrimitive(mesh, loop, options), loop);
    }

    assignLoopToVertices(loop);
    analysis.loops.push_back(std::move(loop));
  };

  auto traceOpenChain = [&](int seed, int firstNeighbor) {
    std::vector<int> vertices;
    vertices.push_back(seed);
    int edgeCount = 0;
    int boundaryEdges = 0;
    int convexEdges = 0;
    int concaveEdges = 0;
    int unknownSignedEdges = 0;
    bool closed = false;

    int previous = seed;
    int current = firstNeighbor;
    while (true) {
      if (edgeVisited(previous, current)) {
        break;
      }
      markEdge(previous, current);
      ++edgeCount;
      if (edgeBoundary(previous, current)) {
        ++boundaryEdges;
      }
      const int sign = edgeSign(previous, current);
      if (sign > 0) ++convexEdges;
      if (sign < 0) ++concaveEdges;
      if (sign == 0 && !edgeBoundary(previous, current)) ++unknownSignedEdges;
      vertices.push_back(current);
      if (current == seed) {
        vertices.pop_back();
        closed = true;
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

    addLoop(std::move(vertices), edgeCount, boundaryEdges, convexEdges, concaveEdges,
            unknownSignedEdges, closed);
  };

  auto traceClosedLoop = [&](int seed, int firstNeighbor) {
    std::vector<int> vertices;
    vertices.push_back(seed);
    int edgeCount = 0;
    int boundaryEdges = 0;
    int convexEdges = 0;
    int concaveEdges = 0;
    int unknownSignedEdges = 0;
    int previous = seed;
    int current = firstNeighbor;
    bool closed = false;

    while (true) {
      if (edgeVisited(previous, current)) {
        break;
      }
      markEdge(previous, current);
      ++edgeCount;
      if (edgeBoundary(previous, current)) {
        ++boundaryEdges;
      }
      const int sign = edgeSign(previous, current);
      if (sign > 0) ++convexEdges;
      if (sign < 0) ++concaveEdges;
      if (sign == 0 && !edgeBoundary(previous, current)) ++unknownSignedEdges;
      if (current == seed) {
        closed = true;
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

    addLoop(std::move(vertices), edgeCount, boundaryEdges, convexEdges, concaveEdges,
            unknownSignedEdges, closed);
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
      << loop.majorAxis.x() << "," << loop.majorAxis.y() << ","
      << loop.majorAxis.z() << "," << loop.minorAxis.x() << ","
      << loop.minorAxis.y() << "," << loop.minorAxis.z() << "," << loop.radius
      << "," << loop.majorRadius << "," << loop.minorRadius << ","
      << loop.axisRatio << "," << loop.rmsRadialError << ","
      << loop.maxRadialError << "," << loop.rmsEllipseError << ","
      << loop.maxEllipseError << "," << loop.rmsPlaneError << ","
      << loop.maxPlaneError << "," << loop.convexEdges << "," << loop.concaveEdges
      << "," << loop.unknownSignedEdges;
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
