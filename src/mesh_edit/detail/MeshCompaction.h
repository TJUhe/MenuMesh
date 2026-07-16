/**
 * @file src/mesh_edit/detail/MeshCompaction.h
 * @brief Declares mesh compaction facilities for ManuMesh's mesh-editing module.
 * @ingroup manumesh_mesh_edit
 *
 * @details Edit-time indices remain stable while faces and vertices are marked inactive; deterministic compaction creates the final dense mesh.
 */

#pragma once

#include "core/Mesh.h"
#include "mesh_edit/detail/MeshEditTypes.h"

#include <vector>

namespace manumesh::mesh_edit {

/**
 * @brief Dense output plus stable edit-to-output index maps.
 */
struct MeshCompactionResult {
    Mesh mesh;
    std::vector<int> oldToNewVertices;
    std::vector<int> oldToNewFaces;
};

/**
 * @brief Builds a dense Mesh from edit-time positions, activity flags, and faces.
 *
 * Active faces that reference an inactive or invalid vertex are omitted.
 * Vertex order is deterministic: first use by an accepted face wins.
 */
MeshCompactionResult compactActiveMesh(
    const std::vector<Vec3>& positions, const std::vector<char>& activeVertices, const std::vector<EditableFace>& faces
);

} // namespace manumesh::mesh_edit
