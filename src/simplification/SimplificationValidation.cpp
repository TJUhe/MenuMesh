/**
 * @file src/simplification/SimplificationValidation.cpp
 * @brief 在创建运行状态前校验简化选项和输入网格。
 * @ingroup manumesh_simplification
 *
 * @details 在分配可变运行状态前校验网格和选项契约。
 * @failuremodes 在 C++ 边界处，遇到无效索引、非有限几何体或选项、重复面顶点、不可能的持久化计数以及不一致的目标范围时抛出 invalid_argument。
 */

#include "detail/SimplificationValidation.h"

#include "algorithms/feature_detection/FeatureDetector.h"
#include "core/MeshTopology.h"
#include "detail/SimplificationPolicies.h"

#include <cmath>
#include <stdexcept>
#include <string>

namespace manumesh {
namespace simplification {
namespace {

void requireFiniteNonNegative(double value, const char* name) {
    if (!std::isfinite(value) || value < 0.0) {
        throw std::invalid_argument(std::string(name) + " must be finite and non-negative.");
    }
}

} // 结束匿名命名空间

void validateSimplifyOptions(const SimplifyOptions& options) {
    if (options.targetFaces == 0 || options.targetFaces < -1) {
        throw std::invalid_argument("targetFaces must be -1 or a positive face count.");
    }
    if (options.targetFaces < 0 &&
        (!std::isfinite(options.targetRatio) || options.targetRatio <= 0.0 || options.targetRatio > 1.0)) {
        throw std::invalid_argument("targetRatio must be finite and in the range (0, 1].");
    }
    requireFiniteNonNegative(options.lineWeight, "lineWeight");
    requireFiniteNonNegative(options.featureBoost, "featureBoost");
    requireFiniteNonNegative(options.adaptiveBaseLineWeight, "adaptiveBaseLineWeight");
    requireFiniteNonNegative(options.boundaryWeight, "boundaryWeight");
    requireFiniteNonNegative(options.textureWeight, "textureWeight");
    requireFiniteNonNegative(options.textureSeamTolerance, "textureSeamTolerance");
    requireFiniteNonNegative(options.minTextureAreaRatio, "minTextureAreaRatio");
    if (options.minTextureAreaRatio > 1.0) {
        throw std::invalid_argument("minTextureAreaRatio must be in [0, 1].");
    }
    requireFiniteNonNegative(options.featureCurveWeight, "featureCurveWeight");
    requireFiniteNonNegative(options.maxFeatureCurveDeviationRatio, "maxFeatureCurveDeviationRatio");
    feature::validateFeatureOptions(featureOptionsFromSimplifyOptions(options));
    if (options.minCircularFeatureLoopVertices < 3) {
        throw std::invalid_argument("minCircularFeatureLoopVertices must be at least 3.");
    }
    requireFiniteNonNegative(options.minTriangleQuality, "minTriangleQuality");
    if (options.minTriangleQuality > 1.0) {
        throw std::invalid_argument("minTriangleQuality must be in [0, 1].");
    }
    requireFiniteNonNegative(options.maxLocalError, "maxLocalError");
    requireFiniteNonNegative(options.maxLocalErrorRatio, "maxLocalErrorRatio");
    if (options.qualityRefinementIterations < 0) {
        throw std::invalid_argument("qualityRefinementIterations must be non-negative.");
    }
    if (!std::isfinite(options.maxNormalDeviationDeg) || options.maxNormalDeviationDeg < 0.0 ||
        options.maxNormalDeviationDeg > 180.0) {
        throw std::invalid_argument("maxNormalDeviationDeg must be finite and in [0, 180].");
    }
}

void validateSimplifierInput(const Mesh& input) {
    // 宽松的几何校验允许零面积面存在（QEM 流水线会通过退化面二次误差回退项处理，并在 SimplifyReport::degenerateInputFaces 中报告）；这里只拒绝任何算法都无法安全处理的输入。
    std::string error;
    if (!validateMeshGeometryLenient(input, &error)) {
        throw std::invalid_argument(error);
    }
    if (input.empty()) {
        return;
    }
    const Result<MeshTopology> topology = MeshTopology::build(input);
    if (!topology.ok()) {
        throw std::invalid_argument(topology.status().message());
    }
}

} // namespace simplification
} // namespace manumesh
