#include "line_quadrics_qem/core/MeshTopology.h"

#include <algorithm>
#include <cstdint>
#include <unordered_map>

namespace lq {
namespace {

struct EdgeBuildRecord {
  int edgeId = -1;
};

} // namespace

std::uint64_t topologyEdgeKey(int a, int b) {
  if (a > b) std::swap(a, b);
  return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(a)) << 32u) |
         static_cast<std::uint32_t>(b);
}

Result<MeshTopology> MeshTopology::build(const Mesh& mesh, bool validate) {
  MeshTopology topology;
  topology.vertexCount_ = static_cast<int>(mesh.vertices.size());
  topology.faceCount_ = static_cast<int>(mesh.faces.size());
  topology.vertices_.resize(mesh.vertices.size());
  topology.edges_.reserve(mesh.faces.size() * 3 / 2);

  std::unordered_map<std::uint64_t, EdgeBuildRecord> edgeByKey;
  edgeByKey.reserve(mesh.faces.size() * 3);

  for (int fi = 0; fi < static_cast<int>(mesh.faces.size()); ++fi) {
    const Face& face = mesh.faces[fi];
    for (int corner = 0; corner < 3; ++corner) {
      const int id = face.v[corner];
      if (validate && (id < 0 || id >= static_cast<int>(mesh.vertices.size()))) {
        return Status::invalidArgument("Mesh face references an invalid vertex index.");
      }
      if (id >= 0 && id < static_cast<int>(topology.vertices_.size())) {
        topology.vertices_[id].faces.push_back(fi);
      }
    }

    if (validate &&
        (face.v[0] == face.v[1] || face.v[1] == face.v[2] || face.v[0] == face.v[2])) {
      return Status::topologyError("Mesh contains a degenerate face.");
    }

    for (int corner = 0; corner < 3; ++corner) {
      int a = face.v[corner];
      int b = face.v[(corner + 1) % 3];
      if (a > b) std::swap(a, b);
      const std::uint64_t key = topologyEdgeKey(a, b);
      auto [it, inserted] = edgeByKey.emplace(
          key, EdgeBuildRecord{static_cast<int>(topology.edges_.size())});
      if (inserted) {
        TopologyEdge edge;
        edge.vertices = {a, b};
        topology.edges_.push_back(std::move(edge));
      }

      TopologyEdge& edge = topology.edges_[it->second.edgeId];
      edge.faces.push_back(fi);
      edge.faceCorners.push_back(corner);
    }
  }

  for (int ei = 0; ei < static_cast<int>(topology.edges_.size()); ++ei) {
    const TopologyEdge& edge = topology.edges_[ei];
    if (edge.boundary()) {
      ++topology.boundaryEdgeCount_;
    } else if (edge.nonManifold()) {
      ++topology.nonManifoldEdgeCount_;
    }
    for (int vertex : edge.vertices) {
      if (vertex >= 0 && vertex < static_cast<int>(topology.vertices_.size())) {
        topology.vertices_[vertex].edges.push_back(ei);
      }
    }
  }

  return topology;
}

} // namespace lq
