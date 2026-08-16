/**
 * @file src/feature_detection/detail/FeatureGraphConsolidation.h
 * @brief 声明跨图分量的弱特征间隙恢复。
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
 * @brief 在相邻图分量的对齐端点之间桥接，且要求证据兼容。
 */
void consolidateFeatureGraph(
    const Mesh& mesh,
    const FeatureOptions& options,
    FeatureDetectionCache& cache,
    TraceGraph& trace,
    FeatureAnalysis& analysis
);

} // namespace detector_detail
} // namespace feature
} // namespace manumesh
