#include "algorithms/feature_detection/FeatureDetector.h"
#include "common/detail/MeshQueries.h"

#include <Eigen/Eigenvalues>
#include <algorithm>
#include <cmath>

namespace manumesh::feature {
namespace {

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

  const std::vector<std::vector<int>> neighbors =
      manumesh::detail::buildVertexNeighbors(mesh);
  const std::vector<double> localScale =
      manumesh::detail::computeVertexAverageEdgeLength(mesh);
  const int baseIterations = std::clamp(options.smoothingIterations, 0, 8);
  const int scaleCount = std::clamp(options.scaleCount, 1, 8);

  auto smoothOnce = [&](std::vector<Eigen::Matrix3d>& current,
                        double radiusMultiplier) {
    std::vector<Eigen::Matrix3d> next = current;
    for (int i = 0; i < static_cast<int>(current.size()); ++i) {
      if (neighbors[i].empty()) {
        continue;
      }
      const double radius =
          std::max(1e-12, localScale[i] * std::max(0.25, radiusMultiplier));
      Eigen::Matrix3d sum = current[i];
      double weightSum = 1.0;
      for (int nb : neighbors[i]) {
        const double distance = (mesh.vertices[nb] - mesh.vertices[i]).norm();
        const double normalized = distance / radius;
        const double weight = std::exp(-normalized * normalized);
        if (weight <= 1e-12) {
          continue;
        }
        sum += weight * current[nb];
        weightSum += weight;
      }
      next[i] = sum / weightSum;
    }
    current.swap(next);
  };

  for (int iter = 0; iter < baseIterations; ++iter) {
    smoothOnce(tensors, 1.0);
  }

  for (int scale = 0; scale < scaleCount; ++scale) {
    for (int i = 0; i < static_cast<int>(tensors.size()); ++i) {
      NormalTensorVertex candidate = analyzeNormalTensor(tensors[i]);
      candidate.localScale =
          i < static_cast<int>(localScale.size()) ? localScale[i] : 0.0;
      result[i].averageFeatureScore += candidate.featureScore;
      if (candidate.featureScore > 1e-12) {
        ++result[i].persistentScales;
      }
      if (scale == 0 || candidate.featureScore > result[i].featureScore) {
        const double accumulatedAverage = result[i].averageFeatureScore;
        const int accumulatedPersistence = result[i].persistentScales;
        result[i] = candidate;
        result[i].averageFeatureScore = accumulatedAverage;
        result[i].persistentScales = accumulatedPersistence;
      }
    }
    if (scale + 1 < scaleCount) {
      smoothOnce(tensors, 1.0 + 0.5 * static_cast<double>(scale + 1));
    }
  }
  for (NormalTensorVertex& vertex : result) {
    vertex.averageFeatureScore /= static_cast<double>(scaleCount);
    const double persistenceRatio = std::clamp(
        static_cast<double>(vertex.persistentScales) / static_cast<double>(scaleCount),
        0.0, 1.0);
    const double robustScore =
        0.65 * vertex.featureScore + 0.35 * vertex.averageFeatureScore;
    vertex.persistentFeatureScore = robustScore * (0.5 + 0.5 * persistenceRatio);
  }
  return result;
}

} // namespace manumesh::feature
