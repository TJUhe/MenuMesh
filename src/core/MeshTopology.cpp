/**
 * @file src/core/MeshTopology.cpp
 * @brief 从三角面构建不可变的边和顶点入射缓存。
 * @ingroup manumesh_core
 *
 * @details 构建不可变的无向边和逐顶点入射缓存。
 * @algorithm 每个三角形贡献三个规范边键；随后对稠密边记录和顶点入射关系排序，
 * 使遍历具有确定性。
 * @invariants 面角数组始终与入射面数组保持对齐。
 */

#include "core/MeshTopology.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace manumesh {
namespace {

/** @brief 为一条边累积的临时入射和绕序数据。 */
struct EdgeBuildRecord {
    int edgeId = -1;
};

} // 命名空间

/** @brief MeshTopology 的私有邻接和边入射存储。 */
struct MeshTopology::Impl {
    int vertexCount = 0;
    int faceCount = 0;
    int boundaryEdgeCount = 0;
    int nonManifoldEdgeCount = 0;
    std::vector<TopologyEdge> edges;
    std::vector<VertexTopology> vertices;
};

bool TopologyEdge::boundary() const { return faces.size() == 1; }

bool TopologyEdge::manifoldInterior() const { return faces.size() == 2; }

bool TopologyEdge::nonManifold() const { return faces.size() > 2; }

MeshTopology::MeshTopology()
    : impl_(std::make_unique<Impl>()) {}

MeshTopology::~MeshTopology() = default;

MeshTopology::MeshTopology(const MeshTopology& other)
    : impl_(other.impl_ ? std::make_unique<Impl>(*other.impl_) : std::make_unique<Impl>()) {}

MeshTopology& MeshTopology::operator=(const MeshTopology& other) {
    if (this != &other) {
        impl_ = other.impl_ ? std::make_unique<Impl>(*other.impl_) : std::make_unique<Impl>();
    }
    return *this;
}

MeshTopology::MeshTopology(MeshTopology&& other) noexcept
    : impl_(std::move(other.impl_)) {}

MeshTopology& MeshTopology::operator=(MeshTopology&& other) noexcept {
    if (this != &other) {
        impl_ = std::move(other.impl_);
    }
    return *this;
}

std::uint64_t topologyEdgeKey(int a, int b) {
    if (a > b)
        std::swap(a, b);
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(a)) << 32u) | static_cast<std::uint32_t>(b);
}

Result<MeshTopology> MeshTopology::build(const Mesh& mesh, bool validate) {
    if (validate) {
        std::string error;
        if (!validateMeshIndices(mesh, &error)) {
            return Status::invalidArgument(error.empty() ? "Mesh contains an invalid vertex index." : error);
        }
        for (const Face& face : mesh.faces) {
            if (face.v[0] == face.v[1] || face.v[1] == face.v[2] || face.v[0] == face.v[2]) {
                return Status::topologyError("Mesh contains a degenerate face.");
            }
        }
        if (!validateMeshGeometryLenient(mesh, &error)) {
            return Status::invalidArgument(error.empty() ? "Mesh geometry is invalid." : error);
        }
    }

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

        for (int corner = 0; corner < 3; ++corner) {
            const int a = face.v[corner];
            const int b = face.v[(corner + 1) % 3];
            const std::uint64_t key = topologyEdgeKey(a, b);
            const auto insertResult =
                edgeByKey.emplace(key, EdgeBuildRecord{static_cast<int>(topology.impl_->edges.size())});
            auto it = insertResult.first;
            const bool inserted = insertResult.second;
            if (inserted) {
                TopologyEdge edge;
                edge.vertices = {std::min(a, b), std::max(a, b)};
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

int MeshTopology::vertexCount() const { return impl_ ? impl_->vertexCount : 0; }

int MeshTopology::faceCount() const { return impl_ ? impl_->faceCount : 0; }

int MeshTopology::edgeCount() const { return impl_ ? static_cast<int>(impl_->edges.size()) : 0; }

int MeshTopology::boundaryEdgeCount() const { return impl_ ? impl_->boundaryEdgeCount : 0; }

int MeshTopology::nonManifoldEdgeCount() const { return impl_ ? impl_->nonManifoldEdgeCount : 0; }

const std::vector<TopologyEdge>& MeshTopology::edges() const {
    static const std::vector<TopologyEdge> empty;
    return impl_ ? impl_->edges : empty;
}

bool MeshTopology::hasEdge(EdgeId id) const {
    return impl_ && id.id >= 0 && id.id < static_cast<int>(impl_->edges.size());
}

bool MeshTopology::hasVertex(VertexId id) const {
    return impl_ && id.id >= 0 && id.id < static_cast<int>(impl_->vertices.size());
}

const TopologyEdge& MeshTopology::edge(EdgeId id) const {
    if (!hasEdge(id)) {
        throw std::out_of_range("MeshTopology edge id is out of range.");
    }
    return impl_->edges[id.id];
}

const VertexTopology& MeshTopology::vertex(VertexId id) const {
    if (!hasVertex(id)) {
        throw std::out_of_range("MeshTopology vertex id is out of range.");
    }
    return impl_->vertices[id.id];
}

} // 命名空间 manumesh
