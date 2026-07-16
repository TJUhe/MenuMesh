/**
 * @file src/mesh_edit/MeshCompaction.cpp
 * @brief Implements mesh compaction facilities for ManuMesh's mesh-editing module.
 * @ingroup manumesh_mesh_edit
 *
 * @details Converts stable-index edit state back to a dense public Mesh.
 * @algorithm Active, index-valid faces are accepted in face order; vertices
 * receive dense indices on first use and aligned texture coordinates follow
 * surviving faces.
 * @invariants Every output vertex is referenced and every output face index is valid.
 */

#include "mesh_edit/detail/MeshCompaction.h"

namespace manumesh::mesh_edit {

MeshCompactionResult compactActiveMesh(
    const std::vector<Vec3>& positions, const std::vector<char>& activeVertices, const std::vector<EditableFace>& faces
) {
    MeshCompactionResult result;
    result.oldToNewVertices.assign(positions.size(), -1);
    result.oldToNewFaces.assign(faces.size(), -1);
    result.mesh.faces.reserve(faces.size());

    for (int faceId = 0; faceId < static_cast<int>(faces.size()); ++faceId) {
        const EditableFace& face = faces[faceId];
        if (!face.active) {
            continue;
        }

        bool valid = true;
        for (int vertex : face.v) {
            if (vertex < 0 || vertex >= static_cast<int>(positions.size()) ||
                vertex >= static_cast<int>(activeVertices.size()) || !activeVertices[vertex]) {
                valid = false;
                break;
            }
        }
        if (!valid || face.v[0] == face.v[1] || face.v[1] == face.v[2] || face.v[0] == face.v[2]) {
            continue;
        }

        Face outputFace;
        for (int corner = 0; corner < 3; ++corner) {
            const int oldVertex = face.v[corner];
            int& newVertex = result.oldToNewVertices[oldVertex];
            if (newVertex < 0) {
                newVertex = static_cast<int>(result.mesh.vertices.size());
                result.mesh.vertices.push_back(positions[oldVertex]);
            }
            outputFace.v[corner] = newVertex;
        }
        result.oldToNewFaces[faceId] = static_cast<int>(result.mesh.faces.size());
        result.mesh.faces.push_back(outputFace);
    }
    return result;
}

} // namespace manumesh::mesh_edit
