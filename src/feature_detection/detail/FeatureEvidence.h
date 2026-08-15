/**
 * @file src/feature_detection/detail/FeatureEvidence.h
 * @brief 声明 ManuMesh 特征检测模块的边证据收集功能。
 * @ingroup manumesh_feature_detection
 *
 * @details 本文件属于确定性的三角曲面特征流水线。局部证据与图清理、轨迹追踪、
 *          图元恢复及面片分割相互独立，各阶段均有明确的接口契约。
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
