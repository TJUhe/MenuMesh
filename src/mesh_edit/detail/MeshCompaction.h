#pragma once

#include "core/Mesh.h"
#include "mesh_edit/detail/MeshEditTypes.h"

#include <vector>

namespace manumesh::mesh_edit {

/// Dense output plus stable edit-to-output index maps.
struct MeshCompactionResult {
    Mesh mesh;
    std::vector<int> oldToNewVertices;
    std::vector<int> oldToNewFaces;
};

/// Builds a dense Mesh from edit-time positions, activity flags, and faces.
///
/// Active faces that reference an inactive or invalid vertex are omitted.
/// Vertex order is deterministic: first use by an accepted face wins.
MeshCompactionResult compactActiveMesh(
    const std::vector<Vec3>& positions, const std::vector<char>& activeVertices, const std::vector<EditableFace>& faces
);

} // namespace manumesh::mesh_edit
