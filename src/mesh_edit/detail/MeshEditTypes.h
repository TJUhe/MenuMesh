/**
 * @file src/mesh_edit/detail/MeshEditTypes.h
 * @brief 声明 ManuMesh 的网格编辑模块的网格编辑类型功能。
 * @ingroup manumesh_mesh_edit
 *
 * @details 编辑期间保持索引稳定，仅将面和顶点标记为非活动；确定性的压缩过程负责生成最终的稠密网格。
 */

#pragma once

#include <array>

namespace manumesh {
namespace mesh_edit {

/**
 * @brief 网格拓扑编辑算法共享的三角形记录。
 *
 * 算法可以在保持面索引稳定的同时将面标记为非活动。
 * compactActiveMesh() 会把这种编辑期间的表示转换回 Mesh。
 */
struct EditableFace {
    std::array<int, 3> v{};
    bool active = true;
};

} // namespace mesh_edit
} // namespace manumesh
