#include "line_quadrics_qem/simplification/QEMSimplifier.h"

#include "line_quadrics_qem/core/MeshTopology.h"
#include "line_quadrics_qem/features/FeatureDetection.h"

#include <Eigen/Eigenvalues>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>
#include <queue>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>

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
  bool isFeature = false;
  bool isBoundary = false;
  bool circularFeature = false;
  bool featureJunction = false;
  FeaturePrimitiveType featurePrimitive = FeaturePrimitiveType::Unknown;
  int featureLoopId = -1;
  Vec3 curveTangent = Vec3::Zero();
  Vec3 circleCenter = Vec3::Zero();
  Vec3 circleNormal = Vec3(0.0, 0.0, 1.0);
  double circleRadius = 0.0;
  Vec3 ellipseCenter = Vec3::Zero();
  Vec3 ellipseNormal = Vec3(0.0, 0.0, 1.0);
  Vec3 ellipseMajorAxis = Vec3(1.0, 0.0, 0.0);
  Vec3 ellipseMinorAxis = Vec3(0.0, 1.0, 0.0);
  double ellipseMajorRadius = 0.0;
  double ellipseMinorRadius = 0.0;
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

  bool operator<(const Candidate& other) const { return cost > other.cost; }
};

struct SolveResult {
  Vec3 position = Vec3::Zero();
  double cost = 0.0;
  bool usedFallback = false;
};

enum class CollapseRejectReason {
  None,
  Topology,
  NormalFlip,
  TriangleQuality,
  SelfIntersection,
  LocalError,
};

struct BoundaryCollapseDecision {
  bool allowed = true;
  bool boundaryEdge = false;
};

struct FeatureCurveConstraint {
  bool valid = false;
  bool closed = false;
  FeaturePrimitiveType primitive = FeaturePrimitiveType::Unknown;
  std::vector<Vec3> samples;
};

struct CellCoord {
  int x = 0;
  int y = 0;
  int z = 0;

  bool operator==(const CellCoord& other) const {
    return x == other.x && y == other.y && z == other.z;
  }
};

struct CellCoordHash {
  std::size_t operator()(const CellCoord& cell) const {
    std::size_t seed = 1469598103934665603ull;
    auto mix = [&](int value) {
      seed ^= static_cast<std::size_t>(static_cast<std::uint32_t>(value));
      seed *= 1099511628211ull;
    };
    mix(cell.x);
    mix(cell.y);
    mix(cell.z);
    return seed;
  }
};

std::uint64_t edgeKey(int a, int b) {
  if (a > b) std::swap(a, b);
  return (static_cast<std::uint64_t>(static_cast<uint32_t>(a)) << 32u) |
         static_cast<uint32_t>(b);
}

std::array<int, 3> faceKey(std::array<int, 3> ids) {
  std::sort(ids.begin(), ids.end());
  return ids;
}

struct FaceKeyHash {
  std::size_t operator()(const std::array<int, 3>& ids) const {
    return static_cast<std::size_t>(ids[0]) * 73856093u ^
           static_cast<std::size_t>(ids[1]) * 19349663u ^
           static_cast<std::size_t>(ids[2]) * 83492791u;
  }
};

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

  Vec3 seed = std::abs(n.x()) < 0.9 ? Vec3(1.0, 0.0, 0.0) : Vec3(0.0, 1.0, 0.0);
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

void requireFiniteNonNegative(double value, const char* name) {
  if (!std::isfinite(value) || value < 0.0) {
    throw std::invalid_argument(std::string(name) +
                                " must be finite and non-negative.");
  }
}

void validateSimplifyOptions(const SimplifyOptions& options) {
  if (options.targetFaces == 0 || options.targetFaces < -1) {
    throw std::invalid_argument("targetFaces must be -1 or a positive face count.");
  }
  if (options.targetFaces < 0 &&
      (!std::isfinite(options.targetRatio) || options.targetRatio <= 0.0 ||
       options.targetRatio > 1.0)) {
    throw std::invalid_argument("targetRatio must be finite and in the range (0, 1].");
  }
  requireFiniteNonNegative(options.lineWeight, "lineWeight");
  requireFiniteNonNegative(options.featureBoost, "featureBoost");
  requireFiniteNonNegative(options.adaptiveBaseLineWeight, "adaptiveBaseLineWeight");
  requireFiniteNonNegative(options.boundaryWeight, "boundaryWeight");
  requireFiniteNonNegative(options.featureCurveWeight, "featureCurveWeight");
  requireFiniteNonNegative(options.maxFeatureCurveDeviationRatio,
                           "maxFeatureCurveDeviationRatio");
  requireFiniteNonNegative(options.circleFitRelativeThreshold,
                           "circleFitRelativeThreshold");
  requireFiniteNonNegative(options.ellipseFitRelativeThreshold,
                           "ellipseFitRelativeThreshold");
  requireFiniteNonNegative(options.nearCircleAxisRatioTolerance,
                           "nearCircleAxisRatioTolerance");
  requireFiniteNonNegative(options.normalTensorFeatureThreshold,
                           "normalTensorFeatureThreshold");
  if (!std::isfinite(options.featureAngleDeg) || options.featureAngleDeg < 0.0 ||
      options.featureAngleDeg > 180.0) {
    throw std::invalid_argument("featureAngleDeg must be finite and in [0, 180].");
  }
  if (!std::isfinite(options.normalTensorMinEdgeAlignment) ||
      options.normalTensorMinEdgeAlignment < 0.0 ||
      options.normalTensorMinEdgeAlignment > 1.0) {
    throw std::invalid_argument(
        "normalTensorMinEdgeAlignment must be finite and in [0, 1].");
  }
  if (options.minFeatureLoopVertices < 3) {
    throw std::invalid_argument("minFeatureLoopVertices must be at least 3.");
  }
  if (options.minCircularFeatureLoopVertices < 3) {
    throw std::invalid_argument("minCircularFeatureLoopVertices must be at least 3.");
  }
  if (options.normalTensorSmoothingIterations < 0) {
    throw std::invalid_argument(
        "normalTensorSmoothingIterations must be non-negative.");
  }
  if (options.normalTensorScaleCount < 1) {
    throw std::invalid_argument("normalTensorScaleCount must be positive.");
  }
  requireFiniteNonNegative(options.minTriangleQuality, "minTriangleQuality");
  if (options.minTriangleQuality > 1.0) {
    throw std::invalid_argument("minTriangleQuality must be in [0, 1].");
  }
  requireFiniteNonNegative(options.maxLocalError, "maxLocalError");
  requireFiniteNonNegative(options.maxLocalErrorRatio, "maxLocalErrorRatio");
  if (!std::isfinite(options.maxNormalDeviationDeg) ||
      options.maxNormalDeviationDeg < 0.0 || options.maxNormalDeviationDeg > 180.0) {
    throw std::invalid_argument(
        "maxNormalDeviationDeg must be finite and in [0, 180].");
  }
}

void validateSimplifierInput(const Mesh& input) {
  if (input.empty()) {
    return;
  }
  std::string error;
  if (!validateMeshIndices(input, &error)) {
    throw std::invalid_argument(error);
  }
  const Result<MeshTopology> topology = MeshTopology::build(input);
  if (!topology.ok()) {
    throw std::invalid_argument(topology.status().message());
  }
}

