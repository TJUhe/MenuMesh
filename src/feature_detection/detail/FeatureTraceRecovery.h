/**
 * @file src/feature_detection/detail/FeatureTraceRecovery.h
 * @brief 声明 ManuMesh 特征检测模块的轨迹追踪恢复功能。
 * @ingroup manumesh_feature_detection
 *
 * @details 本文件属于确定性的三角曲面特征流水线。局部证据与图清理、轨迹追踪、
 *          图元恢复及面片分割相互独立，各阶段均有明确的接口契约。
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
