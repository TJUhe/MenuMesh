#pragma once

#include "line_quadrics_qem/Export.h"
#include "line_quadrics_qem/core/Handles.h"
#include "line_quadrics_qem/core/Mesh.h"
#include "line_quadrics_qem/core/Status.h"

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace lq {

/// Undirected edge with adjacent face and local-corner records.
struct TopologyEdge {
  std::array<int, 2> vertices{{-1, -1}};
  std::vector<int> faces;
  std::vector<int> faceCorners;

  LQ_API bool boundary() const;
  LQ_API bool manifoldInterior() const;
  LQ_API bool nonManifold() const;
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
  LQ_API MeshTopology();
  LQ_API ~MeshTopology();

  LQ_API MeshTopology(const MeshTopology& other);
  LQ_API MeshTopology& operator=(const MeshTopology& other);
  LQ_API MeshTopology(MeshTopology&& other) noexcept;
  LQ_API MeshTopology& operator=(MeshTopology&& other) noexcept;

  static LQ_API Result<MeshTopology> build(const Mesh& mesh, bool validate = true);

  LQ_API int vertexCount() const;
  LQ_API int faceCount() const;
  LQ_API int edgeCount() const;
  LQ_API int boundaryEdgeCount() const;
  LQ_API int nonManifoldEdgeCount() const;

  LQ_API const std::vector<TopologyEdge>& edges() const;
  LQ_API const TopologyEdge& edge(EdgeId id) const;
  LQ_API const VertexTopology& vertex(VertexId id) const;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

/// Returns a packed key for an undirected vertex pair.
LQ_API std::uint64_t topologyEdgeKey(int a, int b);

} // namespace lq
