#pragma once

#include "core/Mesh.h"

#include <array>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

namespace manumesh::detail {

/// Adjacent face list for one undirected mesh edge.
struct MeshEdgeInfo {
    std::vector<int> faces;
};

using MeshEdgeInfoMap = std::unordered_map<std::uint64_t, MeshEdgeInfo>;

/// Packs an undirected vertex pair into a stable integer key.
std::uint64_t meshEdgeKey(int a, int b);

/// Unpacks a key created by meshEdgeKey.
std::pair<int, int> unpackMeshEdgeKey(std::uint64_t key);

/// Returns a sorted key for duplicate-face lookup.
std::array<int, 3> sortedFaceKey(std::array<int, 3> ids);

struct FaceKeyHash {
    std::size_t operator()(const std::array<int, 3>& ids) const;
};

/// Builds edge-to-face incidence once for algorithms that need local topology.
MeshEdgeInfoMap buildMeshEdgeInfo(const Mesh& mesh);

/// Computes one unit normal per face, returning zero for degenerate triangles.
std::vector<Vec3> computeFaceNormals(const Mesh& mesh);

/// Computes the centroid of one triangle face.
Vec3 faceCentroid(const Mesh& mesh, const Face& face);

/// Builds deduplicated one-ring vertex adjacency.
std::vector<std::vector<int>> buildVertexNeighbors(const Mesh& mesh);

/// Computes the average incident edge length per vertex.
///
/// Isolated vertices receive the global mean edge length when available, or 0
/// for an edgeless mesh. Algorithms use this as a local sampling-density scale.
std::vector<double> computeVertexAverageEdgeLength(const Mesh& mesh);

/// Marks vertices incident to boundary edges.
std::vector<char> computeBoundaryVertices(const Mesh& mesh);

} // namespace manumesh::detail
