/**
 * @file src/simplification/detail/CollapseLegality.h
 * @brief 声明 ManuMesh 的简化模块的折叠合法性功能。
 * @ingroup manumesh_simplification
 *
 * @details 本文件属于带特征感知的边折叠流水线。二次误差代价用于排序候选；拓扑、几何、特征、边界、误差及可选纹理策略共同决定一个放置是否可以修改网格。
 */

#pragma once

#include "common/detail/MeshDistanceIndex.h"
#include "core/Mesh.h"
#include "detail/CollapseTopology.h"
#include "detail/SpatialFaceIndex.h"

#include <vector>

namespace manumesh::simplification {

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
 * @brief 在边拓扑已通过 collapseWouldPreserveLinkCondition() 后，评估依赖放置位置的合法性。
 * 依次检查拓扑、退化、法向、质量、误差和相交门控条件。
 * @return 所有启用的硬检查均通过时返回 None，否则返回首个拒绝原因。
 */
CollapseRejectReason collapsePlacementRejectReason(const CollapseLegalityInput& input);

} // 结束 manumesh::simplification 命名空间
