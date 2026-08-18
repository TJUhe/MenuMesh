/**
 * @file src/feature_detection/detail/FeatureLoopRecovery.h
 * @brief 声明全部特征曲线恢复方法的固定调用顺序。
 * @ingroup manumesh_feature_detection
 */

#pragma once

#include "common/detail/ParallelExecution.h"
#include "FeatureDetectionTypes.h"
#include "algorithms/feature_detection/FeatureTypes.h"

namespace manumesh {
namespace feature {
namespace detector_detail {

/**
 * @brief 按顺序执行已清理轨迹图上的全部特征环恢复阶段。
 * @param[in] executionOptions 传递给可安全拆分的候选几何计算阶段。
 */
void recoverFeatureLoops(
    const Mesh& mesh,
    const FeatureOptions& options,
    const TraceGraph& trace,
    FeatureAnalysis& analysis,
    int& loopId,
    const common::parallel::RangeExecutionOptions& executionOptions
);

} // namespace detector_detail
} // namespace feature
} // namespace manumesh
