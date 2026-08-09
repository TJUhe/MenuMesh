/**
 * @file src/feature_detection/detail/FeatureCircularRecovery.h
 * @brief 声明 ManuMesh 特征检测模块的圆形特征恢复功能。
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
 * @brief 从空间上连贯的特征顶点簇中恢复闭合圆形特征环。
 * @param[in] mesh 源三角网格。
 * @param[in] options 图元残差和最小规模筛选策略。
 * @param[in] trace 已清理的特征轨迹图。
 * @param[in,out] analysis 写入恢复出的环、归属关系和诊断信息。
 * @param[in,out] loopId 为接受的特征环分配单调递增 ID。
 */
void recoverCircularVertexClusters(
    const Mesh& mesh, const FeatureOptions& options, const TraceGraph& trace, FeatureAnalysis& analysis, int& loopId
);

} // 命名空间 manumesh::feature::detector_detail
