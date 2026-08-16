/**
 * @file src/feature_detection/detail/FeatureEvidence.h
 * @brief 声明强、弱边证据的统一收集入口。
 * @ingroup manumesh_feature_detection
 */

#pragma once

#include "FeatureDetectionCache.h"
#include "FeatureDetectionTypes.h"
#include "algorithms/feature_detection/FeatureTypes.h"

#include <vector>

namespace manumesh {
namespace feature {
namespace detector_detail {

/**
 * @brief 收集强证据和弱证据，并更新来源统计计数。
 * @return 按确定性顺序排列的候选特征边。
 */
std::vector<CandidateEdge> collectFeatureEdges(
    const Mesh& mesh, const FeatureOptions& options, FeatureDetectionCache& cache, FeatureAnalysisBuilder& builder
);

} // namespace detector_detail
} // namespace feature
} // namespace manumesh
