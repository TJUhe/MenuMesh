/**
 * @file src/feature_detection/detail/FeatureGraphCleanup.h
 * @brief 声明弱毛刺裁剪、局部间隙桥接和组件汇总。
 * @ingroup manumesh_feature_detection
 */

#pragma once

#include "FeatureDetectionCache.h"
#include "FeatureDetectionTypes.h"
#include "algorithms/feature_detection/FeatureTypes.h"

namespace manumesh {
namespace feature {
namespace detector_detail {

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

} // namespace detector_detail
} // namespace feature
} // namespace manumesh
