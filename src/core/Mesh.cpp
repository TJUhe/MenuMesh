/**
 * @file src/core/Mesh.cpp
 * @brief 实现 ManuMesh 核心网格模块的网格设施。
 * @ingroup manumesh_core
 *
 * @details 实现网格边界、校验、压缩和基本三角形几何运算。
 * @algorithm 严格和宽松校验共用索引、有限性、重复索引和 UV 对齐检查；
 * 只有宽松路径允许零面积三角形。压缩会丢弃无效面，并保留顶点首次使用顺序。
 */

#include "core/Mesh.h"

#include "core/MeshTopology.h"
#include "core/Tolerances.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <unordered_set>

namespace manumesh {
namespace {

bool finitePoint(const Vec3& p) { return std::isfinite(p.x()) && std::isfinite(p.y()) && std::isfinite(p.z()); }

bool finiteTexCoord(const Vec2& uv) { return std::isfinite(uv.x()) && std::isfinite(uv.y()); }

} // 命名空间

bool Mesh::empty() const { return vertices.empty() || faces.empty(); }

Vec3 Mesh::bboxMin() const {
    if (vertices.empty()) {
        return Vec3::Zero();
    }
    Vec3 lo(
        std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity()
    );
    for (const Vec3& p : vertices) {
        lo = lo.cwiseMin(p);
    }
    return lo;
}

Vec3 Mesh::bboxMax() const {
    if (vertices.empty()) {
        return Vec3::Zero();
    }
    Vec3 hi(
        -std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity()
    );
    for (const Vec3& p : vertices) {
        hi = hi.cwiseMax(p);
    }
    return hi;
}

double Mesh::bboxDiag() const {
    if (vertices.empty()) {
        return 0.0;
    }
    return (bboxMax() - bboxMin()).norm();
}

bool Mesh::hasTextureCoordinates() const {
    return std::any_of(faceTexCoords.begin(), faceTexCoords.end(), [](const FaceTexCoords& texcoords) {
        return texcoords.valid;
    });
}

void Mesh::removeUnusedVertices() {
    std::vector<int> remap(vertices.size(), -1);
    std::vector<Vec3> newVertices;
    newVertices.reserve(vertices.size());
    std::vector<char> validFace(faces.size(), 1);

    // 第一遍：按整个面进行分类，确保仅被丢弃面引用的顶点不会进入重映射表。
    for (std::size_t faceIndex = 0; faceIndex < faces.size(); ++faceIndex) {
        const Face& face = faces[faceIndex];
        for (int id : face.v) {
            if (id < 0 || id >= static_cast<int>(vertices.size())) {
                validFace[faceIndex] = 0;
                break;
            }
        }
    }

    // 第二遍：仅根据有效面构建顶点重映射表。
    for (std::size_t faceIndex = 0; faceIndex < faces.size(); ++faceIndex) {
        if (!validFace[faceIndex]) {
            continue;
        }
        for (int id : faces[faceIndex].v) {
            if (remap[id] < 0) {
                remap[id] = static_cast<int>(newVertices.size());
                newVertices.push_back(vertices[id]);
            }
        }
    }

    const bool alignedTexcoords = faceTexCoords.size() == faces.size();
    std::vector<Face> newFaces;
    std::vector<FaceTexCoords> newTexcoords;
    newFaces.reserve(faces.size());
    if (alignedTexcoords) {
        newTexcoords.reserve(faceTexCoords.size());
    }
    for (std::size_t faceIndex = 0; faceIndex < faces.size(); ++faceIndex) {
        if (!validFace[faceIndex]) {
            continue;
        }
        Face face = faces[faceIndex];
        for (int& id : face.v) {
            id = remap[id];
        }
        newFaces.push_back(face);
        if (alignedTexcoords) {
            newTexcoords.push_back(faceTexCoords[faceIndex]);
        }
    }
    faces.swap(newFaces);
    if (alignedTexcoords) {
        faceTexCoords.swap(newTexcoords);
    } else {
        faceTexCoords.clear();
    }
    vertices.swap(newVertices);
}

bool validateMeshIndices(const Mesh& mesh, std::string* error) {
    if (error) {
        error->clear();
    }
    if (mesh.vertices.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        if (error)
            *error = "Vertex count exceeds the supported int-index range.";
        return false;
    }
    for (std::size_t faceIndex = 0; faceIndex < mesh.faces.size(); ++faceIndex) {
        const Face& face = mesh.faces[faceIndex];
        for (int id : face.v) {
            if (id < 0 || id >= static_cast<int>(mesh.vertices.size())) {
                if (error) {
                    *error = "Mesh face " + std::to_string(faceIndex) + " references an invalid vertex index.";
                }
                return false;
            }
        }
    }
    return true;
}

namespace {
bool validateMeshGeometryImpl(const Mesh& mesh, std::string* error, bool rejectZeroAreaFaces) {
    if (!validateMeshIndices(mesh, error)) {
        return false;
    }
    for (std::size_t vertexIndex = 0; vertexIndex < mesh.vertices.size(); ++vertexIndex) {
        if (!finitePoint(mesh.vertices[vertexIndex])) {
            if (error) {
                *error = "Mesh vertex " + std::to_string(vertexIndex) + " contains a non-finite coordinate.";
            }
            return false;
        }
    }
    if (!mesh.faceTexCoords.empty() && mesh.faceTexCoords.size() != mesh.faces.size()) {
        if (error) {
            *error = "Per-corner texture coordinates must be empty or aligned with mesh faces.";
        }
        return false;
    }
    for (std::size_t faceIndex = 0; faceIndex < mesh.faceTexCoords.size(); ++faceIndex) {
        const FaceTexCoords& texcoords = mesh.faceTexCoords[faceIndex];
        if (!texcoords.valid) {
            continue;
        }
        for (const Vec2& uv : texcoords.uv) {
            if (!finiteTexCoord(uv)) {
                if (error) {
                    *error = "Mesh face " + std::to_string(faceIndex) + " contains a non-finite texture coordinate.";
                }
                return false;
            }
        }
    }
    for (std::size_t faceIndex = 0; faceIndex < mesh.faces.size(); ++faceIndex) {
        const Face& face = mesh.faces[faceIndex];
        if (face.v[0] == face.v[1] || face.v[1] == face.v[2] || face.v[0] == face.v[2]) {
            if (error) {
                *error = "Mesh face " + std::to_string(faceIndex) + " is degenerate.";
            }
            return false;
        }
        if (!rejectZeroAreaFaces) {
            continue;
        }
        const Vec3& a = mesh.vertices[face.v[0]];
        const Vec3& b = mesh.vertices[face.v[1]];
        const Vec3& c = mesh.vertices[face.v[2]];
        if ((b - a).cross(c - a).squaredNorm() <= kMinSquaredNormalLength) {
            if (error) {
                *error = "Mesh face " + std::to_string(faceIndex) + " has zero area.";
            }
            return false;
        }
    }
    return true;
}

} // 命名空间

bool validateMeshGeometry(const Mesh& mesh, std::string* error) {
    return validateMeshGeometryImpl(mesh, error, /*rejectZeroAreaFaces=*/true);
}

bool validateMeshGeometryLenient(const Mesh& mesh, std::string* error) {
    return validateMeshGeometryImpl(mesh, error, /*rejectZeroAreaFaces=*/false);
}

int countDegenerateFaces(const Mesh& mesh) {
    int count = 0;
    for (const Face& face : mesh.faces) {
        if (face.v[0] == face.v[1] || face.v[1] == face.v[2] || face.v[0] == face.v[2]) {
            ++count;
            continue;
        }
        const Vec3& a = mesh.vertices[face.v[0]];
        const Vec3& b = mesh.vertices[face.v[1]];
        const Vec3& c = mesh.vertices[face.v[2]];
        if ((b - a).cross(c - a).squaredNorm() <= kMinSquaredNormalLength) {
            ++count;
        }
    }
    return count;
}

double triangleArea(const Vec3& a, const Vec3& b, const Vec3& c) { return 0.5 * (b - a).cross(c - a).norm(); }

Vec3 triangleNormal(const Vec3& a, const Vec3& b, const Vec3& c) {
    Vec3 n = (b - a).cross(c - a);
    const double len = n.norm();
    if (len <= kMinNormalLength) {
        return Vec3(0.0, 0.0, 0.0);
    }
    return n / len;
}

std::vector<std::pair<int, int>> uniqueEdges(const Mesh& mesh) {
    std::unordered_set<std::uint64_t> seen;
    std::vector<std::pair<int, int>> edges;
    edges.reserve(mesh.faces.size() * 3 / 2);

    for (const Face& face : mesh.faces) {
        for (int e = 0; e < 3; ++e) {
            int a = face.v[e];
            int b = face.v[(e + 1) % 3];
            if (a == b)
                continue;
            const auto key = topologyEdgeKey(a, b);
            if (seen.insert(key).second) {
                if (a > b)
                    std::swap(a, b);
                edges.emplace_back(a, b);
            }
        }
    }
    std::sort(edges.begin(), edges.end());
    return edges;
}

} // 命名空间 manumesh
