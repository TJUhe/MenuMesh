/**
 * @file src/feature_detection/detail/FeatureNormalFilter.h
 * @brief 声明保持特征的面法向滤波入口。
 * @ingroup manumesh_feature_detection
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
