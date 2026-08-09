/**
 * @file src/feature_detection/detail/FeatureGraphConsolidation.h
 * @brief 声明 ManuMesh 特征检测模块的轨迹图合并功能。
 * @ingroup manumesh_feature_detection
 *
 * @details 本文件属于确定性的三角曲面特征流水线。局部证据与图清理、轨迹追踪、
 *          图元恢复及面片分割相互独立，各阶段均有明确的接口契约。
 */

#pragma once

#include "FeatureDetectionCache.h"
#include "FeatureDetectionTypes.h"
#include "algorithms/feature_detection/FeatureTypes.h"

namespace manumesh::feature::detector_detail {

/**
 * @brief 在相邻图分量的对齐端点之间桥接，且要求证据兼容。
 */
void consolidateFeatureGraph(
    const Mesh& mesh,
    const FeatureOptions& options,
    FeatureDetectionCache& cache,
    TraceGraph& trace,
    FeatureAnalysis& analysis
);

} // 命名空间 manumesh::feature::detector_detail
