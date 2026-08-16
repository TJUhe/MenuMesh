/**
 * @file src/feature_detection/detail/FeatureCycleRecovery.h
 * @brief 声明分叉环和有界图环基恢复入口。
 * @ingroup manumesh_feature_detection
 */

#pragma once

#include "FeatureDetectionTypes.h"
#include "algorithms/feature_detection/FeatureTypes.h"

namespace manumesh {
namespace feature {
namespace detector_detail {

/**
 * @brief 查找经过高阶顶点且满足几何基元拟合条件的特征环。
 */
void recoverCircularCyclesThroughJunctions(
    const Mesh& mesh, const FeatureOptions& options, const TraceGraph& trace, FeatureAnalysis& analysis, int& loopId
);

/**
 * @brief 为仍未归属的图边恢复一个有界且确定性的环基。
 */
void recoverSmallCycleBasis(
    const Mesh& mesh, const FeatureOptions& options, const TraceGraph& trace, FeatureAnalysis& analysis, int& loopId
);

} // namespace detector_detail
} // namespace feature
} // namespace manumesh
