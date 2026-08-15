/**
 * @file src/mesh_edit/MeshCompaction.cpp
 * @brief 实现 ManuMesh 的网格编辑模块的网格压缩功能。
 * @ingroup manumesh_mesh_edit
 *
 * @details 将保持稳定索引的编辑状态转换回稠密的公共 Mesh。
 * @algorithm 按面顺序接受仍处于活动状态且索引有效的面；顶点按首次被接受面引用的顺序分配稠密索引，并同步保留对应的纹理坐标。
 * @invariants 每个输出顶点都被引用，且每个输出面索引都有效。
 */

#include "mesh_edit/detail/MeshCompaction.h"

namespace manumesh {
namespace mesh_edit {

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

} // namespace mesh_edit
} // namespace manumesh
