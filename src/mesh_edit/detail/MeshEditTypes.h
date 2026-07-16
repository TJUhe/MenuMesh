/**
 * @file src/mesh_edit/detail/MeshEditTypes.h
 * @brief Declares mesh edit types facilities for ManuMesh's mesh-editing module.
 * @ingroup manumesh_mesh_edit
 *
 * @details Edit-time indices remain stable while faces and vertices are marked inactive; deterministic compaction creates the final dense mesh.
 */

#pragma once

#include <array>

namespace manumesh::mesh_edit {

/**
 * @brief Triangle record shared by topology-editing algorithms.
 *
 * Algorithms may mark faces inactive while preserving stable face indices.
 * compactActiveMesh() converts this edit-time representation back to Mesh.
 */
struct EditableFace {
    std::array<int, 3> v{};
    bool active = true;
};

} // namespace manumesh::mesh_edit
