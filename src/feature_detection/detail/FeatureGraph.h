/**
 * @file src/feature_detection/detail/FeatureGraph.h
 * @brief 声明 ManuMesh 特征检测模块的轨迹图操作功能。
 * @ingroup manumesh_feature_detection
 *
 * @details 本文件属于确定性的三角曲面特征流水线。局部证据与图清理、轨迹追踪、
 *          图元恢复及面片分割相互独立，各阶段均有明确的接口契约。
 */

#pragma once

#include "FeatureDetectionTypes.h"
#include "algorithms/feature_detection/FeatureTypes.h"

#include <vector>

namespace manumesh::feature::detector_detail {

/**
 * @brief 将候选边复制到公共图，并初始化顶点存储。
 */
void initializeFeatureGraph(const std::vector<CandidateEdge>& featureEdges, FeatureAnalysis& analysis);

/**
 * @brief 构建确定性的邻接表和紧凑边属性，供轨迹追踪使用。
 */
TraceGraph buildTraceGraph(
    const Mesh& mesh,
    const FeatureOptions& options,
    const std::vector<CandidateEdge>& featureEdges,
    FeatureAnalysis& analysis
);

/**
 * @brief 返回边 (a, b) 的属性记录；若轨迹图中不存在该边则返回 nullptr。
 * 热点循环应一次取得该记录，避免多次调用单属性查询函数。
 */
const TraceEdgeAttrs* traceEdgeAttrs(const TraceGraph& trace, int a, int b);

/**
 * @name 轨迹边证据属性查询
 * 边不存在时返回 false 或 0。
 * @{
 */
bool traceEdgeBoundary(const TraceGraph& trace, int a, int b);
bool traceEdgeDihedral(const TraceGraph& trace, int a, int b);
bool traceEdgeNormalTensor(const TraceGraph& trace, int a, int b);
bool traceEdgeSmoothCurvature(const TraceGraph& trace, int a, int b);
bool traceEdgeNonManifold(const TraceGraph& trace, int a, int b);
bool traceEdgeCleanupBridge(const TraceGraph& trace, int a, int b);
int traceEdgeSign(const TraceGraph& trace, int a, int b);
double traceEdgeTensorPersistence(const TraceGraph& trace, int a, int b);
int traceEdgeTensorPersistentScales(const TraceGraph& trace, int a, int b);
double traceEdgeCurvaturePersistence(const TraceGraph& trace, int a, int b);
int traceEdgeCurvaturePersistentScales(const TraceGraph& trace, int a, int b);
/**
 * @}
 */

/**
 * @return 若无向边处于活动状态则返回 true。
 */
bool traceGraphHasEdge(const TraceGraph& trace, int a, int b);
/**
 * @brief 添加一条边，并保持邻接、属性和公共诊断信息一致。
 */
void addTraceGraphEdge(TraceGraph& trace, FeatureAnalysis& analysis, const CandidateEdge& edge);
/**
 * @brief 从邻接表和属性中移除一条活动边。
 */
void removeTraceGraphEdge(TraceGraph& trace, int a, int b);
/**
 * @brief 根据当前活动属性重建确定性的扁平边列表。
 */
void rebuildTraceGraphEdges(TraceGraph& trace);

/**
 * @brief 在全部图变更后重新计算公共特征和分叉标记。
 */
void finalizeFeatureGraphMarkers(const Mesh& mesh, FeatureAnalysis& analysis);

} // 命名空间 manumesh::feature::detector_detail
