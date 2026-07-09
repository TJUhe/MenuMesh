#include "detail/Quadrics.h"

#include "algorithms/feature_detection/FeatureDetector.h"
#include "common/detail/MathConstants.h"
#include "common/detail/MeshQueries.h"

#include <Eigen/Eigenvalues>
#include <algorithm>
#include <cmath>
#include <limits>

namespace manumesh::simplification {

using manumesh::detail::kPi;

namespace {

struct FeatureScoreResult {
  std::vector<double> values;
  int normalTensorScoredVertices = 0;
  double maxNormalTensorPersistentScore = 0.0;
  double meanNormalTensorLocalScale = 0.0;
  double meanNormalTensorPersistence = 0.0;
};

int resolveNormalTensorMinPersistentScales(const SimplifyOptions& options) {
  return std::clamp(options.normalTensorMinPersistentScales, 1,
                    std::max(1, options.normalTensorScaleCount));
}

void summarizeNormalTensorScores(const std::vector<feature::NormalTensorVertex>& tensor,
                                 FeatureScoreResult& result) {
  double localScaleSum = 0.0;
  double persistenceSum = 0.0;
  for (const feature::NormalTensorVertex& vertex : tensor) {
    result.maxNormalTensorPersistentScore =
        std::max(result.maxNormalTensorPersistentScore, vertex.persistentFeatureScore);
    if (vertex.featureScore <= 1e-12 && vertex.persistentFeatureScore <= 1e-12) {
      continue;
    }
    ++result.normalTensorScoredVertices;
    localScaleSum += vertex.localScale;
    persistenceSum += static_cast<double>(vertex.persistentScales);
  }

  if (result.normalTensorScoredVertices > 0) {
    const double count = static_cast<double>(result.normalTensorScoredVertices);
    result.meanNormalTensorLocalScale = localScaleSum / count;
    result.meanNormalTensorPersistence = persistenceSum / count;
  }
}

} // namespace

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

FeatureScoreResult computeFeatureScores(const Mesh& mesh,
                                        const SimplifyOptions& options) {
  const WeightMode mode = options.weightMode;
  FeatureScoreResult result;
  std::vector<double>& score = result.values;
  score.assign(mesh.vertices.size(), 0.0);
  if (mode == WeightMode::Uniform) {
    return result;
  }

  const Vec3 lo = mesh.bboxMin();
  const Vec3 hi = mesh.bboxMax();
  const Vec3 span = hi - lo;

  if (mode == WeightMode::Height) {
    const double denom = std::max(1e-12, span.z());
    for (int i = 0; i < static_cast<int>(mesh.vertices.size()); ++i) {
      score[i] = std::clamp((mesh.vertices[i].z() - lo.z()) / denom, 0.0, 1.0);
    }
    return result;
  }

  if (mode == WeightMode::XBand) {
    const double denom = std::max(1e-12, span.x());
    for (int i = 0; i < static_cast<int>(mesh.vertices.size()); ++i) {
      const double x = (mesh.vertices[i].x() - lo.x()) / denom;
      score[i] = std::exp(-80.0 * (x - 0.5) * (x - 0.5));
    }
    return result;
  }

  if (mode == WeightMode::NormalTensor) {
    const std::vector<feature::NormalTensorVertex> tensor =
        feature::computeNormalTensorFeatures(
            mesh, feature::NormalTensorOptions{options.normalTensorSmoothingIterations,
                                               options.normalTensorScaleCount});
    summarizeNormalTensorScores(tensor, result);
    const int requiredPersistentScales =
        resolveNormalTensorMinPersistentScales(options);
    for (int i = 0; i < static_cast<int>(tensor.size()); ++i) {
      if (tensor[i].persistentScales >= requiredPersistentScales) {
        score[i] = tensor[i].persistentFeatureScore;
      }
    }
    return result;
  }

  const std::vector<Vec3> faceNormals = detail::computeFaceNormals(mesh);

  const detail::MeshEdgeInfoMap edgeInfo = detail::buildMeshEdgeInfo(mesh);
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
      const auto [a, b] = detail::unpackMeshEdgeKey(key);
      score[a] = std::max(score[a], edgeScore);
      score[b] = std::max(score[b], edgeScore);
    }
  }
  return result;
}

