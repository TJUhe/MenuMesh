#include "common/detail/MeshQueries.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace manumesh::common {
namespace {

int faceEdgeDirection(const Face& face, int a, int b) {
    for (int i = 0; i < 3; ++i) {
        const int u = face.v[i];
        const int v = face.v[(i + 1) % 3];
        if (u == a && v == b) {
            return 1;
        }
        if (u == b && v == a) {
            return -1;
        }
    }
    return 0;
}

} // namespace

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

std::vector<char> harmonizeFaceWindings(const Mesh& mesh, const MeshEdgeInfoMap& edges) {
    const int faceCount = static_cast<int>(mesh.faces.size());
    std::vector<char> flip(static_cast<std::size_t>(faceCount), 0);
    std::vector<char> visited(static_cast<std::size_t>(faceCount), 0);
    std::vector<int> queue;
    for (int seed = 0; seed < faceCount; ++seed) {
        if (visited[seed]) {
            continue;
        }
        visited[seed] = 1;
        queue.clear();
        queue.push_back(seed);
        for (std::size_t head = 0; head < queue.size(); ++head) {
            const int f = queue[head];
            const Face& face = mesh.faces[f];
            const int signF = flip[f] ? -1 : 1;
            for (int e = 0; e < 3; ++e) {
                const int a = face.v[e];
                const int b = face.v[(e + 1) % 3];
                const auto it = edges.find(meshEdgeKey(a, b));
                if (it == edges.end() || it->second.faces.size() != 2) {
                    continue;
                }
                const int g = it->second.faces[0] == f ? it->second.faces[1] : it->second.faces[0];
                if (g == f || visited[g]) {
                    continue;
                }
                const int dirF = faceEdgeDirection(face, a, b);
                const int dirG = faceEdgeDirection(mesh.faces[g], a, b);
                if (dirF == 0 || dirG == 0) {
                    continue;
                }
                visited[g] = 1;
                flip[g] = (-dirF * dirG * signF) < 0 ? 1 : 0;
                queue.push_back(g);
            }
        }

        int flippedCount = 0;
        for (int f : queue) {
            flippedCount += flip[f] ? 1 : 0;
        }
        if (2 * flippedCount > static_cast<int>(queue.size())) {
            for (int f : queue) {
                flip[f] = flip[f] ? 0 : 1;
            }
        }
    }
    return flip;
}

OrientedDihedralAngle computeOrientedDihedralAngle(
    const Mesh& mesh,
    const std::vector<Vec3>& normals,
    const std::vector<char>& windingFlip,
    const MeshEdgeInfo& info,
    int a,
    int b
) {
    OrientedDihedralAngle result;
    if (info.faces.size() != 2) {
        return result;
    }

    const int f0 = info.faces[0];
    const int f1 = info.faces[1];
    const int s0 = windingFlip[f0] ? -1 : 1;
    const int s1 = windingFlip[f1] ? -1 : 1;
    double dot = std::clamp(normals[f0].dot(normals[f1]), -1.0, 1.0) * static_cast<double>(s0 * s1);
    const int direction0 = faceEdgeDirection(mesh.faces[f0], a, b) * s0;
    const int direction1 = faceEdgeDirection(mesh.faces[f1], a, b) * s1;
    if (direction0 == 0 || direction1 == 0 || direction0 == direction1) {
        result.inconsistentWinding = true;
        dot = std::abs(dot);
    }
    result.angleRad = std::acos(dot);
    if (!result.inconsistentWinding) {
        Vec3 edge = mesh.vertices[b] - mesh.vertices[a];
        const double edgeLength = edge.norm();
        if (edgeLength > 1e-20) {
            const Vec3 d = edge * (static_cast<double>(direction0) / edgeLength);
            const double side =
                (normals[f0] * static_cast<double>(s0)).cross(normals[f1] * static_cast<double>(s1)).dot(d);
            if (side > 1e-12) {
                result.signedKind = 1;
            } else if (side < -1e-12) {
                result.signedKind = -1;
            }
        }
    }
    return result;
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
