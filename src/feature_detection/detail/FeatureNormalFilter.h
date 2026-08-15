/**
 * @file src/feature_detection/detail/FeatureNormalFilter.h
 * @brief 声明 ManuMesh 特征检测模块的法向滤波功能。
 * @ingroup manumesh_feature_detection
 *
 * @details 本文件属于确定性的三角曲面特征流水线。局部证据与图清理、轨迹追踪、
 *          图元恢复及面片分割相互独立，各阶段均有明确的接口契约。
 */

#pragma once

#include "algorithms/feature_detection/FeatureTypes.h"
#include "common/detail/MeshQueries.h"

namespace manumesh {
namespace feature {
namespace detector_detail {

void validateFeatureNormalFilterOptions(const FeatureNormalFilterOptions& options);

/**
 * @brief 复用预计算边面关联的内部法向滤波入口。
 */
FeatureNormalFilterResult filterFeatureNormalsImpl(
    const Mesh& mesh, const common::MeshEdgeInfoMap& edgeInfo, const FeatureNormalFilterOptions& options
);

} // namespace detector_detail
} // namespace feature
} // namespace manumesh
