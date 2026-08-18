/**
 * @file src/feature_detection/NormalTensor.cpp
 * @brief 计算确定性的多尺度法向张量特征证据。
 * @ingroup manumesh_feature_detection
 *
 * @details 实现确定性的多尺度法向张量投票。
 * @algorithm 在每个顶点和尺度上，对相邻面法向累加加权外积；有序特征值差编码
 *            曲面、折痕和角点显著性，最小特征值对应向量提供折痕切线；持久性支持必须
 *            持续达到配置的尺度数量。
 * @failuremodes 孤立、退化或各向同性邻域返回零显著性及确定性的备用坐标系。
 */

#include "algorithms/feature_detection/FeatureDetector.h"
#include "common/detail/MeshQueries.h"
#include "common/detail/ParallelExecution.h"
#include "core/MathUtils.h"
#include "detail/FeatureDetectionCache.h"
#include "detail/FeatureInputValidation.h"
#include "detail/FeatureNormalFilter.h"

#include <Eigen/Eigenvalues>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace manumesh {
namespace feature {
namespace {

constexpr double kMinScaleTangentConsistency = 0.65;

struct NormalTensorScaleEvidence {
    NormalTensorScaleEvidence()
        : creaseTangent(Vec3::Zero()),
          featureScore(0.0),
          creaseDominant(false) {}

    explicit NormalTensorScaleEvidence(const NormalTensorVertex& candidate)
        : creaseTangent(candidate.creaseTangent),
          featureScore(candidate.featureScore),
          creaseDominant(candidate.creaseSaliency >= candidate.cornerSaliency) {}