void addBoundaryQuadrics(const Mesh& mesh, double boundaryWeight,
                         std::vector<Mat4>& quadrics) {
  if (boundaryWeight <= 0.0) {
    return;
  }
  const std::vector<Vec3> faceNormals = detail::computeFaceNormals(mesh);

  const detail::MeshEdgeInfoMap edgeInfo = detail::buildMeshEdgeInfo(mesh);
  for (const auto& [key, info] : edgeInfo) {
    if (info.faces.size() != 1) {
      continue;
    }
    const auto [a, b] = detail::unpackMeshEdgeKey(key);
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
                            const feature::FeatureAnalysis* featureAnalysis,
                            std::vector<Mat4>& quadrics, SimplifyReport& report) {
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

  report.minAppliedLineWeight = std::numeric_limits<double>::infinity();
  report.maxAppliedLineWeight = 0.0;
  const bool useNormalLineQuadrics =
      options.useLineQuadrics && options.lineWeight > 0.0;
  if (!useNormalLineQuadrics) {
    report.minAppliedLineWeight = 0.0;
  }

  const FeatureScoreResult featureScores = useNormalLineQuadrics
                                               ? computeFeatureScores(mesh, options)
                                               : FeatureScoreResult{};
  if (featureScores.normalTensorScoredVertices > 0) {
    report.normalTensorScoredVertices = std::max(
        report.normalTensorScoredVertices, featureScores.normalTensorScoredVertices);
    report.maxNormalTensorPersistentScore =
        std::max(report.maxNormalTensorPersistentScore,
                 featureScores.maxNormalTensorPersistentScore);
    report.meanNormalTensorLocalScale = featureScores.meanNormalTensorLocalScale;
    report.meanNormalTensorPersistence = featureScores.meanNormalTensorPersistence;
  }

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
        appliedWeight += options.featureBoost * featureScores.values[i];
      }

      if (options.adaptiveScale) {
        quadrics[i] += options.adaptiveBaseLineWeight * vertexArea[i] * ql;
        quadrics[i] *=
            (1.0 + std::max(0.0, options.featureBoost) * featureScores.values[i]);
      } else {
        quadrics[i] += appliedWeight * vertexArea[i] * ql;
      }
      report.minAppliedLineWeight =
          std::min(report.minAppliedLineWeight, appliedWeight);
      report.maxAppliedLineWeight =
          std::max(report.maxAppliedLineWeight, appliedWeight);
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
      const feature::VertexFeature& vf = featureAnalysis->vertices[i];
      if (!vf.isFeature || vf.tangent.norm() <= 1e-20) {
        continue;
      }
      const double areaScale = std::max(vertexArea[i], fallbackArea);
      const Mat4 qCurve = lineQuadric(mesh.vertices[i], vf.tangent);
      const double confidenceScale = 0.35 + 0.65 * std::clamp(vf.confidence, 0.0, 1.0);
      quadrics[i] += options.featureCurveWeight * confidenceScale * areaScale * qCurve;
    }
  }

  if (!std::isfinite(report.minAppliedLineWeight)) {
    report.minAppliedLineWeight = 0.0;
  }
}

std::vector<SolveResult> solvePlacementCandidates(const Mat4& q, const Vec3& a,
                                                  const Vec3& b) {
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

  std::vector<SolveResult> results;
  results.reserve(candidates.size());
  for (const Vec3& p : candidates) {
    const double cost = evaluateQuadric(q, p);
    if (!std::isfinite(cost)) {
      continue;
    }
    bool duplicate = false;
    for (const SolveResult& existing : results) {
      if ((existing.position - p).squaredNorm() <= 1e-24) {
        duplicate = true;
        break;
      }
    }
    if (!duplicate) {
      results.push_back(SolveResult{p, cost, !solved});
    }
  }
  if (results.empty()) {
    results.push_back(SolveResult{0.5 * (a + b), 0.0, true});
  }
  std::stable_sort(results.begin(), results.end(),
                   [](const SolveResult& lhs, const SolveResult& rhs) {
                     return lhs.cost < rhs.cost;
                   });
  return results;
}

SolveResult solveOptimal(const Mat4& q, const Vec3& a, const Vec3& b) {
  return solvePlacementCandidates(q, a, b).front();
}

InitialQuadricBuilder::InitialQuadricBuilder(const SimplifyOptions& options)
    : options_(options) {
}

std::vector<Mat4>
InitialQuadricBuilder::build(const Mesh& mesh,
                             const feature::FeatureAnalysis* featureAnalysis,
                             SimplifyReport& report) const {
  std::vector<Mat4> quadrics;
  computeInitialQuadrics(mesh, options_, featureAnalysis, quadrics, report);
  return quadrics;
}

} // namespace manumesh::simplification
