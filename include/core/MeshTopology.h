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
  std::array<int, 2> vertices{{-1, -1}};
  std::vector<int> faces;
  std::vector<int> faceCorners;

  MANUMESH_API bool boundary() const;
  MANUMESH_API bool manifoldInterior() const;
  MANUMESH_API bool nonManifold() const;
};

/// Per-vertex incident edge/face lists built once and reused by algorithms.
struct VertexTopology {
  std::vector<int> edges;
  std::vector<int> faces;
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

  static MANUMESH_API Result<MeshTopology> build(const Mesh& mesh,
                                                 bool validate = true);

  MANUMESH_API int vertexCount() const;
  MANUMESH_API int faceCount() const;
  MANUMESH_API int edgeCount() const;
  MANUMESH_API int boundaryEdgeCount() const;
  MANUMESH_API int nonManifoldEdgeCount() const;

  MANUMESH_API const std::vector<TopologyEdge>& edges() const;
  MANUMESH_API const TopologyEdge& edge(EdgeId id) const;
  MANUMESH_API const VertexTopology& vertex(VertexId id) const;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

/// Returns a packed key for an undirected vertex pair.
MANUMESH_API std::uint64_t topologyEdgeKey(int a, int b);

} // namespace manumesh
