#pragma once

#include "line_quadrics_qem/Export.h"
#include "line_quadrics_qem/core/Handles.h"
#include "line_quadrics_qem/core/Mesh.h"
#include "line_quadrics_qem/core/Status.h"

#include <array>
#include <cstdint>
#include <vector>

namespace lq {

/// Undirected edge with adjacent face and local-corner records.
struct TopologyEdge {
  std::array<int, 2> vertices{{-1, -1}};
  std::vector<int> faces;
  std::vector<int> faceCorners;

  bool boundary() const { return faces.size() == 1; }
  bool manifoldInterior() const { return faces.size() == 2; }
  bool nonManifold() const { return faces.size() > 2; }
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
  static LQ_API Result<MeshTopology> build(const Mesh& mesh, bool validate = true);

  int vertexCount() const { return vertexCount_; }
  int faceCount() const { return faceCount_; }
  int edgeCount() const { return static_cast<int>(edges_.size()); }
  int boundaryEdgeCount() const { return boundaryEdgeCount_; }
  int nonManifoldEdgeCount() const { return nonManifoldEdgeCount_; }

  const std::vector<TopologyEdge>& edges() const { return edges_; }
  const TopologyEdge& edge(EdgeId id) const { return edges_[id.id]; }
  const VertexTopology& vertex(VertexId id) const { return vertices_[id.id]; }

private:
  int vertexCount_ = 0;
  int faceCount_ = 0;
  int boundaryEdgeCount_ = 0;
  int nonManifoldEdgeCount_ = 0;
  std::vector<TopologyEdge> edges_;
  std::vector<VertexTopology> vertices_;
};

/// Returns a packed key for an undirected vertex pair.
LQ_API std::uint64_t topologyEdgeKey(int a, int b);

} // namespace lq
