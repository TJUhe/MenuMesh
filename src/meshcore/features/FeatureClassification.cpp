/**
 * @file src/meshcore/features/FeatureClassification.cpp
 * @brief 实现 MeshCore 的边界和锐边分类。
 * @ingroup manumesh_feature_detection
 */

#include "meshcore/features/FeatureClassification.h"

#include <cmath>
#include <map>
#include <stdexcept>
#include <vector>

namespace manumesh {
namespace meshcore {
namespace {

struct EdgeUse {
    std::vector<int> faces;
};

MeshEdge sortedEdge(int first, int second) {
    return first < second ? MeshEdge{first, second} : MeshEdge{second, first};
}

void markVertices(std::vector<char>& vertices, const MeshEdge& edge) {
    vertices[static_cast<std::size_t>(edge.first)] = 1;
    vertices[static_cast<std::size_t>(edge.second)] = 1;
}

} // namespace

FeatureReport detectFeatures(const Mesh& mesh, const FeatureOptions& options) {
    if (!std::isfinite(options.dihedralAngleDegrees) || options.dihedralAngleDegrees <= 0.0 ||
        options.dihedralAngleDegrees >= 180.0) {
        throw std::invalid_argument("feature angle must be in (0, 180) degrees");
    }

    std::string error;
    if (!validateMeshGeometryLenient(mesh, &error)) {
        throw std::invalid_argument("cannot detect features: " + error);
    }

    std::map<MeshEdge, EdgeUse> edges;
    for (std::size_t faceIndex = 0; faceIndex < mesh.faces.size(); ++faceIndex) {
        const Face& face = mesh.faces[faceIndex];
        for (int corner = 0; corner < 3; ++corner) {
            const int first = face.v[static_cast<std::size_t>(corner)];
            const int second = face.v[static_cast<std::size_t>((corner + 1) % 3)];
            edges[sortedEdge(first, second)].faces.push_back(static_cast<int>(faceIndex));
        }
    }

    const std::vector<Vec3> normals = computeFaceNormals(mesh);
    const double threshold = std::cos(options.dihedralAngleDegrees * 3.14159265358979323846 / 180.0);
    FeatureReport result;
    result.boundaryVertices.assign(mesh.vertices.size(), 0);
    result.featureVertices.assign(mesh.vertices.size(), 0);

    for (const auto& entry : edges) {
        const MeshEdge& edge = entry.first;
        const std::vector<int>& faces = entry.second.faces;
        if (faces.size() == 1U) {
            ++result.boundaryEdges;
            result.boundary.push_back(edge);
            markVertices(result.boundaryVertices, edge);
            continue;
        }
        if (faces.size() != 2U) {
            ++result.nonManifoldEdges;
            result.nonManifold.push_back(edge);
            markVertices(result.featureVertices, edge);
            continue;
        }

        const Vec3& firstNormal = normals[static_cast<std::size_t>(faces[0])];
        const Vec3& secondNormal = normals[static_cast<std::size_t>(faces[1])];
        const double dot = firstNormal.dot(secondNormal);
        if (firstNormal.squaredNorm() == 0.0 || secondNormal.squaredNorm() == 0.0 || dot <= threshold) {
            ++result.sharpEdges;
            result.sharp.push_back(edge);
            markVertices(result.featureVertices, edge);
        }
    }
    return result;
}

} // namespace meshcore
} // namespace manumesh
