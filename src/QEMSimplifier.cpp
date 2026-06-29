#include "QEMSimplifier.h"

#include <Eigen/Eigenvalues>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <queue>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace lq {
namespace {

constexpr double kPi = 3.141592653589793238462643383279502884;

struct EdgeInfo {
  std::vector<int> faces;
};

struct VertexState {
  Vec3 p = Vec3::Zero();
  Mat4 q = Mat4::Zero();
  bool active = true;
  int version = 0;
};

struct FaceState {
  std::array<int, 3> v{};
  bool active = true;
};

struct Candidate {
  double cost = 0.0;
  int a = -1;
  int b = -1;
  int versionA = 0;
  int versionB = 0;

  bool operator<(const Candidate& other) const {
    return cost > other.cost;
  }
};

struct SolveResult {
  Vec3 position = Vec3::Zero();
  double cost = 0.0;
  bool usedFallback = false;
};

std::uint64_t edgeKey(int a, int b) {
  if (a > b) std::swap(a, b);
  return (static_cast<std::uint64_t>(static_cast<uint32_t>(a)) << 32u) |
         static_cast<uint32_t>(b);
}

std::pair<int, int> unpackEdgeKey(std::uint64_t key) {
  return {static_cast<int>(key >> 32u), static_cast<int>(key & 0xffffffffu)};
}

double evaluateQuadric(const Mat4& q, const Vec3& p) {
  Eigen::Vector4d h;
  h << p.x(), p.y(), p.z(), 1.0;
  return h.dot(q * h);
}

Mat4 planeQuadric(const Vec3& normal, const Vec3& point) {
  Eigen::Vector4d plane;
  plane << normal.x(), normal.y(), normal.z(), -normal.dot(point);
  return plane * plane.transpose();
}

Mat4 pointQuadric(const Vec3& point) {
  Mat4 q = Mat4::Zero();
  q.block<3, 3>(0, 0).setIdentity();
  q.block<3, 1>(0, 3) = -point;
  q.block<1, 3>(3, 0) = -point.transpose();
  q(3, 3) = point.squaredNorm();
  return q;
}

Mat4 lineQuadric(const Vec3& point, const Vec3& normal) {
  Vec3 n = normal;
  const double nlen = n.norm();
  if (nlen <= 1e-20) {
    return pointQuadric(point);
  }
  n /= nlen;

  Vec3 seed = std::abs(n.x()) < 0.9 ? Vec3(1.0, 0.0, 0.0)
                                    : Vec3(0.0, 1.0, 0.0);
  Vec3 x = seed - n * n.dot(seed);
  const double xlen = x.norm();
  if (xlen <= 1e-20) {
    seed = Vec3(0.0, 0.0, 1.0);
    x = seed - n * n.dot(seed);
  }
  x.normalize();
  Vec3 y = n.cross(x).normalized();

  return planeQuadric(x, point) + planeQuadric(y, point);
}

std::unordered_map<std::uint64_t, EdgeInfo> buildEdgeInfo(
    const Mesh& mesh, const std::vector<Vec3>* faceNormals = nullptr) {
  (void)faceNormals;
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

std::vector<double> computeFeatureScores(const Mesh& mesh, WeightMode mode,
                                         double angleDeg) {
  std::vector<double> score(mesh.vertices.size(), 0.0);
  if (mode == WeightMode::Uniform) {
    return score;
  }

  const Vec3 lo = mesh.bboxMin();
  const Vec3 hi = mesh.bboxMax();
  const Vec3 span = hi - lo;

  if (mode == WeightMode::Height) {
    const double denom = std::max(1e-12, span.z());
    for (int i = 0; i < static_cast<int>(mesh.vertices.size()); ++i) {
      score[i] = std::clamp((mesh.vertices[i].z() - lo.z()) / denom, 0.0, 1.0);
    }
    return score;
  }

  if (mode == WeightMode::XBand) {
    const double denom = std::max(1e-12, span.x());
    for (int i = 0; i < static_cast<int>(mesh.vertices.size()); ++i) {
      const double x = (mesh.vertices[i].x() - lo.x()) / denom;
      score[i] = std::exp(-80.0 * (x - 0.5) * (x - 0.5));
    }
    return score;
  }

  std::vector<Vec3> faceNormals(mesh.faces.size(), Vec3::Zero());
  for (int fi = 0; fi < static_cast<int>(mesh.faces.size()); ++fi) {
    const Face& f = mesh.faces[fi];
    faceNormals[fi] =
        triangleNormal(mesh.vertices[f.v[0]], mesh.vertices[f.v[1]],
                       mesh.vertices[f.v[2]]);
  }

  const auto edgeInfo = buildEdgeInfo(mesh);
  const double threshold = angleDeg * kPi / 180.0;
  const double denom = std::max(1e-12, kPi - threshold);
  for (const auto& [key, info] : edgeInfo) {
    double edgeScore = 0.0;
    if (info.faces.size() == 1) {
      edgeScore = 1.0;
    } else if (info.faces.size() == 2) {
      const double dot =
          std::clamp(faceNormals[info.faces[0]].dot(faceNormals[info.faces[1]]),
                     -1.0, 1.0);
      const double angle = std::acos(dot);
      edgeScore = std::clamp((angle - threshold) / denom, 0.0, 1.0);
    }
    if (edgeScore > 0.0) {
      const auto [a, b] = unpackEdgeKey(key);
      score[a] = std::max(score[a], edgeScore);
      score[b] = std::max(score[b], edgeScore);
    }
  }
  return score;
}

void addBoundaryQuadrics(const Mesh& mesh, double boundaryWeight,
                         std::vector<Mat4>& quadrics) {
  if (boundaryWeight <= 0.0) {
    return;
  }
  std::vector<Vec3> faceNormals(mesh.faces.size(), Vec3::Zero());
  for (int fi = 0; fi < static_cast<int>(mesh.faces.size()); ++fi) {
    const Face& f = mesh.faces[fi];
    faceNormals[fi] =
        triangleNormal(mesh.vertices[f.v[0]], mesh.vertices[f.v[1]],
                       mesh.vertices[f.v[2]]);
  }

  const auto edgeInfo = buildEdgeInfo(mesh);
  for (const auto& [key, info] : edgeInfo) {
    if (info.faces.size() != 1) {
      continue;
    }
    const auto [a, b] = unpackEdgeKey(key);
    const Vec3 edge = mesh.vertices[b] - mesh.vertices[a];
    if (edge.norm() <= 1e-20) {
      continue;
    }
    Vec3 n = faceNormals[info.faces.front()].cross(edge.normalized());
    if (n.norm() <= 1e-20) {
      continue;
    }
    n.normalize();
    const Mat4 q = boundaryWeight * edge.norm() * planeQuadric(n, mesh.vertices[a]);
    quadrics[a] += q;
    quadrics[b] += q;
  }
}

void computeInitialQuadrics(const Mesh& mesh, const SimplifyOptions& options,
                            std::vector<Mat4>& quadrics,
                            double& minLineWeight, double& maxLineWeight) {
  quadrics.assign(mesh.vertices.size(), Mat4::Zero());
  std::vector<double> vertexArea(mesh.vertices.size(), 0.0);
  std::vector<Vec3> normalSum(mesh.vertices.size(), Vec3::Zero());

  for (const Face& face : mesh.faces) {
    const Vec3& a = mesh.vertices[face.v[0]];
    const Vec3& b = mesh.vertices[face.v[1]];
    const Vec3& c = mesh.vertices[face.v[2]];
    const Vec3 n = triangleNormal(a, b, c);
    const double area = triangleArea(a, b, c);
    if (area <= 1e-24 || n.norm() <= 1e-20) {
      for (int id : face.v) {
        quadrics[id] += 1e-6 * pointQuadric(mesh.vertices[id]);
      }
      continue;
    }

    const Mat4 q = planeQuadric(n, a);
    for (int id : face.v) {
      const double baryArea = area / 3.0;
      vertexArea[id] += baryArea;
      normalSum[id] += area * n;
      quadrics[id] += baryArea * q;
    }
  }

  addBoundaryQuadrics(mesh, options.boundaryWeight, quadrics);

  minLineWeight = std::numeric_limits<double>::infinity();
  maxLineWeight = 0.0;
  if (!options.useLineQuadrics || options.lineWeight <= 0.0) {
    minLineWeight = 0.0;
    return;
  }

  const std::vector<double> featureScores =
      computeFeatureScores(mesh, options.weightMode, options.featureAngleDeg);

  for (int i = 0; i < static_cast<int>(mesh.vertices.size()); ++i) {
    Vec3 normal = normalSum[i];
    if (normal.norm() <= 1e-20 || vertexArea[i] <= 1e-24) {
      quadrics[i] += 1e-6 * pointQuadric(mesh.vertices[i]);
      continue;
    }
    normal.normalize();

    const Mat4 ql = lineQuadric(mesh.vertices[i], normal);
    double appliedWeight = options.lineWeight;
    if (options.weightMode != WeightMode::Uniform) {
      appliedWeight += options.featureBoost * featureScores[i];
    }

    if (options.adaptiveScale) {
      quadrics[i] += options.adaptiveBaseLineWeight * vertexArea[i] * ql;
      quadrics[i] *= (1.0 + std::max(0.0, options.featureBoost) * featureScores[i]);
    } else {
      quadrics[i] += appliedWeight * vertexArea[i] * ql;
    }
    minLineWeight = std::min(minLineWeight, appliedWeight);
    maxLineWeight = std::max(maxLineWeight, appliedWeight);
  }

  if (!std::isfinite(minLineWeight)) {
    minLineWeight = 0.0;
  }
}

SolveResult solveOptimal(const Mat4& q, const Vec3& a, const Vec3& b) {
  SolveResult result;
  std::vector<Vec3> candidates;
  candidates.reserve(4);
  candidates.push_back(a);
  candidates.push_back(b);
  candidates.push_back(0.5 * (a + b));

  const Eigen::Matrix3d A = q.block<3, 3>(0, 0);
  const Eigen::Vector3d rhs = -q.block<3, 1>(0, 3);
  Eigen::SelfAdjointEigenSolver<Eigen::Matrix3d> eig(A);
  bool solved = false;
  if (eig.info() == Eigen::Success) {
    const double maxEval = eig.eigenvalues().cwiseAbs().maxCoeff();
    const double minEval = eig.eigenvalues().cwiseAbs().minCoeff();
    if (maxEval > 1e-20 && minEval / maxEval > 1e-12) {
      const Vec3 x = A.ldlt().solve(rhs);
      if (x.allFinite()) {
        candidates.push_back(x);
        solved = true;
      }
    }
  }

  result.usedFallback = !solved;
  result.cost = std::numeric_limits<double>::infinity();
  for (const Vec3& p : candidates) {
    const double cost = evaluateQuadric(q, p);
    if (std::isfinite(cost) && cost < result.cost) {
      result.cost = cost;
      result.position = p;
    }
  }
  if (!std::isfinite(result.cost)) {
    result.position = 0.5 * (a + b);
    result.cost = 0.0;
    result.usedFallback = true;
  }
  return result;
}

std::vector<std::pair<int, int>> collectActiveEdges(
    const std::vector<FaceState>& faces) {
  std::unordered_set<std::uint64_t> seen;
  std::vector<std::pair<int, int>> edges;
  for (const FaceState& face : faces) {
    if (!face.active) {
      continue;
    }
    for (int e = 0; e < 3; ++e) {
      int a = face.v[e];
      int b = face.v[(e + 1) % 3];
      if (a == b) {
        continue;
      }
      const std::uint64_t key = edgeKey(a, b);
      if (seen.insert(key).second) {
        if (a > b) std::swap(a, b);
        edges.emplace_back(a, b);
      }
    }
  }
  return edges;
}

bool areAdjacent(int a, int b, const std::vector<FaceState>& faces) {
  for (const FaceState& face : faces) {
    if (!face.active) {
      continue;
    }
    bool hasA = false;
    bool hasB = false;
    for (int id : face.v) {
      hasA = hasA || id == a;
      hasB = hasB || id == b;
    }
    if (hasA && hasB) {
      return true;
    }
  }
  return false;
}

std::vector<int> activeNeighborsOf(int v, const std::vector<FaceState>& faces,
                                  const std::vector<VertexState>& vertices) {
  std::unordered_set<int> seen;
  for (const FaceState& face : faces) {
    if (!face.active) {
      continue;
    }
    bool contains = false;
    for (int id : face.v) {
      contains = contains || id == v;
    }
    if (!contains) {
      continue;
    }
    for (int id : face.v) {
      if (id != v && vertices[id].active) {
        seen.insert(id);
      }
    }
  }
  return std::vector<int>(seen.begin(), seen.end());
}

bool collapseWouldBeValid(int keep, int remove, const Vec3& newPosition,
                          const std::vector<FaceState>& faces,
                          const std::vector<VertexState>& vertices,
                          double areaEps) {
  for (const FaceState& face : faces) {
    if (!face.active) {
      continue;
    }
    bool touches = false;
    bool containsBoth = false;
    std::array<int, 3> mapped = face.v;
    for (int& id : mapped) {
      if (id == keep || id == remove) {
        touches = true;
      }
      if (id == remove) {
        id = keep;
      }
    }
    containsBoth = (face.v[0] == keep || face.v[1] == keep || face.v[2] == keep) &&
                   (face.v[0] == remove || face.v[1] == remove ||
                    face.v[2] == remove);
    if (!touches || containsBoth) {
      continue;
    }
    if (mapped[0] == mapped[1] || mapped[1] == mapped[2] ||
        mapped[0] == mapped[2]) {
      return false;
    }
    Vec3 a = vertices[mapped[0]].p;
    Vec3 b = vertices[mapped[1]].p;
    Vec3 c = vertices[mapped[2]].p;
    if (mapped[0] == keep) a = newPosition;
    if (mapped[1] == keep) b = newPosition;
    if (mapped[2] == keep) c = newPosition;
    if (triangleArea(a, b, c) <= areaEps) {
      return false;
    }
  }
  return true;
}

int removeDuplicateFaces(std::vector<FaceState>& faces) {
  struct KeyHash {
    std::size_t operator()(const std::array<int, 3>& ids) const {
      return static_cast<std::size_t>(ids[0]) * 73856093u ^
             static_cast<std::size_t>(ids[1]) * 19349663u ^
             static_cast<std::size_t>(ids[2]) * 83492791u;
    }
  };

  std::unordered_set<std::array<int, 3>, KeyHash> seen;
  int removed = 0;
  for (FaceState& face : faces) {
    if (!face.active) {
      continue;
    }
    std::array<int, 3> key = face.v;
    std::sort(key.begin(), key.end());
    if (!seen.insert(key).second) {
      face.active = false;
      ++removed;
    }
  }
  return removed;
}

void pushEdge(int a, int b, const std::vector<VertexState>& vertices,
              std::priority_queue<Candidate>& queue, SimplifyReport& report) {
  if (a == b || !vertices[a].active || !vertices[b].active) {
    return;
  }
  const Mat4 q = vertices[a].q + vertices[b].q;
  const SolveResult solve = solveOptimal(q, vertices[a].p, vertices[b].p);
  if (solve.usedFallback) {
    ++report.solverFallbacks;
  }
  queue.push(Candidate{solve.cost, std::min(a, b), std::max(a, b),
                       vertices[a].version, vertices[b].version});
}

Mesh compactResult(const std::vector<VertexState>& vertices,
                   const std::vector<FaceState>& faces) {
  Mesh result;
  std::vector<int> remap(vertices.size(), -1);
  result.faces.reserve(faces.size());
  for (const FaceState& face : faces) {
    if (!face.active) {
      continue;
    }
    Face out;
    bool ok = true;
    for (int i = 0; i < 3; ++i) {
      const int old = face.v[i];
      if (!vertices[old].active) {
        ok = false;
        break;
      }
      if (remap[old] < 0) {
        remap[old] = static_cast<int>(result.vertices.size());
        result.vertices.push_back(vertices[old].p);
      }
      out.v[i] = remap[old];
    }
    if (ok && out.v[0] != out.v[1] && out.v[1] != out.v[2] &&
        out.v[0] != out.v[2]) {
      result.faces.push_back(out);
    }
  }
  return result;
}

}  // namespace

WeightMode parseWeightMode(const std::string& value) {
  if (value == "uniform") return WeightMode::Uniform;
  if (value == "dihedral") return WeightMode::Dihedral;
  if (value == "height") return WeightMode::Height;
  if (value == "xband") return WeightMode::XBand;
  throw std::invalid_argument("Unknown weight mode: " + value);
}

std::string toString(WeightMode mode) {
  switch (mode) {
    case WeightMode::Uniform:
      return "uniform";
    case WeightMode::Dihedral:
      return "dihedral";
    case WeightMode::Height:
      return "height";
    case WeightMode::XBand:
      return "xband";
  }
  return "unknown";
}

Mesh simplifyMesh(const Mesh& input, const SimplifyOptions& options,
                  SimplifyReport* outReport) {
  SimplifyReport report;
  report.initialVertices = static_cast<int>(input.vertices.size());
  report.initialFaces = static_cast<int>(input.faces.size());

  std::vector<Mat4> initialQuadrics;
  computeInitialQuadrics(input, options, initialQuadrics, report.minAppliedLineWeight,
                         report.maxAppliedLineWeight);

  std::vector<VertexState> vertices(input.vertices.size());
  for (int i = 0; i < static_cast<int>(input.vertices.size()); ++i) {
    vertices[i].p = input.vertices[i];
    vertices[i].q = initialQuadrics[i];
  }

  std::vector<FaceState> faces(input.faces.size());
  for (int i = 0; i < static_cast<int>(input.faces.size()); ++i) {
    faces[i].v = input.faces[i].v;
  }

  int activeFaceCount = static_cast<int>(faces.size());
  const int targetFaces =
      options.targetFaces > 0
          ? options.targetFaces
          : std::max(4, static_cast<int>(std::llround(input.faces.size() *
                                                      options.targetRatio)));
  const double diag = std::max(1e-12, input.bboxDiag());
  const double areaEps = diag * diag * 1e-18;

  std::priority_queue<Candidate> queue;
  auto rebuildQueue = [&]() {
    queue = std::priority_queue<Candidate>();
    for (const auto& [a, b] : collectActiveEdges(faces)) {
      pushEdge(a, b, vertices, queue, report);
    }
    ++report.queueRebuilds;
  };
  rebuildQueue();

  int stalePops = 0;
  while (activeFaceCount > targetFaces) {
    if (queue.empty()) {
      rebuildQueue();
      if (queue.empty()) {
        break;
      }
    }

    Candidate candidate = queue.top();
    queue.pop();
    const int a = candidate.a;
    const int b = candidate.b;
    if (a < 0 || b < 0 || a >= static_cast<int>(vertices.size()) ||
        b >= static_cast<int>(vertices.size()) || !vertices[a].active ||
        !vertices[b].active || vertices[a].version != candidate.versionA ||
        vertices[b].version != candidate.versionB || !areAdjacent(a, b, faces)) {
      if (++stalePops > 10000) {
        rebuildQueue();
        stalePops = 0;
      }
      continue;
    }
    stalePops = 0;

    const Mat4 mergedQ = vertices[a].q + vertices[b].q;
    const SolveResult solve = solveOptimal(mergedQ, vertices[a].p, vertices[b].p);
    if (solve.usedFallback) {
      ++report.solverFallbacks;
    }

    const int keep = a;
    const int remove = b;
    if (!collapseWouldBeValid(keep, remove, solve.position, faces, vertices,
                              areaEps)) {
      ++report.rejectedCollapses;
      vertices[keep].version++;
      vertices[remove].version++;
      continue;
    }

    vertices[keep].p = solve.position;
    vertices[keep].q = mergedQ;
    vertices[remove].active = false;
    vertices[keep].version++;
    vertices[remove].version++;

    for (FaceState& face : faces) {
      if (!face.active) {
        continue;
      }
      bool changed = false;
      for (int& id : face.v) {
        if (id == remove) {
          id = keep;
          changed = true;
        }
      }
      if (changed && (face.v[0] == face.v[1] || face.v[1] == face.v[2] ||
                      face.v[0] == face.v[2])) {
        face.active = false;
        --activeFaceCount;
      }
    }
    activeFaceCount -= removeDuplicateFaces(faces);
    ++report.collapsedEdges;

    for (int neighbor : activeNeighborsOf(keep, faces, vertices)) {
      pushEdge(keep, neighbor, vertices, queue, report);
    }

    if (options.verbose && report.collapsedEdges % 1000 == 0) {
      std::cerr << "collapsed " << report.collapsedEdges << ", faces "
                << activeFaceCount << "/" << targetFaces << "\n";
    }
  }

  Mesh result = compactResult(vertices, faces);
  report.finalVertices = static_cast<int>(result.vertices.size());
  report.finalFaces = static_cast<int>(result.faces.size());
  if (outReport) {
    *outReport = report;
  }
  return result;
}

}  // namespace lq
