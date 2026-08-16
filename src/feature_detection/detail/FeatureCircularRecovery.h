/**
 * @file src/feature_detection/detail/FeatureCircularRecovery.h
 * @brief 声明稀疏圆形顶点簇的恢复入口。
 * @ingroup manumesh_feature_detection
 */

#pragma once

#include "FeatureDetectionTypes.h"
#include "algorithms/feature_detection/FeatureTypes.h"

namespace manumesh {
namespace feature {
namespace detector_detail {

/**
 * @brief 从空间上连贯的特征顶点簇中恢复闭合圆形特征环。
 * @param[in] mesh 源三角网格。
 * @param[in] options 几何基元残差和最小规模筛选策略。
 * @param[in] trace 已清理的特征轨迹图。
 * @param[in,out] analysis 写入恢复出的环、归属关系和诊断信息。
 * @param[in,out] loopId 为接受的特征环分配单调递增 ID。
 */
void recoverCircularVertexClusters(
    const Mesh& mesh, const FeatureOptions& options, const TraceGraph& trace, FeatureAnalysis& analysis, int& loopId
);

} // namespace detector_detail
} // namespace feature
} // namespace manumesh
