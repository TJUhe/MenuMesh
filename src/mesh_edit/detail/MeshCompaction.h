/**
 * @file src/mesh_edit/detail/MeshCompaction.h
 * @brief 声明编辑状态压缩和旧索引到新索引的映射。
 * @ingroup manumesh_mesh_edit
 *
 * @details 编辑期间保持索引稳定，仅将面和顶点标记为非活动；确定性的压缩过程负责生成最终的稠密网格。
 */

#pragma once

#include "core/Mesh.h"
#include "mesh_edit/detail/MeshEditTypes.h"

#include <vector>

namespace manumesh {
namespace mesh_edit {

/**
 * @brief 稠密输出网格，以及编辑索引到输出索引的稳定映射。
 */
struct MeshCompactionResult {
    Mesh mesh;
    std::vector<int> oldToNewVertices;
    std::vector<int> oldToNewFaces;
};

/**
 * @brief 根据编辑期间的顶点位置、活动标志和面列表构建稠密 Mesh。
 *
 * 活动面若引用非活动或无效顶点，则会被省略。
 * 顶点顺序具有确定性：被接受面首次引用的顶点优先分配索引。
 */
MeshCompactionResult compactActiveMesh(
    const std::vector<Vec3>& positions, const std::vector<char>& activeVertices, const std::vector<EditableFace>& faces
);

} // namespace mesh_edit
} // namespace manumesh
