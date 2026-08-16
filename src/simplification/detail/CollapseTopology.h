/**
 * @file src/simplification/detail/CollapseTopology.h
 * @brief 声明边坍缩的链接条件、边界分类和局部重写计划。
 * @ingroup manumesh_simplification
 *
 * @details 拓扑层不计算 QEM 代价，也不执行特征曲线投影。
 */

#pragma once

#include "algorithms/simplification/SimplificationTypes.h"
#include "detail/SimplificationTypes.h"
#include "mesh_edit/detail/DynamicTopology.h"

#include <vector>

namespace manumesh {
namespace simplification {

/** @copydoc mesh_edit::activeIncidentFaceCountForEdge */
using mesh_edit::activeIncidentFaceCountForEdge;
/** @copydoc mesh_edit::areAdjacent */
using mesh_edit::areAdjacent;
/** @copydoc mesh_edit::collectActiveEdges */
using mesh_edit::collectActiveEdges;
/** @copydoc mesh_edit::containsVertex */
using mesh_edit::containsVertex;
using mesh_edit::DynamicTopology;

/**
 * @brief 判定开放边界折叠所需的局部关联和位置数据。
 */
struct BoundaryCollapseInput {
    CollapseEdge edge;
    const std::vector<FaceState>& faces;
    const std::vector<VertexState>& vertices;
    const DynamicTopology& topology;
    const SimplifyOptions& options;
};

/**
 * @brief 对边界折叠进行分类，并在可能时施加约束。
 */
BoundaryCollapseDecision boundaryCollapseDecision(const BoundaryCollapseInput& input);

/** @brief 返回顶点活动一环邻居的升序列表。*/
std::vector<int> activeNeighborsOf(
    int vertex,
    const std::vector<FaceState>& faces,
    const std::vector<VertexState>& vertices,
    const DynamicTopology& topology
);

/**
 * @brief 检查单纯形链接条件 link(keep) ∩ link(remove) = link(edge)，同时包含端点链接中的顶点和边。边界扩展会拒绝两个端点都位于开放边界上的内部弦，并拒绝会删除孤立开放三角形的折叠。
 */
bool collapseWouldPreserveLinkCondition(
    int keep,
    int remove,
    const std::vector<FaceState>& faces,
    const std::vector<VertexState>& vertices,
    const DynamicTopology& topology
);

} // namespace simplification
} // namespace manumesh
