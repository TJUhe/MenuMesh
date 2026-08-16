/**
 * @file src/feature_detection/detail/FeatureSegmentation.h
 * @brief 声明由活动特征边约束的曲面片分割。
 * @ingroup manumesh_feature_detection
 */

#pragma once

#include "algorithms/feature_detection/FeatureTypes.h"

namespace manumesh {
namespace feature {
namespace detector_detail {

/**
 * @brief 将面积加权面法向和转换为稳定的面片代表法向。
 *
 * 对闭合或近似闭合、合法向因对称性相消的面片，返回与 FeaturePatch 默认值一致的 +Z，
 * 避免把浮点累加残差归一化为依赖遍历顺序的任意方向。
 */
Vec3 resolveSurfacePatchNormal(const Vec3& normalSum, double patchArea);

/**
 * @brief 沿非特征流形邻接泛洪填充，并汇总曲面面片。
 */
void buildFeaturePatches(const Mesh& mesh, FeatureAnalysis& analysis, const SurfacePatchOptions& options);

} // namespace detector_detail
} // namespace feature
} // namespace manumesh
