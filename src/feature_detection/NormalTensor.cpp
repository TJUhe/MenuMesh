#include "common/detail/MeshQueries.h"
#include "line_quadrics_qem/algorithms/feature_detection/FeatureDetector.h"

#include <Eigen/Eigenvalues>
#include <algorithm>

namespace lq {
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

  const std::vector<std::vector<int>> neighbors = detail::buildVertexNeighbors(mesh);
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

} // namespace lq
