/**
 * @file include/core/MeshTopology.h
 * @brief Declares mesh topology facilities for ManuMesh's core-mesh module.
 * @ingroup manumesh_core
 *
 * @details Core types establish the storage, validation, tolerance, topology, and status contracts consumed by every algorithm module.
 */

#pragma once

#include "Export.h"
#include "core/Handles.h"
#include "core/Mesh.h"
#include "core/Status.h"

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace manumesh {

/// Undirected edge with adjacent face and local-corner records.
struct TopologyEdge {
    std::array<int, 2> vertices{{-1, -1}}; ///< Ascending endpoint indices.
    std::vector<int> faces;                ///< Incident face indices.
    std::vector<int> faceCorners;          ///< Local corner opposite this edge.

    /// @return true when exactly one face is incident.
    MANUMESH_API bool boundary() const;
    /// @return true when exactly two faces are incident.
    MANUMESH_API bool manifoldInterior() const;
    /// @return true when more than two faces are incident.
    MANUMESH_API bool nonManifold() const;
};

/// Per-vertex incident edge/face lists built once and reused by algorithms.
struct VertexTopology {
    std::vector<int> edges; ///< Incident edge indices in deterministic order.
    std::vector<int> faces; ///< Incident face indices in deterministic order.
};

/// Immutable topology cache for triangle meshes.
///
/// The cache owns dense arrays, so algorithms can iterate by integer handle
/// without repeated hash-map reconstruction. Build it from Mesh when topology
/// may be reused for validation, repair, features, simplification, or Boolean
/// preflight.
class MeshTopology {
public:
    MANUMESH_API MeshTopology();
    MANUMESH_API ~MeshTopology();

    MANUMESH_API MeshTopology(const MeshTopology& other);
    MANUMESH_API MeshTopology& operator=(const MeshTopology& other);
    MANUMESH_API MeshTopology(MeshTopology&& other) noexcept;
    MANUMESH_API MeshTopology& operator=(MeshTopology&& other) noexcept;

    /**
     * @brief Builds immutable edge and vertex incidence for a triangle mesh.
     * @param[in] mesh Source mesh; it is not retained by the topology object.
     * @param[in] validate Run lenient geometry validation before reading faces.
     * @return Topology on success, otherwise an invalid-argument/topology status.
     * @complexity Expected O(V + F).
     */
    static MANUMESH_API Result<MeshTopology> build(const Mesh& mesh, bool validate = true);

    MANUMESH_API int vertexCount() const;
    MANUMESH_API int faceCount() const;
    MANUMESH_API int edgeCount() const;
    MANUMESH_API int boundaryEdgeCount() const;
    MANUMESH_API int nonManifoldEdgeCount() const;

    /// @return Dense immutable edge storage owned by this object.
    MANUMESH_API const std::vector<TopologyEdge>& edges() const;
    /// Returns true when id names an edge in this topology.
    MANUMESH_API bool hasEdge(EdgeId id) const;
    /// Returns true when id names a vertex in this topology.
    MANUMESH_API bool hasVertex(VertexId id) const;
    /// Returns the requested edge or throws std::out_of_range for an invalid id.
    MANUMESH_API const TopologyEdge& edge(EdgeId id) const;
    /// Returns the requested vertex topology or throws std::out_of_range.
    MANUMESH_API const VertexTopology& vertex(VertexId id) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/// Packs an undirected vertex pair into a stable 64-bit key.
/// @param[in] a First zero-based vertex index.
/// @param[in] b Second zero-based vertex index.
/// @return Key with the smaller index in the high 32 bits.
MANUMESH_API std::uint64_t topologyEdgeKey(int a, int b);

} // namespace manumesh
