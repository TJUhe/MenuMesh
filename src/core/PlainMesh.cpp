/**
 * @file src/core/PlainMesh.cpp
 * @brief 在 PlainMesh 与内部 Mesh 之间复制几何和逐角 UV。
 * @ingroup manumesh_core
 *
 * @details 转换保持顶点、面和纹理坐标顺序，不进行隐式修复或三角化。
 */

#include "core/PlainMesh.h"

#include "core/Mesh.h"

namespace manumesh {

Mesh toMesh(const PlainMesh& plain) {
    Mesh mesh;
    mesh.vertices.reserve(plain.vertices.size());
    mesh.faces.reserve(plain.faces.size());
    mesh.faceTexCoords.reserve(plain.faceTexCoords.size());
    for (const PlainVec3& vertex : plain.vertices) {
        mesh.vertices.emplace_back(vertex.x, vertex.y, vertex.z);
    }
    for (const PlainFace& face : plain.faces) {
        mesh.faces.push_back(Face{face.v});
    }
    for (const PlainFaceTexCoords& texcoords : plain.faceTexCoords) {
        FaceTexCoords converted;
        converted.valid = texcoords.valid;
        for (int corner = 0; corner < 3; ++corner) {
            converted.uv[corner] = Vec2(texcoords.uv[corner].u, texcoords.uv[corner].v);
        }
        mesh.faceTexCoords.push_back(converted);
    }
    return mesh;
}

PlainMesh toPlainMesh(const Mesh& mesh) {
    PlainMesh plain;
    plain.vertices.reserve(mesh.vertices.size());
    plain.faces.reserve(mesh.faces.size());
    plain.faceTexCoords.reserve(mesh.faceTexCoords.size());
    for (const Vec3& vertex : mesh.vertices) {
        plain.vertices.push_back(PlainVec3{vertex.x(), vertex.y(), vertex.z()});
    }
    for (const Face& face : mesh.faces) {
        plain.faces.push_back(PlainFace{face.v});
    }
    for (const FaceTexCoords& texcoords : mesh.faceTexCoords) {
        PlainFaceTexCoords converted;
        converted.valid = texcoords.valid;
        for (int corner = 0; corner < 3; ++corner) {
            converted.uv[corner] = PlainVec2{texcoords.uv[corner].x(), texcoords.uv[corner].y()};
        }
        plain.faceTexCoords.push_back(converted);
    }
    return plain;
}

} // 命名空间 manumesh
