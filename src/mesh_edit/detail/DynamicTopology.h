/**
 * @file src/mesh_edit/detail/DynamicTopology.h
 * @brief 声明 ManuMesh 的网格编辑模块的动态拓扑功能。
 * @ingroup manumesh_mesh_edit
 *
 * @details 编辑期间保持索引稳定，仅将面和顶点标记为非活动；确定性的压缩过程负责生成最终的稠密网格。
 */

#pragma once

#include "common/detail/MeshQueries.h"
#include "mesh_edit/detail/MeshEditTypes.h"

#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace manumesh::mesh_edit {

/** @brief 判断可编辑面是否引用指定的顶点编号。*/
bool containsVertex(const EditableFace& face, int vertex);

/**
 * @brief 为保持稳定索引的编辑算法提供可变面关联缓存。
 */
struct DynamicTopology {
    std::vector<std::unordered_set<int>> vertexFaces;
    std::unordered_map<std::array<int, 3>, std::unordered_set<int>, common::FaceKeyHash> facesByKey;

    /** @brief 为初始处于活动状态且索引有效的面构建关联缓存。*/
    DynamicTopology(const std::vector<EditableFace>& faces, int vertexCount);

    /**
     * @brief 将一个面登记到两个缓存中。若面引用任何越界顶点，则整体拒绝（两个缓存都不登记），从而保持 vertexFaces 与 facesByKey 的一致性。
     */
    void addFace(int faceId, const EditableFace& face);
    /** @brief 从顶点关联缓存和重复键缓存中移除一个面。*/
    void removeFace(int faceId, const EditableFace& face);
    /** @brief 判断是否存在另一个活动面具有相同的规范化键。*/
    bool hasDuplicateFace(int faceId, const EditableFace& face) const;
};

/** @brief 从所有活动面收集规范化的无向边。*/
std::vector<std::pair<int, int>> collectActiveEdges(const std::vector<EditableFace>& faces);

/** @brief 判断两个顶点是否共享某条活动面边。*/
bool areAdjacent(int a, int b, const std::vector<EditableFace>& faces, const DynamicTopology& topology);

/** @brief 统计与一条无向边相邻的活动面数量。*/
int activeIncidentFaceCountForEdge(
    int a, int b, const std::vector<EditableFace>& faces, const DynamicTopology& topology
);

} // 结束 manumesh::mesh_edit 命名空间
