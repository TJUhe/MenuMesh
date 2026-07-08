#include "core/MeshTopology.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <unordered_map>

namespace manumesh {
namespace {

struct EdgeBuildRecord {
  int edgeId = -1;
};

} // namespace

struct MeshTopology::Impl {
  int vertexCount = 0;
  int faceCount = 0;
  int boundaryEdgeCount = 0;
  int nonManifoldEdgeCount = 0;
  std::vector<TopologyEdge> edges;
  std::vector<VertexTopology> vertices;
};

bool TopologyEdge::boundary() const {
  return faces.size() == 1;
}

bool TopologyEdge::manifoldInterior() const {
  return faces.size() == 2;
}

bool TopologyEdge::nonManifold() const {
  return faces.size() > 2;
}

MeshTopology::MeshTopology() : impl_(std::make_unique<Impl>()) {
}

MeshTopology::~MeshTopology() = default;

MeshTopology::MeshTopology(const MeshTopology& other)
    : impl_(other.impl_ ? std::make_unique<Impl>(*other.impl_)
                        : std::make_unique<Impl>()) {
}

MeshTopology& MeshTopology::operator=(const MeshTopology& other) {
  if (this != &other) {
    impl_ =
        other.impl_ ? std::make_unique<Impl>(*other.impl_) : std::make_unique<Impl>();
  }
  return *this;
}

MeshTopology::MeshTopology(MeshTopology&& other) noexcept
    : impl_(std::move(other.impl_)) {
  if (!impl_) {
    impl_ = std::make_unique<Impl>();
  }
  other.impl_ = std::make_unique<Impl>();
}

MeshTopology& MeshTopology::operator=(MeshTopology&& other) noexcept {
  if (this != &other) {
    impl_ = std::move(other.impl_);
    if (!impl_) {
      impl_ = std::make_unique<Impl>();
    }
    other.impl_ = std::make_unique<Impl>();
  }
  return *this;
}

std::uint64_t topologyEdgeKey(int a, int b) {
  if (a > b) std::swap(a, b);
  return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(a)) << 32u) |
         static_cast<std::uint32_t>(b);
}

Result<MeshTopology> MeshTopology::build(const Mesh& mesh, bool validate) {
  MeshTopology topology;
  topology.impl_->vertexCount = static_cast<int>(mesh.vertices.size());
  topology.impl_->faceCount = static_cast<int>(mesh.faces.size());
  topology.impl_->vertices.resize(mesh.vertices.size());
  topology.impl_->edges.reserve(mesh.faces.size() * 3 / 2);

  std::unordered_map<std::uint64_t, EdgeBuildRecord> edgeByKey;
  edgeByKey.reserve(mesh.faces.size() * 3);

  for (int fi = 0; fi < static_cast<int>(mesh.faces.size()); ++fi) {
    const Face& face = mesh.faces[fi];
    for (int corner = 0; corner < 3; ++corner) {
      const int id = face.v[corner];
      if (validate && (id < 0 || id >= static_cast<int>(mesh.vertices.size()))) {
        return Status::invalidArgument("Mesh face references an invalid vertex index.");
      }
      if (id >= 0 && id < static_cast<int>(topology.impl_->vertices.size())) {
        topology.impl_->vertices[id].faces.push_back(fi);
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
          key, EdgeBuildRecord{static_cast<int>(topology.impl_->edges.size())});
      if (inserted) {
        TopologyEdge edge;
        edge.vertices = {a, b};
        topology.impl_->edges.push_back(std::move(edge));
      }

      TopologyEdge& edge = topology.impl_->edges[it->second.edgeId];
      edge.faces.push_back(fi);
      edge.faceCorners.push_back(corner);
    }
  }

  for (int ei = 0; ei < static_cast<int>(topology.impl_->edges.size()); ++ei) {
    const TopologyEdge& edge = topology.impl_->edges[ei];
    if (edge.boundary()) {
      ++topology.impl_->boundaryEdgeCount;
    } else if (edge.nonManifold()) {
      ++topology.impl_->nonManifoldEdgeCount;
    }
    for (int vertex : edge.vertices) {
      if (vertex >= 0 && vertex < static_cast<int>(topology.impl_->vertices.size())) {
        topology.impl_->vertices[vertex].edges.push_back(ei);
      }
    }
  }

  return topology;
}

int MeshTopology::vertexCount() const {
  return impl_->vertexCount;
}

int MeshTopology::faceCount() const {
  return impl_->faceCount;
}

int MeshTopology::edgeCount() const {
  return static_cast<int>(impl_->edges.size());
}

int MeshTopology::boundaryEdgeCount() const {
  return impl_->boundaryEdgeCount;
}

int MeshTopology::nonManifoldEdgeCount() const {
  return impl_->nonManifoldEdgeCount;
}

const std::vector<TopologyEdge>& MeshTopology::edges() const {
  return impl_->edges;
}

const TopologyEdge& MeshTopology::edge(EdgeId id) const {
  return impl_->edges[id.id];
}

const VertexTopology& MeshTopology::vertex(VertexId id) const {
  return impl_->vertices[id.id];
}

} // namespace manumesh
