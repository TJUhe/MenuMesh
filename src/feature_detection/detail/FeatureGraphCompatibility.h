/**
 * @file src/feature_detection/detail/FeatureGraphCompatibility.h
 * @brief 声明 ManuMesh 特征检测模块的图边兼容性查询功能。
 * @ingroup manumesh_feature_detection
 *
 * @details 本文件属于确定性的三角曲面特征流水线。局部证据与图清理、轨迹追踪、
 *          图元恢复及面片分割相互独立，各阶段均有明确的接口契约。
 */

#pragma once

#include "FeatureDetectionTypes.h"
#include "core/Mesh.h"

namespace manumesh::feature::detector_detail {

/**
 * @brief 保存穿过图顶点的最佳证据兼容分支。
 */
struct ContinuationBranch {
    int neighbor = -1;                     /**< 邻接顶点；没有合格分支时为 -1。 */
    const TraceEdgeAttrs* attrs = nullptr; /**< 被选图边的属性。 */
    double alignment = 0.0;                /**< 绝对切线对齐度，范围为 [0, 1]。 */
};

/**
 * @brief 选择从 vertex 朝 target 延伸时最共线的分支。
 */
ContinuationBranch
bestContinuationBranch(const Mesh& mesh, const TraceGraph& trace, int vertex, int target, double minAlignment);

/**
 * @return 两条边可参与同一条恢复延续链时返回 true。
 */
bool compatibleFeatureEvidence(const TraceEdgeAttrs* lhs, const TraceEdgeAttrs* rhs);

/**
 * @return 兼容的凸/凹符号；未知或不兼容输入返回 0。
 */
int compatibleSignedKind(const TraceEdgeAttrs* lhs, const TraceEdgeAttrs* rhs);

} // 命名空间 manumesh::feature::detector_detail