std::unordered_map<std::uint64_t, EdgeInfo>
buildEdgeInfo(const Mesh& mesh, const std::vector<Vec3>* faceNormals = nullptr) {
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

std::vector<double> computeFeatureScores(const Mesh& mesh,
                                         const SimplifyOptions& options) {
  const WeightMode mode = options.weightMode;
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

  if (mode == WeightMode::NormalTensor) {
    const std::vector<NormalTensorVertex> tensor = computeNormalTensorFeatures(
        mesh, NormalTensorOptions{options.normalTensorSmoothingIterations,
                                  options.normalTensorScaleCount});
    for (int i = 0; i < static_cast<int>(tensor.size()); ++i) {
      score[i] = tensor[i].featureScore;
    }
    return score;
  }

  std::vector<Vec3> faceNormals(mesh.faces.size(), Vec3::Zero());
  for (int fi = 0; fi < static_cast<int>(mesh.faces.size()); ++fi) {
    const Face& f = mesh.faces[fi];
    faceNormals[fi] = triangleNormal(mesh.vertices[f.v[0]], mesh.vertices[f.v[1]],
                                     mesh.vertices[f.v[2]]);
  }

  const auto edgeInfo = buildEdgeInfo(mesh);
  const double threshold = options.featureAngleDeg * kPi / 180.0;
  const double denom = std::max(1e-12, kPi - threshold);
  for (const auto& [key, info] : edgeInfo) {
    double edgeScore = 0.0;
    if (info.faces.size() == 1) {
      edgeScore = 1.0;
    } else if (info.faces.size() == 2) {
      const double dot = std::clamp(
          std::abs(faceNormals[info.faces[0]].dot(faceNormals[info.faces[1]])), -1.0,
          1.0);
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
    faceNormals[fi] = triangleNormal(mesh.vertices[f.v[0]], mesh.vertices[f.v[1]],
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
                            const FeatureAnalysis* featureAnalysis,
                            std::vector<Mat4>& quadrics, double& minLineWeight,
                            double& maxLineWeight) {
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
  const bool useNormalLineQuadrics =
      options.useLineQuadrics && options.lineWeight > 0.0;
  if (!useNormalLineQuadrics) {
    minLineWeight = 0.0;
  }

  const std::vector<double> featureScores = useNormalLineQuadrics
                                                ? computeFeatureScores(mesh, options)
                                                : std::vector<double>();

  if (useNormalLineQuadrics) {
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
  }

  if (options.preserveFeatureCurves && featureAnalysis &&
      options.featureCurveWeight > 0.0) {
    double positiveAreaSum = 0.0;
    int positiveAreaCount = 0;
    for (double area : vertexArea) {
      if (area > 1e-24) {
        positiveAreaSum += area;
        ++positiveAreaCount;
      }
    }
    const double fallbackArea =
        positiveAreaCount > 0
            ? positiveAreaSum / static_cast<double>(positiveAreaCount)
            : std::max(1e-12, mesh.bboxDiag() * mesh.bboxDiag() * 1e-6);

    for (int i = 0; i < static_cast<int>(mesh.vertices.size()); ++i) {
      if (i >= static_cast<int>(featureAnalysis->vertices.size())) {
        continue;
      }
      const VertexFeature& vf = featureAnalysis->vertices[i];
      if (!vf.isFeature || vf.tangent.norm() <= 1e-20) {
        continue;
      }
      const double areaScale = std::max(vertexArea[i], fallbackArea);
      const Mat4 qCurve = lineQuadric(mesh.vertices[i], vf.tangent);
      quadrics[i] += options.featureCurveWeight * areaScale * qCurve;
    }
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

Vec3 projectToCircle(const Vec3& p, const VertexState& feature) {
  Vec3 normal = feature.circleNormal;
  if (normal.norm() <= 1e-20 || feature.circleRadius <= 1e-20) {
    return p;
  }
  normal.normalize();

  Vec3 radial = p - feature.circleCenter;
  radial -= normal * radial.dot(normal);
  if (radial.norm() <= 1e-20) {
    radial = feature.p - feature.circleCenter;
    radial -= normal * radial.dot(normal);
  }
  if (radial.norm() <= 1e-20) {
    return feature.circleCenter;
  }
  return feature.circleCenter + feature.circleRadius * radial.normalized();
}

Vec3 projectToEllipse(const Vec3& p, const VertexState& feature) {
  Vec3 major = feature.ellipseMajorAxis;
  Vec3 minor = feature.ellipseMinorAxis;
  Vec3 normal = feature.ellipseNormal;
  if (major.norm() <= 1e-20 || minor.norm() <= 1e-20 || normal.norm() <= 1e-20 ||
      feature.ellipseMajorRadius <= 1e-20 || feature.ellipseMinorRadius <= 1e-20) {
    return p;
  }
  major.normalize();
  minor.normalize();
  normal.normalize();

  Vec3 delta = p - feature.ellipseCenter;
  delta -= normal * delta.dot(normal);
  if (delta.norm() <= 1e-20) {
    delta = feature.p - feature.ellipseCenter;
    delta -= normal * delta.dot(normal);
  }
  if (delta.norm() <= 1e-20) {
    return feature.ellipseCenter + feature.ellipseMajorRadius * major;
  }

  const double theta = std::atan2(delta.dot(minor) / feature.ellipseMinorRadius,
                                  delta.dot(major) / feature.ellipseMajorRadius);
  return feature.ellipseCenter + feature.ellipseMajorRadius * std::cos(theta) * major +
         feature.ellipseMinorRadius * std::sin(theta) * minor;
}

double triangleQualityLocal(const Vec3& a, const Vec3& b, const Vec3& c) {
  const double l0 = (b - a).squaredNorm();
  const double l1 = (c - b).squaredNorm();
  const double l2 = (a - c).squaredNorm();
  const double denom = l0 + l1 + l2;
  if (denom <= 1e-30) {
    return 0.0;
  }
  return 4.0 * std::sqrt(3.0) * triangleArea(a, b, c) / denom;
}

double pointTriangleDistanceSquaredLocal(const Vec3& p, const Vec3& a, const Vec3& b,
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

bool aabbOverlap(const Vec3& aLo, const Vec3& aHi, const Vec3& bLo, const Vec3& bHi,
                 double eps) {
  for (int axis = 0; axis < 3; ++axis) {
    if (aHi[axis] + eps < bLo[axis] || bHi[axis] + eps < aLo[axis]) {
      return false;
    }
  }
  return true;
}

std::pair<Vec3, Vec3> triangleAabb(const std::array<Vec3, 3>& tri,
                                   double padding = 0.0) {
  Vec3 lo = tri[0].cwiseMin(tri[1]).cwiseMin(tri[2]);
  Vec3 hi = tri[0].cwiseMax(tri[1]).cwiseMax(tri[2]);
  const Vec3 pad(padding, padding, padding);
  return {lo - pad, hi + pad};
}

bool segmentIntersectsTriangle(const Vec3& p0, const Vec3& p1, const Vec3& a,
                               const Vec3& b, const Vec3& c, double eps) {
  const Vec3 dir = p1 - p0;
  const Vec3 e1 = b - a;
  const Vec3 e2 = c - a;
  const Vec3 h = dir.cross(e2);
  const double det = e1.dot(h);
  if (std::abs(det) <= eps) {
    return false;
  }
  const double invDet = 1.0 / det;
  const Vec3 s = p0 - a;
  const double u = invDet * s.dot(h);
  if (u < -eps || u > 1.0 + eps) {
    return false;
  }
  const Vec3 q = s.cross(e1);
  const double v = invDet * dir.dot(q);
  if (v < -eps || u + v > 1.0 + eps) {
    return false;
  }
  const double t = invDet * e2.dot(q);
  return t >= eps && t <= 1.0 - eps;
}

Eigen::Vector2d projectToDominantPlane(const Vec3& p, int dropAxis) {
  switch (dropAxis) {
  case 0:
    return Eigen::Vector2d(p.y(), p.z());
  case 1:
    return Eigen::Vector2d(p.x(), p.z());
  default:
    return Eigen::Vector2d(p.x(), p.y());
  }
}

double orient2d(const Eigen::Vector2d& a, const Eigen::Vector2d& b,
                const Eigen::Vector2d& c) {
  return (b.x() - a.x()) * (c.y() - a.y()) - (b.y() - a.y()) * (c.x() - a.x());
}

bool intervalsOverlapWithLength(double a0, double a1, double b0, double b1,
                                double eps) {
  if (a0 > a1) std::swap(a0, a1);
  if (b0 > b1) std::swap(b0, b1);
  return std::min(a1, b1) - std::max(a0, b0) > eps;
}

bool pointOnSegment2d(const Eigen::Vector2d& p, const Eigen::Vector2d& a,
                      const Eigen::Vector2d& b, double eps) {
  if (std::abs(orient2d(a, b, p)) > eps) {
    return false;
  }
  return p.x() >= std::min(a.x(), b.x()) - eps &&
         p.x() <= std::max(a.x(), b.x()) + eps &&
         p.y() >= std::min(a.y(), b.y()) - eps && p.y() <= std::max(a.y(), b.y()) + eps;
}

bool segmentsIntersect2d(const Eigen::Vector2d& a0, const Eigen::Vector2d& a1,
                         const Eigen::Vector2d& b0, const Eigen::Vector2d& b1,
                         double eps) {
  const double o1 = orient2d(a0, a1, b0);
  const double o2 = orient2d(a0, a1, b1);
  const double o3 = orient2d(b0, b1, a0);
  const double o4 = orient2d(b0, b1, a1);
  const auto sign = [eps](double value) {
    if (value > eps) return 1;
    if (value < -eps) return -1;
    return 0;
  };
  const int s1 = sign(o1);
  const int s2 = sign(o2);
  const int s3 = sign(o3);
  const int s4 = sign(o4);

  if (s1 * s2 < 0 && s3 * s4 < 0) {
    return true;
  }

  const bool collinear = std::abs(o1) <= eps && std::abs(o2) <= eps &&
                         std::abs(o3) <= eps && std::abs(o4) <= eps;
  if (collinear) {
    const bool useX = std::abs(a1.x() - a0.x()) >= std::abs(a1.y() - a0.y());
    return useX ? intervalsOverlapWithLength(a0.x(), a1.x(), b0.x(), b1.x(), eps)
                : intervalsOverlapWithLength(a0.y(), a1.y(), b0.y(), b1.y(), eps);
  }

  const bool endpointTouch = (s1 == 0 && pointOnSegment2d(b0, a0, a1, eps)) ||
                             (s2 == 0 && pointOnSegment2d(b1, a0, a1, eps)) ||
                             (s3 == 0 && pointOnSegment2d(a0, b0, b1, eps)) ||
                             (s4 == 0 && pointOnSegment2d(a1, b0, b1, eps));
  return endpointTouch && (s1 * s2 < 0 || s3 * s4 < 0);
}

bool pointStrictlyInsideTriangle2d(const Eigen::Vector2d& p,
                                   const std::array<Eigen::Vector2d, 3>& tri,
                                   double eps) {
  const double o0 = orient2d(tri[0], tri[1], p);
  const double o1 = orient2d(tri[1], tri[2], p);
  const double o2 = orient2d(tri[2], tri[0], p);
  return (o0 > eps && o1 > eps && o2 > eps) || (o0 < -eps && o1 < -eps && o2 < -eps);
}

bool coplanarTrianglesOverlap(const std::array<Vec3, 3>& lhs,
                              const std::array<Vec3, 3>& rhs, const Vec3& normal,
                              double eps) {
  int dropAxis = 0;
  if (std::abs(normal.y()) > std::abs(normal[dropAxis])) {
    dropAxis = 1;
  }
  if (std::abs(normal.z()) > std::abs(normal[dropAxis])) {
    dropAxis = 2;
  }

  std::array<Eigen::Vector2d, 3> a{};
  std::array<Eigen::Vector2d, 3> b{};
  for (int i = 0; i < 3; ++i) {
    a[i] = projectToDominantPlane(lhs[i], dropAxis);
    b[i] = projectToDominantPlane(rhs[i], dropAxis);
  }

  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      if (segmentsIntersect2d(a[i], a[(i + 1) % 3], b[j], b[(j + 1) % 3], eps)) {
        return true;
      }
    }
  }
  for (int i = 0; i < 3; ++i) {
    if (pointStrictlyInsideTriangle2d(a[i], b, eps) ||
        pointStrictlyInsideTriangle2d(b[i], a, eps)) {
      return true;
    }
  }
  return false;
}

bool trianglesIntersect(const std::array<Vec3, 3>& lhs, const std::array<Vec3, 3>& rhs,
                        double eps) {
  Vec3 lhsLo = lhs[0].cwiseMin(lhs[1]).cwiseMin(lhs[2]);
  Vec3 lhsHi = lhs[0].cwiseMax(lhs[1]).cwiseMax(lhs[2]);
  Vec3 rhsLo = rhs[0].cwiseMin(rhs[1]).cwiseMin(rhs[2]);
  Vec3 rhsHi = rhs[0].cwiseMax(rhs[1]).cwiseMax(rhs[2]);
  if (!aabbOverlap(lhsLo, lhsHi, rhsLo, rhsHi, eps)) {
    return false;
  }

  const Vec3 lhsNormal = (lhs[1] - lhs[0]).cross(lhs[2] - lhs[0]);
  const Vec3 rhsNormal = (rhs[1] - rhs[0]).cross(rhs[2] - rhs[0]);
  const double lhsNorm = lhsNormal.norm();
  const double rhsNorm = rhsNormal.norm();
  if (lhsNorm <= eps || rhsNorm <= eps) {
    return false;
  }
  const Vec3 lhsUnit = lhsNormal / lhsNorm;
  const Vec3 rhsUnit = rhsNormal / rhsNorm;
  const double parallelError = lhsUnit.cross(rhsUnit).norm();
  double maxPlaneDistance = 0.0;
  for (const Vec3& p : rhs) {
    maxPlaneDistance = std::max(maxPlaneDistance, std::abs((p - lhs[0]).dot(lhsUnit)));
  }
  if (parallelError <= 1e-8 && maxPlaneDistance <= eps) {
    return coplanarTrianglesOverlap(lhs, rhs, lhsUnit, eps);
  }

  for (int i = 0; i < 3; ++i) {
    if (segmentIntersectsTriangle(lhs[i], lhs[(i + 1) % 3], rhs[0], rhs[1], rhs[2],
                                  eps)) {
      return true;
    }
    if (segmentIntersectsTriangle(rhs[i], rhs[(i + 1) % 3], lhs[0], lhs[1], lhs[2],
                                  eps)) {
      return true;
    }
  }
  return false;
}

void refreshCircularTangent(VertexState& vertex) {
  if (!vertex.circularFeature) {
    return;
  }
  Vec3 normal = vertex.circleNormal;
  if (normal.norm() <= 1e-20) {
    return;
  }
  normal.normalize();
  Vec3 radial = vertex.p - vertex.circleCenter;
  radial -= normal * radial.dot(normal);
  if (radial.norm() <= 1e-20) {
    return;
  }
  vertex.curveTangent = normal.cross(radial).normalized();
}

void refreshEllipseTangent(VertexState& vertex) {
  if (vertex.featurePrimitive != FeaturePrimitiveType::Ellipse) {
    return;
  }
  Vec3 major = vertex.ellipseMajorAxis;
  Vec3 minor = vertex.ellipseMinorAxis;
  Vec3 normal = vertex.ellipseNormal;
  if (major.norm() <= 1e-20 || minor.norm() <= 1e-20 || normal.norm() <= 1e-20 ||
      vertex.ellipseMajorRadius <= 1e-20 || vertex.ellipseMinorRadius <= 1e-20) {
    return;
  }
  major.normalize();
  minor.normalize();
  normal.normalize();
  Vec3 delta = vertex.p - vertex.ellipseCenter;
  delta -= normal * delta.dot(normal);
  if (delta.norm() <= 1e-20) {
    return;
  }
  const double theta = std::atan2(delta.dot(minor) / vertex.ellipseMinorRadius,
                                  delta.dot(major) / vertex.ellipseMajorRadius);
  Vec3 tangent = -vertex.ellipseMajorRadius * std::sin(theta) * major +
                 vertex.ellipseMinorRadius * std::cos(theta) * minor;
  if (tangent.norm() > 1e-20) {
    vertex.curveTangent = tangent.normalized();
  }
}

bool featureCollapseAllowed(int keep, int remove,
                            const std::vector<VertexState>& vertices,
                            const std::vector<int>& activeLoopCounts,
                            const SimplifyOptions& options) {
  if (!options.preserveFeatureCurves) {
    return true;
  }

  const VertexState& a = vertices[keep];
  const VertexState& b = vertices[remove];
  if (!a.isFeature && !b.isFeature) {
    return true;
  }
  const bool primitiveFeature = a.circularFeature || b.circularFeature ||
                                a.featurePrimitive == FeaturePrimitiveType::Ellipse ||
                                b.featurePrimitive == FeaturePrimitiveType::Ellipse;
  const bool needsHardProtection = options.protectAllFeatureEdges || primitiveFeature;
  if (!needsHardProtection) {
    return true;
  }
  if (a.isFeature != b.isFeature) {
    return false;
  }
  if (a.featureLoopId < 0 || a.featureLoopId != b.featureLoopId) {
    return false;
  }
  if (a.featureJunction || b.featureJunction) {
    return false;
  }
  if (a.featureLoopId >= static_cast<int>(activeLoopCounts.size())) {
    return false;
  }
  const int minActiveLoopVertices = (a.circularFeature || b.circularFeature)
                                        ? options.minCircularFeatureLoopVertices
                                        : options.minFeatureLoopVertices;
  if (activeLoopCounts[a.featureLoopId] <= minActiveLoopVertices) {
    const bool hasCurveErrorBudget = options.maxFeatureCurveDeviationRatio > 0.0;
    const bool ellipseFeature = a.featurePrimitive == FeaturePrimitiveType::Ellipse ||
                                b.featurePrimitive == FeaturePrimitiveType::Ellipse;
    const int absoluteMinLoopVertices =
        (a.circularFeature || b.circularFeature || ellipseFeature) ? 4 : 3;
    if (!hasCurveErrorBudget ||
        activeLoopCounts[a.featureLoopId] <= absoluteMinLoopVertices) {
      return false;
    }
  }
  return true;
}

std::vector<char> computeBoundaryVertices(const Mesh& mesh) {
  std::vector<char> boundary(mesh.vertices.size(), 0);
  const auto edgeInfo = buildEdgeInfo(mesh);
  for (const auto& [key, info] : edgeInfo) {
    if (info.faces.size() == 1) {
      const auto [a, b] = unpackEdgeKey(key);
      if (a >= 0 && a < static_cast<int>(boundary.size())) {
        boundary[a] = 1;
      }
      if (b >= 0 && b < static_cast<int>(boundary.size())) {
        boundary[b] = 1;
      }
    }
  }
  return boundary;
}

bool projectFeaturePlacement(int keep, int remove,
                             const std::vector<VertexState>& vertices,
                             const std::vector<FeatureCurveConstraint>& curves,
                             const SimplifyOptions& options, Vec3& position) {
  if (!options.preserveFeatureCurves) {
    return false;
  }
  const VertexState& a = vertices[keep];
  const VertexState& b = vertices[remove];
  if (!a.isFeature || !b.isFeature || a.featureLoopId != b.featureLoopId) {
    return false;
  }
  if (a.circularFeature) {
    position = projectToCircle(position, a);
    return true;
  }
  if (b.circularFeature) {
    position = projectToCircle(position, b);
    return true;
  }
  if (a.featurePrimitive == FeaturePrimitiveType::Ellipse) {
    position = projectToEllipse(position, a);
    return true;
  }
  if (b.featurePrimitive == FeaturePrimitiveType::Ellipse) {
    position = projectToEllipse(position, b);
    return true;
  }
  if (a.featureLoopId >= 0 && a.featureLoopId < static_cast<int>(curves.size()) &&
      curves[a.featureLoopId].valid &&
      curves[a.featureLoopId].primitive == FeaturePrimitiveType::PolygonalLoop) {
    const FeatureCurveConstraint& curve = curves[a.featureLoopId];
    double bestDist2 = std::numeric_limits<double>::infinity();
    Vec3 best = position;
    const int segmentCount =
        curve.closed ? static_cast<int>(curve.samples.size())
                     : std::max(0, static_cast<int>(curve.samples.size()) - 1);
    for (int i = 0; i < segmentCount; ++i) {
      const Vec3& p0 = curve.samples[i];
      const Vec3& p1 = curve.samples[(i + 1) % curve.samples.size()];
      const Vec3 edge = p1 - p0;
      const double len2 = edge.squaredNorm();
      Vec3 candidate = p0;
      if (len2 > 1e-30) {
        const double t = std::clamp((position - p0).dot(edge) / len2, 0.0, 1.0);
        candidate = p0 + t * edge;
      }
      const double dist2 = (position - candidate).squaredNorm();
      if (dist2 < bestDist2) {
        bestDist2 = dist2;
        best = candidate;
      }
    }
    if (std::isfinite(bestDist2)) {
      position = best;
      return true;
    }
  }
  const Vec3 segment = b.p - a.p;
  const double segmentLen2 = segment.squaredNorm();
  if (segmentLen2 > 1e-30) {
    const double t = std::clamp((position - a.p).dot(segment) / segmentLen2, 0.0, 1.0);
    position = a.p + t * segment;
    return true;
  }
  Vec3 tangent = a.curveTangent;
  if (tangent.norm() <= 1e-20) {
    tangent = b.curveTangent;
  }
  if (tangent.norm() > 1e-20) {
    tangent.normalize();
    const Vec3 anchor = 0.5 * (a.p + b.p);
    position = anchor + tangent * (position - anchor).dot(tangent);
    return true;
  }
  return false;
}

bool projectBoundaryPlacement(int keep, int remove,
                              const BoundaryCollapseDecision& decision,
                              const std::vector<VertexState>& vertices,
                              Vec3& position) {
  if (!decision.boundaryEdge) {
    return false;
  }

  const Vec3& a = vertices[keep].p;
  const Vec3& b = vertices[remove].p;
  const Vec3 edge = b - a;
  const double len2 = edge.squaredNorm();
  if (len2 <= 1e-30) {
    position = 0.5 * (a + b);
    return true;
  }
  const double t = std::clamp((position - a).dot(edge) / len2, 0.0, 1.0);
  position = a + t * edge;
  return true;
}

bool containsVertex(const FaceState& face, int vertex) {
  return face.v[0] == vertex || face.v[1] == vertex || face.v[2] == vertex;
}

struct DynamicTopology {
  std::vector<std::unordered_set<int>> vertexFaces;
  std::unordered_map<std::array<int, 3>, std::unordered_set<int>, FaceKeyHash>
      facesByKey;

  DynamicTopology(const std::vector<FaceState>& faces, int vertexCount) {
    vertexFaces.resize(vertexCount);
    for (int fi = 0; fi < static_cast<int>(faces.size()); ++fi) {
      if (faces[fi].active) {
        addFace(fi, faces[fi]);
      }
    }
  }

  void addFace(int faceId, const FaceState& face) {
    for (int id : face.v) {
      if (id >= 0 && id < static_cast<int>(vertexFaces.size())) {
        vertexFaces[id].insert(faceId);
      }
    }
    facesByKey[faceKey(face.v)].insert(faceId);
  }

  void removeFace(int faceId, const FaceState& face) {
    for (int id : face.v) {
      if (id >= 0 && id < static_cast<int>(vertexFaces.size())) {
        vertexFaces[id].erase(faceId);
      }
    }
    const auto key = faceKey(face.v);
    auto it = facesByKey.find(key);
    if (it != facesByKey.end()) {
      it->second.erase(faceId);
      if (it->second.empty()) {
        facesByKey.erase(it);
      }
    }
  }

  bool hasDuplicateFace(int faceId, const FaceState& face) const {
    const auto it = facesByKey.find(faceKey(face.v));
    if (it == facesByKey.end()) {
      return false;
    }
    for (int id : it->second) {
      if (id != faceId) {
        return true;
      }
    }
    return false;
  }
};

class SpatialFaceIndex {
public:
  void rebuild(const std::vector<FaceState>& faces,
               const std::vector<VertexState>& vertices) {
    cells_.clear();
    overflowFaces_.clear();
    activeFaces_.clear();
    faceCells_.assign(faces.size(), {});
    enabled_ = false;
    if (faces.empty() || vertices.empty()) {
      return;
    }

    Vec3 lo = Vec3::Constant(std::numeric_limits<double>::infinity());
    Vec3 hi = Vec3::Constant(-std::numeric_limits<double>::infinity());
    int activeFaces = 0;
    for (const FaceState& face : faces) {
      if (!face.active) {
        continue;
      }
      ++activeFaces;
      for (int id : face.v) {
        lo = lo.cwiseMin(vertices[id].p);
        hi = hi.cwiseMax(vertices[id].p);
      }
    }
    if (activeFaces <= 0 || !lo.allFinite() || !hi.allFinite()) {
      return;
    }

    origin_ = lo;
    const double diag = std::max(1e-12, (hi - lo).norm());
    cellSize_ = std::max(
        diag / std::max(1.0, std::cbrt(static_cast<double>(activeFaces))), diag * 1e-6);
    enabled_ = std::isfinite(cellSize_) && cellSize_ > 0.0;
    if (!enabled_) {
      return;
    }

    for (int fi = 0; fi < static_cast<int>(faces.size()); ++fi) {
      if (faces[fi].active) {
        insertFace(fi, faces[fi], vertices);
      }
    }
  }

  void removeFace(int faceId) {
    if (!enabled_ || faceId < 0 || faceId >= static_cast<int>(faceCells_.size())) {
      return;
    }
    activeFaces_.erase(faceId);
    for (const CellCoord& cell : faceCells_[faceId]) {
      auto it = cells_.find(cell);
      if (it == cells_.end()) {
        continue;
      }
      it->second.erase(faceId);
      if (it->second.empty()) {
        cells_.erase(it);
      }
    }
    faceCells_[faceId].clear();
    overflowFaces_.erase(faceId);
  }

  void updateFace(int faceId, const FaceState& face,
                  const std::vector<VertexState>& vertices) {
    removeFace(faceId);
    if (face.active) {
      insertFace(faceId, face, vertices);
    }
  }

  std::vector<int> query(const Vec3& lo, const Vec3& hi) const {
    std::unordered_set<int> result;
    if (!enabled_) {
      return {};
    }
    const std::vector<CellCoord> queryCells = cellsForAabb(lo, hi);
    if (queryCells.empty()) {
      return std::vector<int>(activeFaces_.begin(), activeFaces_.end());
    }
    for (const CellCoord& cell : queryCells) {
      const auto it = cells_.find(cell);
      if (it == cells_.end()) {
        continue;
      }
      result.insert(it->second.begin(), it->second.end());
    }
    result.insert(overflowFaces_.begin(), overflowFaces_.end());
    return std::vector<int>(result.begin(), result.end());
  }

  bool enabled() const { return enabled_; }

private:
  void insertFace(int faceId, const FaceState& face,
                  const std::vector<VertexState>& vertices) {
    if (!enabled_ || faceId < 0 || faceId >= static_cast<int>(faceCells_.size())) {
      return;
    }
    activeFaces_.insert(faceId);
    const std::array<Vec3, 3> tri = {vertices[face.v[0]].p, vertices[face.v[1]].p,
                                     vertices[face.v[2]].p};
    const auto [lo, hi] = triangleAabb(tri);
    std::vector<CellCoord> cells = cellsForAabb(lo, hi);
    if (cells.empty()) {
      overflowFaces_.insert(faceId);
      return;
    }
    faceCells_[faceId] = cells;
    for (const CellCoord& cell : cells) {
      cells_[cell].insert(faceId);
    }
  }

  CellCoord coordFor(const Vec3& p) const {
    return {static_cast<int>(std::floor((p.x() - origin_.x()) / cellSize_)),
            static_cast<int>(std::floor((p.y() - origin_.y()) / cellSize_)),
            static_cast<int>(std::floor((p.z() - origin_.z()) / cellSize_))};
  }

  std::vector<CellCoord> cellsForAabb(const Vec3& lo, const Vec3& hi) const {
    if (!enabled_ || !lo.allFinite() || !hi.allFinite()) {
      return {};
    }
    const CellCoord c0 = coordFor(lo);
    const CellCoord c1 = coordFor(hi);
    const int minX = std::min(c0.x, c1.x);
    const int maxX = std::max(c0.x, c1.x);
    const int minY = std::min(c0.y, c1.y);
    const int maxY = std::max(c0.y, c1.y);
    const int minZ = std::min(c0.z, c1.z);
    const int maxZ = std::max(c0.z, c1.z);
    const long long count = static_cast<long long>(maxX - minX + 1) *
                            static_cast<long long>(maxY - minY + 1) *
                            static_cast<long long>(maxZ - minZ + 1);
    constexpr long long kMaxCellsPerFace = 512;
    if (count <= 0 || count > kMaxCellsPerFace) {
      return {};
    }

    std::vector<CellCoord> result;
    result.reserve(static_cast<std::size_t>(count));
    for (int x = minX; x <= maxX; ++x) {
      for (int y = minY; y <= maxY; ++y) {
        for (int z = minZ; z <= maxZ; ++z) {
          result.push_back(CellCoord{x, y, z});
        }
      }
    }
    return result;
  }

  bool enabled_ = false;
  Vec3 origin_ = Vec3::Zero();
  double cellSize_ = 0.0;
  std::unordered_map<CellCoord, std::unordered_set<int>, CellCoordHash> cells_;
  std::unordered_set<int> overflowFaces_;
  std::unordered_set<int> activeFaces_;
  std::vector<std::vector<CellCoord>> faceCells_;
};

std::vector<std::pair<int, int>>
collectActiveEdges(const std::vector<FaceState>& faces) {
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

bool areAdjacent(int a, int b, const std::vector<FaceState>& faces,
                 const DynamicTopology& topology) {
  if (a < 0 || b < 0 || a >= static_cast<int>(topology.vertexFaces.size()) ||
      b >= static_cast<int>(topology.vertexFaces.size())) {
    return false;
  }
  const auto& aFaces = topology.vertexFaces[a];
  const auto& bFaces = topology.vertexFaces[b];
  const auto& smaller = aFaces.size() <= bFaces.size() ? aFaces : bFaces;
  const auto& larger = aFaces.size() <= bFaces.size() ? bFaces : aFaces;
  for (int faceId : smaller) {
    if (larger.find(faceId) != larger.end() && faces[faceId].active) {
      return true;
    }
  }
  return false;
}

int activeIncidentFaceCountForEdge(int a, int b, const std::vector<FaceState>& faces,
                                   const DynamicTopology& topology) {
  if (a < 0 || b < 0 || a >= static_cast<int>(topology.vertexFaces.size()) ||
      b >= static_cast<int>(topology.vertexFaces.size())) {
    return 0;
  }
  const auto& aFaces = topology.vertexFaces[a];
  const auto& bFaces = topology.vertexFaces[b];
  const auto& smaller = aFaces.size() <= bFaces.size() ? aFaces : bFaces;
  const auto& larger = aFaces.size() <= bFaces.size() ? bFaces : aFaces;
  int count = 0;
  for (int faceId : smaller) {
    if (larger.find(faceId) != larger.end() && faces[faceId].active) {
      ++count;
    }
  }
  return count;
}

BoundaryCollapseDecision
boundaryCollapseDecision(int keep, int remove, const std::vector<FaceState>& faces,
                         const std::vector<VertexState>& vertices,
                         const DynamicTopology& topology,
                         const SimplifyOptions& options) {
  if (!options.preserveBoundary) {
    return {};
  }

  const bool keepBoundary = vertices[keep].isBoundary;
  const bool removeBoundary = vertices[remove].isBoundary;
  if (!keepBoundary && !removeBoundary) {
    return {};
  }
  if (keepBoundary != removeBoundary) {
    return {false, false};
  }

  const bool isCurrentBoundaryEdge =
      activeIncidentFaceCountForEdge(keep, remove, faces, topology) == 1;
  return {isCurrentBoundaryEdge, isCurrentBoundaryEdge};
}

std::vector<int> activeNeighborsOf(int v, const std::vector<FaceState>& faces,
                                   const std::vector<VertexState>& vertices,
                                   const DynamicTopology& topology) {
  std::unordered_set<int> seen;
  if (v < 0 || v >= static_cast<int>(topology.vertexFaces.size())) {
    return {};
  }
  for (int faceId : topology.vertexFaces[v]) {
    const FaceState& face = faces[faceId];
    if (!face.active) {
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

std::unordered_set<int> activeLinkOf(int vertex, const std::vector<FaceState>& faces,
                                     const std::vector<VertexState>& vertices,
                                     const DynamicTopology& topology,
                                     int excludedVertex) {
  std::unordered_set<int> link;
  if (vertex < 0 || vertex >= static_cast<int>(topology.vertexFaces.size())) {
    return link;
  }
  for (int faceId : topology.vertexFaces[vertex]) {
    const FaceState& face = faces[faceId];
    if (!face.active) {
      continue;
    }
    for (int id : face.v) {
      if (id != vertex && id != excludedVertex && vertices[id].active) {
        link.insert(id);
      }
    }
  }
  return link;
}

bool collapseWouldPreserveLinkCondition(int keep, int remove,
                                        const std::vector<FaceState>& faces,
                                        const std::vector<VertexState>& vertices,
                                        const DynamicTopology& topology) {
  std::unordered_set<int> edgeLink;
  int incidentFaceCount = 0;
  const auto& keepFaces = topology.vertexFaces[keep];
  const auto& removeFaces = topology.vertexFaces[remove];
  const auto& smaller =
      keepFaces.size() <= removeFaces.size() ? keepFaces : removeFaces;
  const auto& larger = keepFaces.size() <= removeFaces.size() ? removeFaces : keepFaces;
  for (int faceId : smaller) {
    if (larger.find(faceId) == larger.end()) {
      continue;
    }
    const FaceState& face = faces[faceId];
    if (!face.active) {
      continue;
    }
    ++incidentFaceCount;
    for (int id : face.v) {
      if (id != keep && id != remove && vertices[id].active) {
        edgeLink.insert(id);
      }
    }
  }

  if (incidentFaceCount <= 0 || incidentFaceCount > 2 ||
      edgeLink.size() != static_cast<std::size_t>(incidentFaceCount)) {
    return false;
  }

  const std::unordered_set<int> keepLink =
      activeLinkOf(keep, faces, vertices, topology, remove);
  const std::unordered_set<int> removeLink =
      activeLinkOf(remove, faces, vertices, topology, keep);
  std::unordered_set<int> intersection;
  for (int id : keepLink) {
    if (removeLink.find(id) != removeLink.end()) {
      intersection.insert(id);
    }
  }

  if (intersection.size() != edgeLink.size()) {
    return false;
  }
  for (int id : intersection) {
    if (edgeLink.find(id) == edgeLink.end()) {
      return false;
    }
  }
  return true;
}

CollapseRejectReason collapseRejectReason(int keep, int remove, const Vec3& newPosition,
                                          const std::vector<FaceState>& faces,
                                          const std::vector<VertexState>& vertices,
                                          const DynamicTopology& topology,
                                          double areaEps, double minTriangleQuality,
                                          double minNormalDot, double maxLocalError,
                                          bool preventLocalIntersections,
                                          const SpatialFaceIndex* spatialIndex) {
  if (!collapseWouldPreserveLinkCondition(keep, remove, faces, vertices, topology)) {
    return CollapseRejectReason::Topology;
  }

  std::unordered_set<int> touchedFaces = topology.vertexFaces[keep];
  touchedFaces.insert(topology.vertexFaces[remove].begin(),
                      topology.vertexFaces[remove].end());
  struct NewTriangle {
    int faceId = -1;
    std::array<int, 3> ids{};
    std::array<Vec3, 3> p{};
  };
  std::vector<NewTriangle> newTriangles;
  std::vector<Vec3> localReferencePoints;
  const bool measureLocalError = maxLocalError > 0.0;
  if (measureLocalError) {
    localReferencePoints.push_back(vertices[keep].p);
    localReferencePoints.push_back(vertices[remove].p);
  }
  for (int faceId : touchedFaces) {
    const FaceState& face = faces[faceId];
    if (!face.active) {
      continue;
    }
    for (int id : face.v) {
      if (id != keep && id != remove && vertices[id].active) {
        localReferencePoints.push_back(vertices[id].p);
      }
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
                   (face.v[0] == remove || face.v[1] == remove || face.v[2] == remove);
    if (!touches || containsBoth) {
      continue;
    }
    if (mapped[0] == mapped[1] || mapped[1] == mapped[2] || mapped[0] == mapped[2]) {
      return CollapseRejectReason::Topology;
    }
    const Vec3 oldNormal = triangleNormal(vertices[face.v[0]].p, vertices[face.v[1]].p,
                                          vertices[face.v[2]].p);
    Vec3 a = vertices[mapped[0]].p;
    Vec3 b = vertices[mapped[1]].p;
    Vec3 c = vertices[mapped[2]].p;
    if (mapped[0] == keep) a = newPosition;
    if (mapped[1] == keep) b = newPosition;
    if (mapped[2] == keep) c = newPosition;
    const double area = triangleArea(a, b, c);
    if (area <= areaEps) {
      return CollapseRejectReason::Topology;
    }
    if (minTriangleQuality > 0.0 &&
        triangleQualityLocal(a, b, c) < minTriangleQuality) {
      return CollapseRejectReason::TriangleQuality;
    }
    if (minNormalDot > -1.0 && oldNormal.norm() > 1e-20) {
      const Vec3 newNormal = triangleNormal(a, b, c);
      if (newNormal.norm() <= 1e-20 || oldNormal.dot(newNormal) < minNormalDot) {
        return CollapseRejectReason::NormalFlip;
      }
    }
    if (preventLocalIntersections || measureLocalError) {
      newTriangles.push_back(NewTriangle{faceId, mapped, {a, b, c}});
    }
  }

  if (measureLocalError && !localReferencePoints.empty()) {
    if (newTriangles.empty()) {
      return CollapseRejectReason::LocalError;
    }
    const double maxError2 = maxLocalError * maxLocalError;
    for (const Vec3& point : localReferencePoints) {
      double best = std::numeric_limits<double>::infinity();
      for (const NewTriangle& tri : newTriangles) {
        best = std::min(best, pointTriangleDistanceSquaredLocal(point, tri.p[0],
                                                                tri.p[1], tri.p[2]));
      }
      if (!std::isfinite(best) || best > maxError2) {
        return CollapseRejectReason::LocalError;
      }
    }
  }

  if (preventLocalIntersections) {
    const double eps = std::sqrt(std::max(areaEps, 1e-30));
    auto sharesVertex = [](const std::array<int, 3>& a, const std::array<int, 3>& b) {
      for (int lhs : a) {
        for (int rhs : b) {
          if (lhs == rhs) {
            return true;
          }
        }
      }
      return false;
    };
    for (const NewTriangle& tri : newTriangles) {
      const auto [triLo, triHi] = triangleAabb(tri.p, eps);
      const std::vector<int> candidateFaces =
          spatialIndex ? spatialIndex->query(triLo, triHi) : std::vector<int>();
      const bool useSpatialCandidates = spatialIndex && spatialIndex->enabled();
      const int candidateCount = useSpatialCandidates
                                     ? static_cast<int>(candidateFaces.size())
                                     : static_cast<int>(faces.size());
      for (int candidateIndex = 0; candidateIndex < candidateCount; ++candidateIndex) {
        const int faceId =
            useSpatialCandidates ? candidateFaces[candidateIndex] : candidateIndex;
        if (!faces[faceId].active || touchedFaces.find(faceId) != touchedFaces.end()) {
          continue;
        }
        const FaceState& face = faces[faceId];
        if (sharesVertex(tri.ids, face.v)) {
          continue;
        }
        const std::array<Vec3, 3> other = {vertices[face.v[0]].p, vertices[face.v[1]].p,
                                           vertices[face.v[2]].p};
        if (trianglesIntersect(tri.p, other, eps)) {
          return CollapseRejectReason::SelfIntersection;
        }
      }
    }
  }
  return CollapseRejectReason::None;
}

[[maybe_unused]] int removeDuplicateFaces(std::vector<FaceState>& faces) {
  std::unordered_set<std::array<int, 3>, FaceKeyHash> seen;
  int removed = 0;
  for (FaceState& face : faces) {
    if (!face.active) {
      continue;
    }
    const std::array<int, 3> key = faceKey(face.v);
    if (!seen.insert(key).second) {
      face.active = false;
      ++removed;
    }
  }
  return removed;
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
    if (ok && out.v[0] != out.v[1] && out.v[1] != out.v[2] && out.v[0] != out.v[2]) {
      result.faces.push_back(out);
    }
  }
  return result;
}

class InitialQuadricBuilder {
public:
  explicit InitialQuadricBuilder(const SimplifyOptions& options) : options_(options) {}

  std::vector<Mat4> build(const Mesh& mesh, const FeatureAnalysis* featureAnalysis,
                          SimplifyReport& report) const {
    std::vector<Mat4> quadrics;
    computeInitialQuadrics(mesh, options_, featureAnalysis, quadrics,
                           report.minAppliedLineWeight, report.maxAppliedLineWeight);
    return quadrics;
  }

private:
  const SimplifyOptions& options_;
};

class FeatureConstraintPolicy {
public:
  explicit FeatureConstraintPolicy(const SimplifyOptions& options)
      : options_(options) {}

  bool canCollapse(int keep, int remove, const std::vector<VertexState>& vertices,
                   const std::vector<int>& activeLoopCounts) const {
    return featureCollapseAllowed(keep, remove, vertices, activeLoopCounts, options_);
  }

  bool projectPlacement(int keep, int remove, const std::vector<VertexState>& vertices,
                        const std::vector<FeatureCurveConstraint>& curves,
                        Vec3& position) const {
    return projectFeaturePlacement(keep, remove, vertices, curves, options_, position);
  }

private:
  const SimplifyOptions& options_;
};

class CandidateQueue {
public:
  void clear() { queue_ = std::priority_queue<Candidate>(); }
  bool empty() const { return queue_.empty(); }

  Candidate pop() {
    Candidate candidate = queue_.top();
    queue_.pop();
    return candidate;
  }

  void pushEdge(int a, int b, const std::vector<VertexState>& vertices,
                SimplifyReport& report) {
    if (a == b || !vertices[a].active || !vertices[b].active) {
      return;
    }
    const Mat4 q = vertices[a].q + vertices[b].q;
    const SolveResult solve = solveOptimal(q, vertices[a].p, vertices[b].p);
    if (solve.usedFallback) {
      ++report.solverFallbacks;
    }
    queue_.push(Candidate{solve.cost, std::min(a, b), std::max(a, b),
                          vertices[a].version, vertices[b].version});
  }

private:
  std::priority_queue<Candidate> queue_;
};

class SimplificationRun {
public:
  SimplificationRun(const Mesh& input, const SimplifyOptions& options)
      : input_(input), options_(options), quadrics_(options), featurePolicy_(options) {}

  Mesh execute(SimplifyReport* outReport) {
    initializeReport();
    analyzeFeatures();
    initializeVertices();
    initializeFaces();
    initializeBudget();
    rebuildQueue();
    collapseUntilTarget();

    Mesh result = compactResult(vertices_, faces_);
    report_.finalVertices = static_cast<int>(result.vertices.size());
    report_.finalFaces = static_cast<int>(result.faces.size());
    if (outReport) {
      *outReport = report_;
    }
    return result;
  }

private:
  void initializeReport() {
    report_ = SimplifyReport{};
    report_.initialVertices = static_cast<int>(input_.vertices.size());
    report_.initialFaces = static_cast<int>(input_.faces.size());
  }

  void analyzeFeatures() {
    featureAnalysis_ = FeatureAnalysis{};
    featureAnalysisPtr_ = nullptr;
    if (!options_.preserveFeatureCurves) {
      return;
    }

    FeatureOptions featureOptions;
    featureOptions.featureAngleDeg = options_.featureAngleDeg;
    featureOptions.circleFitRelativeThreshold = options_.circleFitRelativeThreshold;
    featureOptions.ellipseFitRelativeThreshold = options_.ellipseFitRelativeThreshold;
    featureOptions.nearCircleAxisRatioTolerance = options_.nearCircleAxisRatioTolerance;
    featureOptions.minFeatureLoopVertices =
        std::max(5, options_.minFeatureLoopVertices);
    featureOptions.useNormalTensorFeatures = options_.useNormalTensorFeatures;
    featureOptions.normalTensorFeatureThreshold = options_.normalTensorFeatureThreshold;
    featureOptions.normalTensorMinEdgeAlignment = options_.normalTensorMinEdgeAlignment;
    featureOptions.normalTensorSmoothingIterations =
        options_.normalTensorSmoothingIterations;
    featureOptions.normalTensorScaleCount = options_.normalTensorScaleCount;
    featureAnalysis_ = detectFeatureCurves(input_, featureOptions);
    featureAnalysisPtr_ = &featureAnalysis_;
    report_.featureLoops = static_cast<int>(featureAnalysis_.loops.size());
    report_.normalTensorFeatureEdges = featureAnalysis_.normalTensorFeatureEdges;
    for (const FeatureLoop& loop : featureAnalysis_.loops) {
      if (loop.circular) {
        ++report_.circularFeatureLoops;
      }
    }
    for (const VertexFeature& vertex : featureAnalysis_.vertices) {
      if (vertex.isFeature) {
        ++report_.featureVertices;
      }
    }
    initializeFeatureCurveConstraints();
  }

  void initializeFeatureCurveConstraints() {
    featureCurves_.clear();
    featureCurves_.resize(featureAnalysis_.loops.size());
    for (const FeatureLoop& loop : featureAnalysis_.loops) {
      if (loop.id < 0 || loop.id >= static_cast<int>(featureCurves_.size())) {
        continue;
      }
      FeatureCurveConstraint constraint;
      constraint.valid = loop.vertices.size() >= 2;
      constraint.closed = loop.closed;
      constraint.primitive = loop.primitive;
      constraint.samples.reserve(loop.vertices.size());
      for (int vertexId : loop.vertices) {
        if (vertexId >= 0 && vertexId < static_cast<int>(input_.vertices.size())) {
          constraint.samples.push_back(input_.vertices[vertexId]);
        }
      }
      constraint.valid = constraint.valid && constraint.samples.size() >= 2;
      featureCurves_[loop.id] = std::move(constraint);
    }
  }

  void initializeVertices() {
    const std::vector<Mat4> initialQuadrics =
        quadrics_.build(input_, featureAnalysisPtr_, report_);
    boundaryVertices_ = options_.preserveBoundary ? computeBoundaryVertices(input_)
                                                  : std::vector<char>();
    vertices_.assign(input_.vertices.size(), VertexState{});
    for (int i = 0; i < static_cast<int>(input_.vertices.size()); ++i) {
      vertices_[i].p = input_.vertices[i];
      vertices_[i].q = initialQuadrics[i];
      vertices_[i].isBoundary =
          i < static_cast<int>(boundaryVertices_.size()) && boundaryVertices_[i] != 0;
      initializeVertexFeature(i);
    }

    activeLoopCounts_.clear();
    if (!featureAnalysisPtr_) {
      return;
    }
    activeLoopCounts_.assign(featureAnalysisPtr_->loops.size(), 0);
    for (const VertexState& vertex : vertices_) {
      if (vertex.isFeature && vertex.featureLoopId >= 0 &&
          vertex.featureLoopId < static_cast<int>(activeLoopCounts_.size())) {
        ++activeLoopCounts_[vertex.featureLoopId];
      }
    }
  }

  void initializeVertexFeature(int vertexId) {
    if (!featureAnalysisPtr_ ||
        vertexId >= static_cast<int>(featureAnalysisPtr_->vertices.size())) {
      return;
    }
    const VertexFeature& vf = featureAnalysisPtr_->vertices[vertexId];
    VertexState& vertex = vertices_[vertexId];
    vertex.isFeature = vf.isFeature;
    vertex.circularFeature = vf.circular;
    vertex.featureJunction = vf.junction;
    vertex.featurePrimitive = vf.primitive;
    vertex.featureLoopId = vf.loopId;
    vertex.curveTangent = vf.tangent;
    vertex.circleCenter = vf.circleCenter;
    vertex.circleNormal = vf.circleNormal;
    vertex.circleRadius = vf.circleRadius;
    vertex.ellipseCenter = vf.ellipseCenter;
    vertex.ellipseNormal = vf.ellipseNormal;
    vertex.ellipseMajorAxis = vf.ellipseMajorAxis;
    vertex.ellipseMinorAxis = vf.ellipseMinorAxis;
    vertex.ellipseMajorRadius = vf.ellipseMajorRadius;
    vertex.ellipseMinorRadius = vf.ellipseMinorRadius;
  }

  void initializeFaces() {
    faces_.assign(input_.faces.size(), FaceState{});
    for (int i = 0; i < static_cast<int>(input_.faces.size()); ++i) {
      faces_[i].v = input_.faces[i].v;
    }
    topology_ =
        std::make_unique<DynamicTopology>(faces_, static_cast<int>(vertices_.size()));
    activeFaceCount_ = static_cast<int>(faces_.size());
    if (options_.preventLocalIntersections) {
      spatialIndex_.rebuild(faces_, vertices_);
    }
  }

  void initializeBudget() {
    targetFaces_ = options_.targetFaces > 0
                       ? options_.targetFaces
                       : std::max(4, static_cast<int>(std::llround(
                                         input_.faces.size() * options_.targetRatio)));
    const double diag = std::max(1e-12, input_.bboxDiag());
    areaEps_ = diag * diag * 1e-18;
    minNormalDot_ = options_.maxNormalDeviationDeg >= 180.0
                        ? -1.0
                        : std::cos(options_.maxNormalDeviationDeg * kPi / 180.0);
    maxLocalError_ =
        std::max(options_.maxLocalError, options_.maxLocalErrorRatio * diag);

    const int initialActiveEdgeCount =
        static_cast<int>(collectActiveEdges(faces_).size());
    maxAttemptsWithoutCollapse_ =
        std::max(1000, std::max(1, initialActiveEdgeCount) * 6);
    attemptsWithoutCollapse_ = 0;
    stalePops_ = 0;
  }

  void rebuildQueue() {
    queue_.clear();
    for (const auto& [a, b] : collectActiveEdges(faces_)) {
      queue_.pushEdge(a, b, vertices_, report_);
    }
    ++report_.queueRebuilds;
  }

  void collapseUntilTarget() {
    while (activeFaceCount_ > targetFaces_) {
      if (!ensureQueueHasCandidates()) {
        break;
      }

      const Candidate candidate = queue_.pop();
      if (!isCurrentCandidate(candidate)) {
        handleStaleCandidate();
        continue;
      }
      stalePops_ = 0;

      if (!tryCollapse(candidate.a, candidate.b)) {
        if (attemptsWithoutCollapse_ > maxAttemptsWithoutCollapse_) {
          break;
        }
        continue;
      }
      attemptsWithoutCollapse_ = 0;

      if (options_.verbose && report_.collapsedEdges % 1000 == 0) {
        std::cerr << "collapsed " << report_.collapsedEdges << ", faces "
                  << activeFaceCount_ << "/" << targetFaces_ << "\n";
      }
    }
  }

  bool ensureQueueHasCandidates() {
    if (!queue_.empty()) {
      return true;
    }
    rebuildQueue();
    return !queue_.empty();
  }

  bool isCurrentCandidate(const Candidate& candidate) const {
    const int a = candidate.a;
    const int b = candidate.b;
    return a >= 0 && b >= 0 && a < static_cast<int>(vertices_.size()) &&
           b < static_cast<int>(vertices_.size()) && vertices_[a].active &&
           vertices_[b].active && vertices_[a].version == candidate.versionA &&
           vertices_[b].version == candidate.versionB &&
           areAdjacent(a, b, faces_, *topology_);
  }

  void handleStaleCandidate() {
    if (++stalePops_ > 10000) {
      rebuildQueue();
      stalePops_ = 0;
    }
  }

  bool tryCollapse(int keep, int remove) {
    const Mat4 mergedQ = vertices_[keep].q + vertices_[remove].q;
    const SolveResult solve =
        solveOptimal(mergedQ, vertices_[keep].p, vertices_[remove].p);
    if (solve.usedFallback) {
      ++report_.solverFallbacks;
    }

    if (!featurePolicy_.canCollapse(keep, remove, vertices_, activeLoopCounts_)) {
      rejectFeatureCollapse(keep, remove);
      return false;
    }

    const BoundaryCollapseDecision boundaryDecision =
        boundaryCollapseDecision(keep, remove, faces_, vertices_, *topology_, options_);
    if (!boundaryDecision.allowed) {
      rejectBoundaryCollapse(keep, remove);
      return false;
    }

    Vec3 collapsePosition = solve.position;
    projectBoundaryPlacement(keep, remove, boundaryDecision, vertices_,
                             collapsePosition);
    if (!curveBudgetAllows(keep, remove, collapsePosition)) {
      rejectCurveBudgetCollapse(keep, remove);
      return false;
    }
    if (featurePolicy_.projectPlacement(keep, remove, vertices_, featureCurves_,
                                        collapsePosition)) {
      ++report_.projectedFeaturePlacements;
    }

    const CollapseRejectReason rejectReason = collapseRejectReason(
        keep, remove, collapsePosition, faces_, vertices_, *topology_, areaEps_,
        options_.minTriangleQuality, minNormalDot_, maxLocalError_,
        options_.preventLocalIntersections,
        options_.preventLocalIntersections ? &spatialIndex_ : nullptr);
    if (rejectReason != CollapseRejectReason::None) {
      rejectLegalityCollapse(keep, remove, rejectReason);
      return false;
    }

    applyCollapse(keep, remove, collapsePosition, mergedQ);
    return true;
  }

  void rejectFeatureCollapse(int keep, int remove) {
    (void)keep;
    (void)remove;
    ++report_.rejectedCollapses;
    ++report_.featureRejectedCollapses;
    if (++attemptsWithoutCollapse_ > maxAttemptsWithoutCollapse_ && options_.verbose) {
      std::cerr << "stopped: feature constraints leave no valid collapses\n";
    }
  }

  void rejectBoundaryCollapse(int keep, int remove) {
    (void)keep;
    (void)remove;
    ++report_.rejectedCollapses;
    ++report_.boundaryRejectedCollapses;
    if (++attemptsWithoutCollapse_ > maxAttemptsWithoutCollapse_ && options_.verbose) {
      std::cerr << "stopped: boundary constraints leave no valid collapses\n";
    }
  }

  void rejectCurveBudgetCollapse(int keep, int remove) {
    (void)keep;
    (void)remove;
    ++report_.rejectedCollapses;
    ++report_.curveBudgetRejectedCollapses;
    if (++attemptsWithoutCollapse_ > maxAttemptsWithoutCollapse_ && options_.verbose) {
      std::cerr << "stopped: feature curve budgets leave no valid collapses\n";
    }
  }

  bool curveBudgetAllows(int keep, int remove, const Vec3& position) const {
    if (!options_.preserveFeatureCurves ||
        options_.maxFeatureCurveDeviationRatio <= 0.0) {
      return true;
    }
    const VertexState& a = vertices_[keep];
    const VertexState& b = vertices_[remove];
    if (!a.isFeature || !b.isFeature || a.featureLoopId < 0 ||
        a.featureLoopId != b.featureLoopId ||
        a.featureLoopId >= static_cast<int>(featureCurves_.size())) {
      return true;
    }
    const FeatureCurveConstraint& curve = featureCurves_[a.featureLoopId];
    if (!curve.valid) {
      return true;
    }
    const double maxDistance =
        options_.maxFeatureCurveDeviationRatio * std::max(1e-12, input_.bboxDiag());
    if (a.circularFeature || b.circularFeature) {
      const Vec3 projected = projectToCircle(position, a.circularFeature ? a : b);
      return (position - projected).squaredNorm() <= maxDistance * maxDistance;
    }
    if (a.featurePrimitive == FeaturePrimitiveType::Ellipse ||
        b.featurePrimitive == FeaturePrimitiveType::Ellipse) {
      const Vec3 projected = projectToEllipse(
          position, a.featurePrimitive == FeaturePrimitiveType::Ellipse ? a : b);
      return (position - projected).squaredNorm() <= maxDistance * maxDistance;
    }
    if (curve.primitive != FeaturePrimitiveType::PolygonalLoop) {
      return true;
    }
    const int segmentCount =
        curve.closed ? static_cast<int>(curve.samples.size())
                     : std::max(0, static_cast<int>(curve.samples.size()) - 1);
    double bestDist2 = std::numeric_limits<double>::infinity();
    for (int i = 0; i < segmentCount; ++i) {
      const Vec3& p0 = curve.samples[i];
      const Vec3& p1 = curve.samples[(i + 1) % curve.samples.size()];
      const Vec3 edge = p1 - p0;
      const double len2 = edge.squaredNorm();
      Vec3 closest = p0;
      if (len2 > 1e-30) {
        const double t = std::clamp((position - p0).dot(edge) / len2, 0.0, 1.0);
        closest = p0 + t * edge;
      }
      bestDist2 = std::min(bestDist2, (position - closest).squaredNorm());
    }
    return std::isfinite(bestDist2) && bestDist2 <= maxDistance * maxDistance;
  }

  void rejectLegalityCollapse(int keep, int remove, CollapseRejectReason reason) {
    (void)keep;
    (void)remove;
    ++report_.rejectedCollapses;
    switch (reason) {
    case CollapseRejectReason::Topology:
      ++report_.topologyRejectedCollapses;
      break;
    case CollapseRejectReason::NormalFlip:
      ++report_.normalFlipRejectedCollapses;
      break;
    case CollapseRejectReason::TriangleQuality:
      ++report_.qualityRejectedCollapses;
      break;
    case CollapseRejectReason::SelfIntersection:
      ++report_.selfIntersectionRejectedCollapses;
      break;
    case CollapseRejectReason::LocalError:
      ++report_.errorRejectedCollapses;
      break;
    case CollapseRejectReason::None:
      break;
    }
    if (++attemptsWithoutCollapse_ > maxAttemptsWithoutCollapse_ && options_.verbose) {
      std::cerr << "stopped: legality checks leave no valid collapses\n";
    }
  }

  void bumpVersions(int keep, int remove) {
    vertices_[keep].version++;
    vertices_[remove].version++;
  }

  void applyCollapse(int keep, int remove, const Vec3& position, const Mat4& mergedQ) {
    const bool mergedFeatureLoop =
        vertices_[keep].isFeature && vertices_[remove].isFeature &&
        vertices_[keep].featureLoopId == vertices_[remove].featureLoopId &&
        vertices_[keep].featureLoopId >= 0 &&
        vertices_[keep].featureLoopId < static_cast<int>(activeLoopCounts_.size());

    const std::unordered_set<int> affectedFaces =
        collectAffectedFacesForCollapse(keep, remove);
    if (options_.preventLocalIntersections) {
      for (int faceId : affectedFaces) {
        spatialIndex_.removeFace(faceId);
      }
    }

    vertices_[keep].p = position;
    vertices_[keep].q = mergedQ;
    refreshCircularTangent(vertices_[keep]);
    refreshEllipseTangent(vertices_[keep]);
    vertices_[remove].active = false;
    if (mergedFeatureLoop) {
      --activeLoopCounts_[vertices_[keep].featureLoopId];
    }
    bumpVersions(keep, remove);

    rewriteIncidentFaces(keep, remove);
    if (options_.preventLocalIntersections) {
      for (int faceId : affectedFaces) {
        if (faceId >= 0 && faceId < static_cast<int>(faces_.size()) &&
            faces_[faceId].active) {
          spatialIndex_.updateFace(faceId, faces_[faceId], vertices_);
        }
      }
    }
    ++report_.collapsedEdges;

    for (int neighbor : activeNeighborsOf(keep, faces_, vertices_, *topology_)) {
      queue_.pushEdge(keep, neighbor, vertices_, report_);
    }
  }

  std::unordered_set<int> collectAffectedFacesForCollapse(int keep, int remove) const {
    std::unordered_set<int> affected;
    if (keep >= 0 && keep < static_cast<int>(topology_->vertexFaces.size())) {
      affected.insert(topology_->vertexFaces[keep].begin(),
                      topology_->vertexFaces[keep].end());
    }
    if (remove >= 0 && remove < static_cast<int>(topology_->vertexFaces.size())) {
      affected.insert(topology_->vertexFaces[remove].begin(),
                      topology_->vertexFaces[remove].end());
    }
    return affected;
  }

  void rewriteIncidentFaces(int keep, int remove) {
    const std::vector<int> removeIncidentFaces(topology_->vertexFaces[remove].begin(),
                                               topology_->vertexFaces[remove].end());
    for (int faceId : removeIncidentFaces) {
      FaceState& face = faces_[faceId];
      if (!face.active || !containsVertex(face, remove)) {
        continue;
      }
      topology_->removeFace(faceId, face);
      for (int& id : face.v) {
        if (id == remove) {
          id = keep;
        }
      }
      if (face.v[0] == face.v[1] || face.v[1] == face.v[2] || face.v[0] == face.v[2] ||
          topology_->hasDuplicateFace(faceId, face)) {
        face.active = false;
        --activeFaceCount_;
      } else {
        topology_->addFace(faceId, face);
      }
    }
  }

  const Mesh& input_;
  const SimplifyOptions& options_;
  SimplifyReport report_;
  FeatureAnalysis featureAnalysis_;
  const FeatureAnalysis* featureAnalysisPtr_ = nullptr;
  std::vector<char> boundaryVertices_;
  std::vector<VertexState> vertices_;
  std::vector<FaceState> faces_;
  std::unique_ptr<DynamicTopology> topology_;
  SpatialFaceIndex spatialIndex_;
  std::vector<int> activeLoopCounts_;
  std::vector<FeatureCurveConstraint> featureCurves_;
  CandidateQueue queue_;
  InitialQuadricBuilder quadrics_;
  FeatureConstraintPolicy featurePolicy_;
  int activeFaceCount_ = 0;
  int targetFaces_ = 0;
  double areaEps_ = 0.0;
  double minNormalDot_ = 0.0;
  double maxLocalError_ = 0.0;
  int maxAttemptsWithoutCollapse_ = 0;
  int attemptsWithoutCollapse_ = 0;
  int stalePops_ = 0;
};

} // namespace

WeightMode parseWeightMode(const std::string& value) {
  if (value == "uniform") return WeightMode::Uniform;
  if (value == "dihedral") return WeightMode::Dihedral;
  if (value == "normal-tensor" || value == "normal_tensor") {
    return WeightMode::NormalTensor;
  }
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
  case WeightMode::NormalTensor:
    return "normal-tensor";
  case WeightMode::Height:
    return "height";
  case WeightMode::XBand:
    return "xband";
  }
  return "unknown";
}

QEMSimplifier::QEMSimplifier(SimplifyOptions options) : options_(std::move(options)) {
}

void QEMSimplifier::setOptions(SimplifyOptions options) {
  options_ = std::move(options);
}

Mesh QEMSimplifier::simplify(const Mesh& input) {
  return simplify(input, nullptr);
}

Mesh QEMSimplifier::simplify(const Mesh& input, SimplifyReport* outReport) {
  validateSimplifyOptions(options_);
  validateSimplifierInput(input);
  SimplificationRun run(input, options_);
  Mesh output = run.execute(&report_);
  if (outReport) {
    *outReport = report_;
  }
  return output;
}

Mesh simplifyMesh(const Mesh& input, const SimplifyOptions& options,
                  SimplifyReport* outReport) {
  QEMSimplifier simplifier(options);
  return simplifier.simplify(input, outReport);
}

} // namespace lq
