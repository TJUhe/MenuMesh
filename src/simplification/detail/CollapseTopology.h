/**
 * @file src/simplification/detail/CollapseTopology.h
 * @brief 声明 ManuMesh 的简化模块的折叠拓扑功能。
 * @ingroup manumesh_simplification
 *
 * @details 本文件属于带特征感知的边折叠流水线。二次误差代价用于排序候选；拓扑、几何、特征、边界、误差及可选纹理策略共同决定一个放置是否可以修改网格。
 */

#pragma once

#include "algorithms/simplification/SimplificationTypes.h"
#include "detail/SimplificationTypes.h"
#include "mesh_edit/detail/DynamicTopology.h"

#include <vector>

namespace manumesh::simplification {

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

} // 结束 manumesh::simplification 命名空间
