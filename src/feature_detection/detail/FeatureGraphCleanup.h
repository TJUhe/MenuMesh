/**
 * @file src/feature_detection/detail/FeatureGraphCleanup.h
 * @brief 声明 ManuMesh 特征检测模块的轨迹图清理功能。
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
 * @brief 裁剪弱毛刺、桥接兼容的短间隙，并重写图诊断信息。
 */
void cleanupTraceGraph(
    const Mesh& mesh,
    const FeatureOptions& options,
    FeatureDetectionCache& cache,
    TraceGraph& trace,
    FeatureAnalysis& analysis
);

/**
 * @brief 计算连通分量的证据比例、闭合度和置信度。
 */
void summarizeFeatureComponents(
    const Mesh& mesh, const FeatureOptions& options, const TraceGraph& trace, FeatureAnalysis& analysis
);

} // 命名空间 manumesh::feature::detector_detail
