/**
 * @file src/feature_detection/detail/FeaturePrimitiveRecovery.h
 * @brief 声明 ManuMesh 特征检测模块的图元特征恢复功能。
 * @ingroup manumesh_feature_detection
 *
 * @details 本文件属于确定性的三角曲面特征流水线。局部证据与图清理、轨迹追踪、
 *          图元恢复及面片分割相互独立，各阶段均有明确的接口契约。
 */

#pragma once

#include "FeatureDetectionTypes.h"
#include "algorithms/feature_detection/FeatureTypes.h"

namespace manumesh::feature::detector_detail {

/**
 * @brief 为尚未归属的连通分量拟合并生成图元特征环。
 */
void recoverPrimitiveComponents(
    const Mesh& mesh, const FeatureOptions& options, const TraceGraph& trace, FeatureAnalysis& analysis, int& loopId
);

} // 命名空间 manumesh::feature::detector_detail
