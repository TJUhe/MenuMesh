/**
 * @file src/feature_detection/detail/FeatureSegmentation.h
 * @brief 声明 ManuMesh 特征检测模块的曲面面片分割功能。
 * @ingroup manumesh_feature_detection
 *
 * @details 本文件属于确定性的三角曲面特征流水线。局部证据与图清理、轨迹追踪、
 *          图元恢复及面片分割相互独立，各阶段均有明确的接口契约。
 */

#pragma once

#include "algorithms/feature_detection/FeatureTypes.h"

namespace manumesh::feature::detector_detail {

/**
 * @brief 沿非特征流形邻接泛洪填充，并汇总曲面面片。
 */
void buildFeaturePatches(const Mesh& mesh, FeatureAnalysis& analysis, const SurfacePatchOptions& options);

} // 命名空间 manumesh::feature::detector_detail
