/**
 * @file src/feature_detection/detail/FeatureCycleRecovery.h
 * @brief 声明 ManuMesh 特征检测模块的图环恢复功能。
 * @ingroup manumesh_feature_detection
 *
 * @details 本文件属于确定性的三角曲面特征流水线。局部证据与图清理、轨迹追踪、
 *          图元恢复及面片分割相互独立，各阶段均有明确的接口契约。
 */

#pragma once

#include "FeatureDetectionTypes.h"
#include "algorithms/feature_detection/FeatureTypes.h"

namespace manumesh::feature::detector_detail {

/**
 * @brief 查找经过高阶顶点且满足图元拟合条件的特征环。
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

} // 命名空间 manumesh::feature::detector_detail
