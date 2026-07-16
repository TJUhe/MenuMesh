/**
 * @file src/common/detail/MeshQueries.h
 * @brief Declares mesh queries facilities for ManuMesh's common-geometry module.
 * @ingroup manumesh_common
 *
 * @details The routines here are policy-free geometry foundations shared by feature detection, simplification, analysis, and mesh editing.
 */

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

/**
 * @brief Adjacent face list for one undirected mesh edge.
 */
struct MeshEdgeInfo {
    std::vector<int> faces;
};

using MeshEdgeInfoMap = std::unordered_map<std::uint64_t, MeshEdgeInfo>;

/**
 * @brief Winding-aware dihedral classification for one manifold interior edge.
 */
struct OrientedDihedralAngle {
    double angleRad = 0.0;
    bool inconsistentWinding = false;
    /**
     * @brief +1 convex ridge, -1 concave valley, 0 flat/unknown.
     */
    int signedKind = 0;
};

/**
 * @brief Packs an undirected vertex pair into a stable integer key.
 *
 * Kept as an inline forwarder to the core topologyEdgeKey so every module
 * shares one packing scheme; existing common::meshEdgeKey callers keep
 * working unchanged.
 */
inline std::uint64_t meshEdgeKey(int a, int b) { return topologyEdgeKey(a, b); }

/**
 * @brief Unpacks a key created by meshEdgeKey.
 */
std::pair<int, int> unpackMeshEdgeKey(std::uint64_t key);

/**
 * @brief Returns a sorted key for duplicate-face lookup.
 */
std::array<int, 3> sortedFaceKey(std::array<int, 3> ids);

/**
 * @brief Stable hash for canonical sorted triangle vertex ids.
 */
struct FaceKeyHash {
    /** @brief Hashes all three canonical vertex ids. */
    std::size_t operator()(const std::array<int, 3>& ids) const;
};

/**
 * @brief Builds edge-to-face incidence once for algorithms that need local topology.
 */
MeshEdgeInfoMap buildMeshEdgeInfo(const Mesh& mesh);

/**
 * @brief Computes one unit normal per face, returning zero for degenerate triangles.
 */
std::vector<Vec3> computeFaceNormals(const Mesh& mesh);

/**
 * @brief Builds deterministic per-face flip marks that harmonize winding within
 * each orientable manifold component without modifying the input mesh.
 */
std::vector<char> harmonizeFaceWindings(const Mesh& mesh, const MeshEdgeInfoMap& edges);

/**
 * @brief Computes a winding-aware dihedral angle for a two-face edge. Unresolvable
 * orientation conflicts fall back to the unsigned normal angle and set the
 * diagnostic flag.
 */
OrientedDihedralAngle computeOrientedDihedralAngle(
    const Mesh& mesh,
    const std::vector<Vec3>& normals,
    const std::vector<char>& windingFlip,
    const MeshEdgeInfo& info,
    int a,
    int b
);

/**
 * @brief Computes the centroid of one triangle face.
 */
Vec3 faceCentroid(const Mesh& mesh, const Face& face);

/**
 * @brief Builds deduplicated one-ring vertex adjacency.
 *
 * Each per-vertex neighbor list is sorted ascending, which keeps iteration
 * order (and therefore floating-point reduction order) deterministic across
 * platforms and standard-library implementations.
 */
std::vector<std::vector<int>> buildVertexNeighbors(const Mesh& mesh);

/**
 * @brief Computes the average incident edge length per vertex.
 *
 * Isolated vertices receive the global mean edge length when available, or 0
 * for an edgeless mesh. Algorithms use this as a local sampling-density scale.
 */
std::vector<double> computeVertexAverageEdgeLength(const Mesh& mesh);

/**
 * @brief Marks vertices incident to boundary edges.
 */
std::vector<char> computeBoundaryVertices(const Mesh& mesh);

} // namespace manumesh::common

namespace manumesh {
// Transitional alias: manumesh::detail was renamed to manumesh::common
// (architecture v2, R6). New code must use manumesh::common; this alias is
// removed after one minor version.
namespace detail = common;
} // namespace manumesh
