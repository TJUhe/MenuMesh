#include "core/PlainMesh.h"

#include "core/Mesh.h"

namespace manumesh {

Mesh toMesh(const PlainMesh& plain) {
    Mesh mesh;
    mesh.vertices.reserve(plain.vertices.size());
    mesh.faces.reserve(plain.faces.size());
    for (const PlainVec3& vertex : plain.vertices) {
        mesh.vertices.emplace_back(vertex.x, vertex.y, vertex.z);
    }
    for (const PlainFace& face : plain.faces) {
        mesh.faces.push_back(Face{face.v});
    }
    return mesh;
}

PlainMesh toPlainMesh(const Mesh& mesh) {
    PlainMesh plain;
    plain.vertices.reserve(mesh.vertices.size());
    plain.faces.reserve(mesh.faces.size());
    for (const Vec3& vertex : mesh.vertices) {
        plain.vertices.push_back(PlainVec3{vertex.x(), vertex.y(), vertex.z()});
    }
    for (const Face& face : mesh.faces) {
        plain.faces.push_back(PlainFace{face.v});
    }
    return plain;
}

} // namespace manumesh
