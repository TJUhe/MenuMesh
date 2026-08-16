/**
 * @file src/feature_detection/detail/FeatureTraceRecovery.h
 * @brief 声明剩余开放链和二度顶点环的追踪入口。
 * @ingroup manumesh_feature_detection
 */

#pragma once

#include "FeatureDetectionTypes.h"
#include "algorithms/feature_detection/FeatureTypes.h"

namespace manumesh {
namespace feature {
namespace detector_detail {

/**
 * @brief 对剩余开放链和二度顶点环各追踪一次。
 */
void traceRemainingFeatureLoops(
    const Mesh& mesh, const FeatureOptions& options, const TraceGraph& trace, FeatureAnalysis& analysis, int& loopId
);

} // namespace detector_detail
} // namespace feature
} // namespace manumesh
