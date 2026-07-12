#include "common/detail/MeshQueries.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace manumesh::common {

std::pair<int, int> unpackMeshEdgeKey(std::uint64_t key) {
    return {static_cast<int>(key >> 32u), static_cast<int>(key & 0xffffffffu)};
}

std::array<int, 3> sortedFaceKey(std::array<int, 3> ids) {
    std::sort(ids.begin(), ids.end());
    return ids;
}

std::size_t FaceKeyHash::operator()(const std::array<int, 3>& ids) const {
    return static_cast<std::size_t>(ids[0]) * 73856093u ^ static_cast<std::size_t>(ids[1]) * 19349663u ^
           static_cast<std::size_t>(ids[2]) * 83492791u;
}

MeshEdgeInfoMap buildMeshEdgeInfo(const Mesh& mesh) {
    MeshEdgeInfoMap edges;
    edges.reserve(mesh.faces.size() * 3);
    for (int fi = 0; fi < static_cast<int>(mesh.faces.size()); ++fi) {
        const Face& face = mesh.faces[fi];
        for (int e = 0; e < 3; ++e) {
            edges[meshEdgeKey(face.v[e], face.v[(e + 1) % 3])].faces.push_back(fi);
        }
    }
    return edges;
}

std::vector<Vec3> computeFaceNormals(const Mesh& mesh) {
    std::vector<Vec3> normals(mesh.faces.size(), Vec3::Zero());
    for (int fi = 0; fi < static_cast<int>(mesh.faces.size()); ++fi) {
        const Face& f = mesh.faces[fi];
        normals[fi] = triangleNormal(mesh.vertices[f.v[0]], mesh.vertices[f.v[1]], mesh.vertices[f.v[2]]);
    }
    return normals;
}

Vec3 faceCentroid(const Mesh& mesh, const Face& face) {
    return (mesh.vertices[face.v[0]] + mesh.vertices[face.v[1]] + mesh.vertices[face.v[2]]) / 3.0;
}

std::vector<std::vector<int>> buildVertexNeighbors(const Mesh& mesh) {
    std::vector<std::vector<int>> neighbors(mesh.vertices.size());
    for (const Face& face : mesh.faces) {
        for (int i = 0; i < 3; ++i) {
            const int a = face.v[i];
            const int b = face.v[(i + 1) % 3];
            if (a >= 0 && b >= 0 && a < static_cast<int>(neighbors.size()) && b < static_cast<int>(neighbors.size()) &&
                a != b) {
                neighbors[a].push_back(b);
                neighbors[b].push_back(a);
            }
        }
    }

    // Sort and deduplicate so each adjacency list is ascending; this keeps
    // downstream iteration and floating-point reduction order deterministic.
    for (std::vector<int>& list : neighbors) {
        std::sort(list.begin(), list.end());
        list.erase(std::unique(list.begin(), list.end()), list.end());
    }
    return neighbors;
}

std::vector<double> computeVertexAverageEdgeLength(const Mesh& mesh) {
    std::vector<double> sums(mesh.vertices.size(), 0.0);
    std::vector<int> counts(mesh.vertices.size(), 0);

    // Accumulate in ascending edge-key order so the floating-point reduction
    // order is stable across platforms and hash-map implementations.
    const MeshEdgeInfoMap edgeInfo = buildMeshEdgeInfo(mesh);
    std::vector<std::uint64_t> edgeKeys;
    edgeKeys.reserve(edgeInfo.size());
    for (const auto& [key, info] : edgeInfo) {
        (void)info;
        edgeKeys.push_back(key);
    }
    std::sort(edgeKeys.begin(), edgeKeys.end());

    double totalLength = 0.0;
    int totalEdges = 0;
    for (const std::uint64_t key : edgeKeys) {
        const auto [a, b] = unpackMeshEdgeKey(key);
        if (a < 0 || b < 0 || a >= static_cast<int>(mesh.vertices.size()) ||
            b >= static_cast<int>(mesh.vertices.size()) || a == b) {
            continue;
        }

        const double length = (mesh.vertices[b] - mesh.vertices[a]).norm();
        if (!std::isfinite(length) || length <= 1e-20) {
            continue;
        }
        sums[a] += length;
        sums[b] += length;
        ++counts[a];
        ++counts[b];
        totalLength += length;
        ++totalEdges;
    }

    const double fallback = totalEdges > 0 ? totalLength / static_cast<double>(totalEdges) : 0.0;
    std::vector<double> localScale(mesh.vertices.size(), fallback);
    for (int i = 0; i < static_cast<int>(localScale.size()); ++i) {
        if (counts[i] > 0) {
            localScale[i] = sums[i] / static_cast<double>(counts[i]);
        }
    }
    return localScale;
}

std::vector<char> computeBoundaryVertices(const Mesh& mesh) {
    std::vector<char> boundary(mesh.vertices.size(), 0);
    const MeshEdgeInfoMap edgeInfo = buildMeshEdgeInfo(mesh);
    for (const auto& [key, info] : edgeInfo) {
        if (info.faces.size() == 1) {
            const auto [a, b] = unpackMeshEdgeKey(key);
            if (a >= 0 && a < static_cast<int>(boundary.size())) {
                boundary[a] = 1;
            }
            if (b >= 0 && b < static_cast<int>(boundary.size())) {
                boundary[b] = 1;
            }
        }
    }
    return boundary;
}

} // namespace manumesh::common
