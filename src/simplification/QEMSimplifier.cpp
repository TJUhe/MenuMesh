#include "line_quadrics_qem/simplification/QEMSimplifier.h"

#include "line_quadrics_qem/core/MeshTopology.h"
#include "line_quadrics_qem/features/FeatureDetection.h"
#include "simplification/CollapseLegality.h"
#include "simplification/DynamicTopology.h"
#include "simplification/SimplificationTypes.h"
#include "simplification/SpatialFaceIndex.h"

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
  if (!validateMeshGeometry(input, &error)) {
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
    if (activeFaceCount_ <= targetFaces_) {
      report_.terminationReason =
          report_.collapsedEdges > 0
              ? SimplifyTerminationReason::ReachedTarget
              : SimplifyTerminationReason::AlreadyAtOrBelowTarget;
      return;
    }

    while (activeFaceCount_ > targetFaces_) {
      if (!ensureQueueHasCandidates()) {
        report_.terminationReason = SimplifyTerminationReason::NoCandidates;
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
          report_.terminationReason = SimplifyTerminationReason::RejectionLimit;
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

    if (activeFaceCount_ <= targetFaces_) {
      report_.terminationReason = SimplifyTerminationReason::ReachedTarget;
    } else if (report_.terminationReason == SimplifyTerminationReason::NotStarted) {
      report_.terminationReason = SimplifyTerminationReason::NoCandidates;
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

std::string toString(SimplifyTerminationReason reason) {
  switch (reason) {
  case SimplifyTerminationReason::NotStarted:
    return "not-started";
  case SimplifyTerminationReason::ReachedTarget:
    return "reached-target";
  case SimplifyTerminationReason::AlreadyAtOrBelowTarget:
    return "already-at-or-below-target";
  case SimplifyTerminationReason::NoCandidates:
    return "no-candidates";
  case SimplifyTerminationReason::RejectionLimit:
    return "rejection-limit";
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
