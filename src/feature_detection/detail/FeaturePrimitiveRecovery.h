/**
 * @file src/feature_detection/detail/FeaturePrimitiveRecovery.h
 * @brief 声明未归属图分量的几何基元曲线恢复。
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
 * @brief 为尚未归属的连通分量拟合并生成几何基元特征环。
 * @param[in] mesh 输入三角网格。
 * @param[in] options 特征检测选项。
 * @param[in] trace 已清理、可用于识别未归属分量的轨迹图。
 * @param[in,out] analysis 写入恢复出的特征分析结果。
 * @param[in,out] loopId 分配新特征环 ID 的计数器。
 * @param[in] executionOptions 仅约束独立基元拟合；结果提交仍按组件顺序串行完成。
 */
void recoverPrimitiveComponents(
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
