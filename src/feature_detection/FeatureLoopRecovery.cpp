/**
 * @file src/feature_detection/FeatureLoopRecovery.cpp
 * @brief 按固定顺序运行全部特征曲线恢复方法。
 * @ingroup manumesh_feature_detection
 *
 * @details 本文件执行确定性的三角曲面特征环恢复流水线，将局部证据与图清理、
 *          轨迹追踪及几何基元拟合阶段串联起来。
 */

#include "detail/FeatureLoopRecovery.h"

#include "detail/FeatureCircularRecovery.h"
#include "detail/FeatureCycleRecovery.h"
#include "detail/FeaturePrimitiveRecovery.h"
#include "detail/FeatureTraceRecovery.h"

namespace manumesh {
namespace feature {
namespace detector_detail {

void recoverFeatureLoops(
    const Mesh& mesh,
    const FeatureOptions& options,
    const TraceGraph& trace,
    FeatureAnalysis& analysis,
    int& loopId,
    const common::parallel::RangeExecutionOptions& executionOptions
) {
    recoverCircularCyclesThroughJunctions(mesh, options, trace, analysis, loopId);
    recoverSmallCycleBasis(mesh, options, trace, analysis, loopId);
    traceRemainingFeatureLoops(mesh, options, trace, analysis, loopId);
    recoverPrimitiveComponents(mesh, options, trace, analysis, loopId, executionOptions);
    recoverCircularVertexClusters(mesh, options, trace, analysis, loopId);
}

} // namespace detector_detail
} // namespace feature
} // namespace manumesh
