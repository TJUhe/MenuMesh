/**
 * @file src/simplification/detail/Placement.h
 * @brief 声明边坍缩候选位置求解与边界投影。
 * @ingroup manumesh_simplification
 *
 * @details 该层只处理位置几何，不决定拓扑、特征或纹理合法性。
 */

#pragma once

#include "detail/SimplificationTypes.h"
#include "mesh_edit/detail/DynamicTopology.h"

#include <vector>

namespace manumesh {
namespace simplification {

// 边折叠的放置策略。本单元负责“合并后的顶点应放在哪里”，合法性检查由 CollapseTopology/CollapseLegality 负责，特征曲线约束由 FeatureConstraints 负责。

/**
 * @brief Lindstrom-Turk 放置投影所需的有向边界链几何量。
 */
struct BoundaryProjectionInput {
    CollapseEdge edge;
    const BoundaryCollapseDecision& decision;
    const std::vector<VertexState>& vertices;
    const std::vector<FaceState>& faces;
    const mesh_edit::DynamicTopology& topology;
};

/**
 * @brief 使用 Lindstrom-Turk 边界保持约束（M032 4.2.2）处理边界边折叠：将放置点投影到使相邻边界链有向面积变化最小的直线，再夹到收缩边在该直线上的投影。局部边界链退化时回退到线段 [keep, remove] 上的夹紧。该函数将 position 投影到局部边界目标和安全线段上。
 * @return 成功生成有限约束位置时返回 true。
 */
bool projectBoundaryPlacement(const BoundaryProjectionInput& input, Vec3& position);

} // namespace simplification
} // namespace manumesh
