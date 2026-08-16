/**
 * @file src/feature_detection/detail/FeaturePrimitiveRecovery.h
 * @brief 声明未归属图分量的几何基元曲线恢复。
 * @ingroup manumesh_feature_detection
 */

#pragma once

#include "FeatureDetectionTypes.h"
#include "algorithms/feature_detection/FeatureTypes.h"

namespace manumesh {
namespace feature {
namespace detector_detail {

/**
 * @brief 为尚未归属的连通分量拟合并生成几何基元特征环。
 */
void recoverPrimitiveComponents(
    const Mesh& mesh, const FeatureOptions& options, const TraceGraph& trace, FeatureAnalysis& analysis, int& loopId
);

} // namespace detector_detail
} // namespace feature
} // namespace manumesh
