/**
 * @file src/simplification/detail/CollapseLegality.h
 * @brief 声明依赖候选位置的拓扑和几何合法性检查。
 * @ingroup manumesh_simplification
 *
 * @details 检查只读取活动状态，并返回稳定拒绝类别；它不修改网格或报告。
 */

#pragma once

#include "common/detail/MeshDistanceIndex.h"
#include "core/Mesh.h"
#include "detail/CollapseTopology.h"
#include "detail/SpatialFaceIndex.h"

#include <vector>

namespace manumesh {
namespace simplification {

/**
 * @brief 供合法性谓词使用的只读活动顶点、面和拓扑视图。
 */
struct MeshStateView {
    const std::vector<FaceState>& faces;
    const std::vector<VertexState>& vertices;
    const DynamicTopology& topology;
};

/**
 * @brief 候选边放置及所有启用的几何接受阈值。
 */
struct CollapseLegalityInput {
    CollapseEdge edge;
    Vec3 newPosition = Vec3::Zero();
    MeshStateView mesh;
    double areaEps = 0.0;
    double minTriangleQuality = 0.0;
    double minNormalDot = -1.0;
    double maxLocalError = 0.0;
    bool preventLocalIntersections = false;
    const SpatialFaceIndex* spatialIndex = nullptr;
    const manumesh::common::MeshDistanceIndex* referenceSurface = nullptr;
};

/**
 * @brief 收集一次边折叠涉及的全部活动面编号。
 *
 * 同一候选的多个放置只改变新顶点位置，涉及面集合不变，因此调用方可以缓存该结果。
 */
std::vector<int> collectCollapseTouchedFaces(const CollapseLegalityInput& input);

/**
 * @brief 在边拓扑已通过 collapseWouldPreserveLinkCondition() 后，评估依赖放置位置的合法性。
 * 依次检查拓扑、退化、法向、质量、误差和相交门控条件。
 * @return 所有启用的硬检查均通过时返回 None，否则返回首个拒绝原因。
 */
CollapseRejectReason
collapsePlacementRejectReason(const CollapseLegalityInput& input, const std::vector<int>* cachedTouchedFaces = nullptr);

} // namespace simplification
} // namespace manumesh
