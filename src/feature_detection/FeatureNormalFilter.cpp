/**
 * @file src/feature_detection/FeatureNormalFilter.cpp
 * @brief 实现 ManuMesh 的特征检测模块的特征法向过滤功能。
 * @ingroup manumesh_feature_detection
 *
 * @details 在不改变网格位置和拓扑的前提下稳定面法向。
 * @algorithm 每次迭代重新计算基于边角度的指示量，保留超过配置角度的间断，
 *            按面面积加权平均兼容邻面法向，并应用有界松弛。
 * @invariants 输出法向始终有限且已归一化；受保护的硬边不会跨间断交换平滑支持。
 */

#include "algorithms/feature_detection/FeatureDetector.h"

#include "common/detail/MathConstants.h"
#include "common/detail/MeshQueries.h"
#include "detail/FeatureDetectionCache.h"
#include "detail/FeatureInputValidation.h"
#include "detail/FeatureNormalFilter.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>
#include <vector>

namespace manumesh::feature {
namespace detector_detail {
namespace {

/** @brief 参与双边法向平滑的相邻面对。 */
struct FacePair {
    int first = -1;
    int second = -1;
};

double angleBetween(const Vec3& lhs, const Vec3& rhs) {
    if (lhs.squaredNorm() <= 1e-30 || rhs.squaredNorm() <= 1e-30) {
        return 0.0;
    }
    return std::acos(std::clamp(lhs.dot(rhs), -1.0, 1.0));
}

double edgeIndicator(const Vec3& lhs, const Vec3& rhs, double sigmaRad, double preserveRad) {
    if (lhs.squaredNorm() <= 1e-30 || rhs.squaredNorm() <= 1e-30) {
        return 0.0;
    }
    const double angle = angleBetween(lhs, rhs);
    if (angle >= preserveRad) {
        return 0.0;
    }
    const double normalized = angle / std::max(1e-9, sigmaRad);
    return std::exp(-0.5 * normalized * normalized);
}

std::vector<double> faceAreas(const Mesh& mesh) {
    std::vector<double> result(mesh.faces.size(), 0.0);
    for (int faceId = 0; faceId < static_cast<int>(mesh.faces.size()); ++faceId) {
        const Face& face = mesh.faces[faceId];
        result[faceId] = 0.5 * (mesh.vertices[face.v[1]] - mesh.vertices[face.v[0]])
                                   .cross(mesh.vertices[face.v[2]] - mesh.vertices[face.v[0]])
                                   .norm();
    }
    return result;
}

std::vector<FacePair> manifoldFacePairs(const common::MeshEdgeInfoMap& edgeInfo) {
    std::vector<FacePair> result;
    result.reserve(edgeInfo.size());
    for (const auto& [key, info] : edgeInfo) {
        (void)key;
        if (info.faces.size() != 2 || info.faces[0] == info.faces[1]) {
            continue;
        }
        result.push_back({std::min(info.faces[0], info.faces[1]), std::max(info.faces[0], info.faces[1])});
    }
    std::sort(result.begin(), result.end(), [](const FacePair& lhs, const FacePair& rhs) {
        return lhs.first != rhs.first ? lhs.first < rhs.first : lhs.second < rhs.second;
    });
    result.erase(
        std::unique(
            result.begin(),
            result.end(),
            [](const FacePair& lhs, const FacePair& rhs) {
                return lhs.first == rhs.first && lhs.second == rhs.second;
            }
        ),
        result.end()
    );
    return result;
}

} // 匿名命名空间

FeatureNormalFilterResult filterFeatureNormalsImpl(
    const Mesh& mesh, const common::MeshEdgeInfoMap& edgeInfo, const FeatureNormalFilterOptions& options
) {
    FeatureNormalFilterResult result;
    result.faceNormals = common::computeFaceNormals(mesh);
    if (!options.enabled || options.iterations <= 0 || mesh.faces.empty()) {
        return result;
    }

    const std::vector<char> windingFlip = common::harmonizeFaceWindings(mesh, edgeInfo);
    std::vector<Vec3> harmonized = result.faceNormals;
    for (int faceId = 0; faceId < static_cast<int>(harmonized.size()); ++faceId) {
        if (faceId < static_cast<int>(windingFlip.size()) && windingFlip[faceId]) {
            harmonized[faceId] = -harmonized[faceId];
        }
    }

    const std::vector<double> areas = faceAreas(mesh);
    const std::vector<FacePair> pairs = manifoldFacePairs(edgeInfo);
    std::vector<std::vector<int>> facePairs(mesh.faces.size());
    for (int pairId = 0; pairId < static_cast<int>(pairs.size()); ++pairId) {
        facePairs[pairs[pairId].first].push_back(pairId);
        facePairs[pairs[pairId].second].push_back(pairId);
    }

    const double sigmaRad = options.angleSigmaDeg * common::kPi / 180.0;
    const double preserveRad = options.preserveAngleDeg * common::kPi / 180.0;
    std::vector<Vec3> next = harmonized;
    for (int iteration = 0; iteration < options.iterations; ++iteration) {
        for (int faceId = 0; faceId < static_cast<int>(harmonized.size()); ++faceId) {
            if (harmonized[faceId].squaredNorm() <= 1e-30) {
                next[faceId] = Vec3::Zero();
                continue;
            }

            const double selfWeight = std::max(areas[faceId], 1e-12);
            Vec3 weighted = selfWeight * harmonized[faceId];
            double weightSum = selfWeight;
            for (int pairId : facePairs[faceId]) {
                const FacePair& pair = pairs[pairId];
                const int neighbor = pair.first == faceId ? pair.second : pair.first;
                const double indicator = edgeIndicator(harmonized[faceId], harmonized[neighbor], sigmaRad, preserveRad);
                if (indicator <= 1e-12) {
                    continue;
                }
                const double weight = indicator * std::sqrt(std::max(areas[faceId] * areas[neighbor], 1e-24));
                weighted += weight * harmonized[neighbor];
                weightSum += weight;
            }

            Vec3 target = weightSum > 0.0 ? weighted / weightSum : harmonized[faceId];
            if (target.squaredNorm() <= 1e-30) {
                next[faceId] = harmonized[faceId];
                continue;
            }
            target.normalize();
            Vec3 blended = (1.0 - options.relaxation) * harmonized[faceId] + options.relaxation * target;
            next[faceId] = blended.squaredNorm() > 1e-30 ? blended.normalized() : harmonized[faceId];
        }
        harmonized.swap(next);
        ++result.report.iterationsCompleted;
    }

    double indicatorSum = 0.0;
    int indicatorCount = 0;
    for (const FacePair& pair : pairs) {
        const double indicator = edgeIndicator(harmonized[pair.first], harmonized[pair.second], sigmaRad, preserveRad);
        indicatorSum += indicator;
        ++indicatorCount;
        if (indicator <= 0.05) {
            ++result.report.preservedEdges;
        }
    }
    if (indicatorCount > 0) {
        result.report.meanEdgeIndicator = indicatorSum / static_cast<double>(indicatorCount);
    }

    double angularChangeSum = 0.0;
    int validFaces = 0;
    for (int faceId = 0; faceId < static_cast<int>(harmonized.size()); ++faceId) {
        Vec3 filtered = harmonized[faceId];
        if (faceId < static_cast<int>(windingFlip.size()) && windingFlip[faceId]) {
            filtered = -filtered;
        }
        const Vec3 original = result.faceNormals[faceId];
        if (original.squaredNorm() > 1e-30 && filtered.squaredNorm() > 1e-30) {
            const double changeDeg = angleBetween(original, filtered) * 180.0 / common::kPi;
            angularChangeSum += changeDeg;
            result.report.maxAngularChangeDeg = std::max(result.report.maxAngularChangeDeg, changeDeg);
            if (changeDeg > 1e-8) {
                ++result.report.changedFaces;
            }
            ++validFaces;
        }
        result.faceNormals[faceId] = filtered;
    }
    if (validFaces > 0) {
        result.report.meanAngularChangeDeg = angularChangeSum / static_cast<double>(validFaces);
    }
    return result;
}

const std::vector<Vec3>& FeatureDetectionCache::faceNormals() {
    if (!hasFaceNormals_) {
        const FeatureNormalFilterResult filtered = filterFeatureNormalsImpl(*mesh_, edgeInfo(), normalFilterOptions_);
        faceNormals_ = filtered.faceNormals;
        normalFilterReport_ = filtered.report;
        hasFaceNormals_ = true;
    }
    return faceNormals_;
}

const FeatureNormalFilterReport& FeatureDetectionCache::normalFilterReport() {
    (void)faceNormals();
    return normalFilterReport_;
}

} // 命名空间 manumesh::feature::detector_detail

FeatureNormalFilterResult filterFeatureNormals(const Mesh& mesh, const FeatureNormalFilterOptions& options) {
    detector_detail::validateFeatureMeshInput(mesh);
    if (options.iterations < 0 || options.iterations > kMaxFeatureNormalFilterIterations) {
        throw std::invalid_argument(
            "FeatureNormalFilterOptions::iterations must be in [0, " +
            std::to_string(kMaxFeatureNormalFilterIterations) + "]."
        );
    }
    if (!std::isfinite(options.angleSigmaDeg) || options.angleSigmaDeg <= 0.0 || options.angleSigmaDeg > 180.0) {
        throw std::invalid_argument("FeatureNormalFilterOptions::angleSigmaDeg must be finite and in (0, 180].");
    }
    if (!std::isfinite(options.preserveAngleDeg) || options.preserveAngleDeg < 0.0 ||
        options.preserveAngleDeg > 180.0) {
        throw std::invalid_argument("FeatureNormalFilterOptions::preserveAngleDeg must be finite and in [0, 180].");
    }
    if (!std::isfinite(options.relaxation) || options.relaxation < 0.0 || options.relaxation > 1.0) {
        throw std::invalid_argument("FeatureNormalFilterOptions::relaxation must be finite and in [0, 1].");
    }
    const common::MeshEdgeInfoMap edgeInfo = common::buildMeshEdgeInfo(mesh);
    return detector_detail::filterFeatureNormalsImpl(mesh, edgeInfo, options);
}

} // 命名空间 manumesh::feature