    Vec3 creaseTangent;
    double featureScore;
    bool creaseDominant;
};

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

double normalTensorScaleRadiusMultiplier(int scale) { return 1.0 + 0.5 * static_cast<double>(std::max(0, scale)); }

double normalTensorEffectiveRadius(double localEdgeLength, int baseIterations, int selectedScale) {
    if (!(localEdgeLength > 0.0) || selectedScale < 0) {
        return 0.0;
    }
    // 初始面投票已经覆盖顶点的一环。后续每次平滑近似为独立高斯扩散，
    // 因此用各次名义半径的平方和报告累计支持宽度。
    double squaredMultiplier = 1.0 + static_cast<double>(std::max(0, baseIterations));
    for (int scale = 1; scale <= selectedScale; ++scale) {
        const double multiplier = normalTensorScaleRadiusMultiplier(scale);
        squaredMultiplier += multiplier * multiplier;
    }
    return localEdgeLength * std::sqrt(squaredMultiplier);
}

bool isMeaningfullyStrongerScore(double candidate, double reference) {
    const double tolerance = 1e-12 * std::max({1.0, std::abs(candidate), std::abs(reference)});
    return candidate > reference + tolerance;
}

bool isCreaseDominant(const NormalTensorVertex& candidate) {
    return candidate.creaseSaliency >= candidate.cornerSaliency;
}

bool hasConsistentCreaseTangent(const NormalTensorScaleEvidence& candidate, const NormalTensorVertex& reference) {
    return std::abs(candidate.creaseTangent.dot(reference.creaseTangent)) >= kMinScaleTangentConsistency;
}

bool supportsReferenceFeature(const NormalTensorScaleEvidence& candidate, const NormalTensorVertex& reference) {
    if (isCreaseDominant(reference)) {
        return candidate.creaseDominant && hasConsistentCreaseTangent(candidate, reference);
    }
    return !candidate.creaseDominant;
}

void validateNormalTensorOptions(const NormalTensorOptions& options) {
    if (options.smoothingIterations < 0 || options.smoothingIterations > kMaxNormalTensorSmoothingIterations) {
        throw std::invalid_argument(
            "NormalTensorOptions::smoothingIterations must be in [0, " +
            std::to_string(kMaxNormalTensorSmoothingIterations) + "]."
        );
    }
    if (options.scaleCount < 1 || options.scaleCount > kMaxNormalTensorScaleCount) {
        throw std::invalid_argument(
            "NormalTensorOptions::scaleCount must be in [1, " + std::to_string(kMaxNormalTensorScaleCount) + "]."
        );
    }
    detector_detail::validateFeatureNormalFilterOptions(options.normalFilter);
}

void validatePersistenceThreshold(double threshold) {
    if (!std::isfinite(threshold) || threshold < 0.0) {
        throw std::invalid_argument("Normal Tensor persistence threshold must be finite and non-negative.");
    }
}

} // namespace

namespace detector_detail {

std::vector<NormalTensorVertex> computeNormalTensorFeaturesCached(
    const Mesh& mesh,
    FeatureDetectionCache& cache,
    const NormalTensorOptions& options,
    double requestedPersistenceThreshold,
    const common::parallel::RangeExecutionOptions& executionOptions
) {
    detector_detail::validateFeatureMeshInput(mesh);
    std::vector<NormalTensorVertex> result(mesh.vertices.size());
    if (mesh.empty()) {
        return result;
    }

    std::vector<Eigen::Matrix3d> tensors(mesh.vertices.size(), Eigen::Matrix3d::Zero());
    std::vector<double> weights(mesh.vertices.size(), 0.0);
    const std::vector<Vec3>& faceNormals = cache.faceNormals();
    for (std::size_t faceId = 0; faceId < mesh.faces.size(); ++faceId) {
        const Face& face = mesh.faces[faceId];
        const Vec3& a = mesh.vertices[face.v[0]];
        const Vec3& b = mesh.vertices[face.v[1]];
        const Vec3& c = mesh.vertices[face.v[2]];
        const Vec3& normal = faceNormals[faceId];
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
    common::parallel::forEachRange(0, tensors.size(), executionOptions, [&](std::size_t begin, std::size_t end) {
        for (std::size_t vertexId = begin; vertexId < end; ++vertexId) {
            if (weights[vertexId] > 1e-24) {
                tensors[vertexId] /= weights[vertexId];
            }
        }
    });

    const std::vector<std::vector<int>>& neighbors = cache.vertexNeighbors();
    const std::vector<double>& localScale = cache.vertexAverageEdgeLength();
    const int baseIterations =
        manumesh::clampValue(options.smoothingIterations, 0, kMaxNormalTensorSmoothingIterations);
    const int scaleCount = manumesh::clampValue(options.scaleCount, 1, kMaxNormalTensorScaleCount);
    const double persistenceThreshold =
        std::isfinite(requestedPersistenceThreshold) ? std::max(1e-12, requestedPersistenceThreshold) : 1e-12;

    // 双缓冲平滑：在各次平滑迭代之间复用一个临时张量数组，
    // 避免每次调用都复制完整张量集合。
    std::vector<Eigen::Matrix3d> smoothScratch(tensors.size(), Eigen::Matrix3d::Zero());
    auto smoothOnce = [&](std::vector<Eigen::Matrix3d>& current, double radiusMultiplier) {
        std::vector<Eigen::Matrix3d>& next = smoothScratch;
        common::parallel::forEachRange(0, current.size(), executionOptions, [&](std::size_t begin, std::size_t end) {
            for (std::size_t vertexId = begin; vertexId < end; ++vertexId) {
                if (neighbors[vertexId].empty()) {
                    next[vertexId] = current[vertexId];
                    continue;
                }
                const double radius = std::max(1e-12, localScale[vertexId] * std::max(0.25, radiusMultiplier));
                Eigen::Matrix3d sum = current[vertexId];
                double weightSum = 1.0;
                for (int neighborId : neighbors[vertexId]) {
                    const double distance = (mesh.vertices[neighborId] - mesh.vertices[vertexId]).norm();
                    const double normalized = distance / radius;
                    const double weight = std::exp(-normalized * normalized);
                    if (weight <= 1e-12) {
                        continue;
                    }
                    sum += weight * current[neighborId];
                    weightSum += weight;
                }
                next[vertexId] = sum / weightSum;
            }
        });
        current.swap(next);
    };

    for (int iter = 0; iter < baseIterations; ++iter) {
        smoothOnce(tensors, 1.0);
    }

    const std::size_t vertexCount = mesh.vertices.size();
    std::vector<double> scoreSums(vertexCount, 0.0);
    // Persistence only needs the per-scale score, feature kind and crease tangent.
    // Keeping this compact evidence avoids a second tensor propagation/eigendecomposition pass.
    std::vector<NormalTensorScaleEvidence> scaleEvidence(vertexCount * static_cast<std::size_t>(scaleCount));
    for (int scale = 0; scale < scaleCount; ++scale) {
        const double radiusMultiplier = normalTensorScaleRadiusMultiplier(scale);
        common::parallel::forEachRange(0, tensors.size(), executionOptions, [&](std::size_t begin, std::size_t end) {
            for (std::size_t vertexId = begin; vertexId < end; ++vertexId) {
                NormalTensorVertex candidate = analyzeNormalTensor(tensors[vertexId]);
                scaleEvidence[static_cast<std::size_t>(scale) * vertexCount + vertexId] =
                    NormalTensorScaleEvidence(candidate);
                const double vertexScale = vertexId < localScale.size() ? localScale[vertexId] : 0.0;
                candidate.localScale = vertexScale * radiusMultiplier;
                candidate.selectedScale = scale;
                candidate.smoothingSteps = baseIterations + scale;
                candidate.effectiveRadius = normalTensorEffectiveRadius(vertexScale, baseIterations, scale);
                scoreSums[vertexId] += candidate.featureScore;
                if (scale == 0 || isMeaningfullyStrongerScore(candidate.featureScore, result[vertexId].featureScore)) {
                    result[vertexId] = candidate;
                }
            }
        });
        if (scale + 1 < scaleCount) {
            smoothOnce(tensors, normalTensorScaleRadiusMultiplier(scale + 1));
        }
    }

    for (int scale = 0; scale < scaleCount; ++scale) {
        common::parallel::forEachRange(0, result.size(), executionOptions, [&](std::size_t begin, std::size_t end) {
            for (std::size_t vertexId = begin; vertexId < end; ++vertexId) {
                NormalTensorVertex& vertex = result[vertexId];
                const NormalTensorScaleEvidence& evidence =
                    scaleEvidence[static_cast<std::size_t>(scale) * vertexCount + vertexId];
                if (vertex.featureScore >= persistenceThreshold && evidence.featureScore >= persistenceThreshold &&
                    supportsReferenceFeature(evidence, vertex)) {
                    ++vertex.persistentScales;
                }
            }
        });
    }

    common::parallel::forEachRange(0, result.size(), executionOptions, [&](std::size_t begin, std::size_t end) {
        for (std::size_t vertexId = begin; vertexId < end; ++vertexId) {
            NormalTensorVertex& vertex = result[vertexId];
            vertex.averageFeatureScore = scoreSums[vertexId] / static_cast<double>(scaleCount);
            const double persistenceRatio = manumesh::clampValue(
                static_cast<double>(vertex.persistentScales) / static_cast<double>(scaleCount), 0.0, 1.0
            );
            const double robustScore = 0.65 * vertex.featureScore + 0.35 * vertex.averageFeatureScore;
            vertex.persistentFeatureScore = robustScore * persistenceRatio;
        }
    });
    return result;
}

std::vector<NormalTensorVertex> computeNormalTensorFeaturesCached(
    const Mesh& mesh,
    FeatureDetectionCache& cache,
    const NormalTensorOptions& options,
    double requestedPersistenceThreshold
) {
    return computeNormalTensorFeaturesCached(
        mesh,
        cache,
        options,
        requestedPersistenceThreshold,
        common::parallel::makeRangeExecutionOptions(cache.executionOptions())
    );
}

} // namespace detector_detail

std::vector<NormalTensorVertex> computeNormalTensorFeatures(
    const Mesh& mesh, const NormalTensorOptions& options, double requestedPersistenceThreshold
) {
    return computeNormalTensorFeatures(mesh, options, requestedPersistenceThreshold, ExecutionOptions{});
}

std::vector<NormalTensorVertex> computeNormalTensorFeatures(
    const Mesh& mesh, const NormalTensorOptions& options, const ExecutionOptions& executionOptions
) {
    return computeNormalTensorFeatures(mesh, options, 0.0, executionOptions);
}

std::vector<NormalTensorVertex> computeNormalTensorFeatures(
    const Mesh& mesh,
    const NormalTensorOptions& options,
    double requestedPersistenceThreshold,
    const ExecutionOptions& executionOptions
) {
    validateNormalTensorOptions(options);
    validatePersistenceThreshold(requestedPersistenceThreshold);
    detector_detail::validateFeatureMeshInput(mesh);
    validateExecutionOptions(executionOptions);
    detector_detail::FeatureDetectionCache cache(mesh, options.normalFilter, executionOptions);
    return detector_detail::computeNormalTensorFeaturesCached(mesh, cache, options, requestedPersistenceThreshold);
}

std::vector<NormalTensorVertex> computeNormalTensorFeatures(const Mesh& mesh, const NormalTensorOptions& options) {
    return computeNormalTensorFeatures(mesh, options, 0.0);
}

} // namespace feature
} // namespace manumesh
