#pragma once

#include "core/Mesh.h"
#include "core/MeshTopology.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

namespace manumesh::common {

/// Adjacent face list for one undirected mesh edge.
struct MeshEdgeInfo {
    std::vector<int> faces;
};

using MeshEdgeInfoMap = std::unordered_map<std::uint64_t, MeshEdgeInfo>;

/// Packs an undirected vertex pair into a stable integer key.
///
/// Kept as an inline forwarder to the core topologyEdgeKey so every module
/// shares one packing scheme; existing common::meshEdgeKey callers keep
/// working unchanged.
inline std::uint64_t meshEdgeKey(int a, int b) { return topologyEdgeKey(a, b); }

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
///
/// Each per-vertex neighbor list is sorted ascending, which keeps iteration
/// order (and therefore floating-point reduction order) deterministic across
/// platforms and standard-library implementations.
std::vector<std::vector<int>> buildVertexNeighbors(const Mesh& mesh);

/// Computes the average incident edge length per vertex.
///
/// Isolated vertices receive the global mean edge length when available, or 0
/// for an edgeless mesh. Algorithms use this as a local sampling-density scale.
std::vector<double> computeVertexAverageEdgeLength(const Mesh& mesh);

/// Marks vertices incident to boundary edges.
std::vector<char> computeBoundaryVertices(const Mesh& mesh);

} // namespace manumesh::common

namespace manumesh {
// Transitional alias: manumesh::detail was renamed to manumesh::common
// (architecture v2, R6). New code must use manumesh::common; this alias is
// removed after one minor version.
namespace detail = common;
} // namespace manumesh
