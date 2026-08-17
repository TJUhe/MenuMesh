/**
 * @file src/core/MeshTopology.cpp
 * @brief 从三角面构建不可变的边和顶点入射缓存。
 * @ingroup manumesh_core
 *
 * @details 构建不可变的无向边和逐顶点入射缓存。
 * @algorithm 每个三角形贡献三个规范边键；边按输入面/角顺序建立，顶点入射边按
 * 稠密边 ID 追加，面按输入面顺序追加，因此遍历具有确定性。
 * @invariants 面角数组始终与入射面数组保持对齐。
 */

#include "core/MeshTopology.h"
#include "core/detail/MeshValidation.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace manumesh {
namespace {

/** @brief 为一条边累积的临时入射和绕序数据。 */
struct EdgeBuildRecord {
    int edgeId = -1;
};

class FaceComponents {
public:
    explicit FaceComponents(int count)
        : parent_(static_cast<std::size_t>(count)),
          rank_(static_cast<std::size_t>(count), 0) {
        std::iota(parent_.begin(), parent_.end(), 0);
    }

    int find(int value) {
        int root = value;
        while (parent_[static_cast<std::size_t>(root)] != root) {
            root = parent_[static_cast<std::size_t>(root)];
        }
        while (value != root) {
            const int next = parent_[static_cast<std::size_t>(value)];
            parent_[static_cast<std::size_t>(value)] = root;
            value = next;
        }
        return root;
    }

    void join(int lhs, int rhs) {
        int lhsRoot = find(lhs);
        int rhsRoot = find(rhs);
        if (lhsRoot == rhsRoot) {
            return;
        }
        if (rank_[static_cast<std::size_t>(lhsRoot)] < rank_[static_cast<std::size_t>(rhsRoot)]) {
            std::swap(lhsRoot, rhsRoot);
        }
        parent_[static_cast<std::size_t>(rhsRoot)] = lhsRoot;
        if (rank_[static_cast<std::size_t>(lhsRoot)] == rank_[static_cast<std::size_t>(rhsRoot)]) {
            ++rank_[static_cast<std::size_t>(lhsRoot)];
        }
    }

private:
    std::vector<int> parent_;
    std::vector<unsigned char> rank_;
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
    if (mesh.vertices.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
        mesh.faces.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return Status::invalidArgument("Mesh vertex/face count exceeds the supported int-index range.");
    }
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
        // 索引已在上面检查过，几何校验跳过重复的索引扫描。
        if (!detail::validateMeshGeometryLenientAfterIndices(mesh, &error)) {
            return Status::invalidArgument(error.empty() ? "Mesh geometry is invalid." : error);
        }
    }

    MeshTopology topology;
    topology.impl_->vertexCount = static_cast<int>(mesh.vertices.size());
    topology.impl_->faceCount = static_cast<int>(mesh.faces.size());
    topology.impl_->vertices.resize(mesh.vertices.size());

    // 先按面角统计每个顶点的入射数量，给面和边邻接列表一次性预留容量。
    // 对三角网格而言，入射边数不会超过入射面角数，可复用同一上界。
    std::vector<std::size_t> vertexIncidenceReserve(mesh.vertices.size(), 0);
    for (const Face& face : mesh.faces) {
        for (int id : face.v) {
            if (id >= 0 && id < static_cast<int>(vertexIncidenceReserve.size())) {
                ++vertexIncidenceReserve[static_cast<std::size_t>(id)];
            }
        }
    }
    for (std::size_t vertex = 0; vertex < vertexIncidenceReserve.size(); ++vertex) {
        topology.impl_->vertices[vertex].faces.reserve(vertexIncidenceReserve[vertex]);
        topology.impl_->vertices[vertex].edges.reserve(vertexIncidenceReserve[vertex]);
    }

    const std::size_t edgeReserve = mesh.faces.size() <= std::numeric_limits<std::size_t>::max() / 3
                                        ? mesh.faces.size() * 3 / 2
                                        : mesh.faces.size();
    topology.impl_->edges.reserve(edgeReserve);

    std::unordered_map<std::uint64_t, EdgeBuildRecord> edgeByKey;
    edgeByKey.reserve(
        mesh.faces.size() <= std::numeric_limits<std::size_t>::max() / 3 ? mesh.faces.size() * 3 : mesh.faces.size()
    );

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
            auto it = edgeByKey.find(key);
            if (it == edgeByKey.end()) {
                if (topology.impl_->edges.size() >= static_cast<std::size_t>(std::numeric_limits<int>::max())) {
                    return Status::invalidArgument("Mesh unique edge count exceeds the supported int-id range.");
                }
                const int edgeId = static_cast<int>(topology.impl_->edges.size());
                it = edgeByKey.emplace(key, EdgeBuildRecord{edgeId}).first;
                TopologyEdge edge;
                edge.vertices = {std::min(a, b), std::max(a, b)};
                edge.faces.reserve(2);
                edge.faceCorners.reserve(2);
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

Result<MeshTopologySummary> summarizeMeshTopology(const Mesh& mesh) {
    const Result<MeshTopology> topologyResult = MeshTopology::build(mesh);
    if (!topologyResult.ok()) {
        return topologyResult.status();
    }

    const MeshTopology& topology = topologyResult.value();
    MeshTopologySummary summary;
    summary.uniqueEdges = topology.edges().size();
    summary.boundaryEdges = static_cast<std::size_t>(topology.boundaryEdgeCount());
    summary.nonManifoldEdges = static_cast<std::size_t>(topology.nonManifoldEdgeCount());
    if (topology.faceCount() == 0) {
        return summary;
    }

    summary.closedManifold = true;
    summary.consistentlyOriented = true;
    FaceComponents components(topology.faceCount());
    for (const TopologyEdge& edge : topology.edges()) {
        if (edge.faces.empty() || edge.faces.size() != edge.faceCorners.size()) {
            return Status::topologyError("Mesh topology edge incidence is inconsistent.");
        }
        for (std::size_t index = 1; index < edge.faces.size(); ++index) {
            components.join(edge.faces[0], edge.faces[index]);
        }
        if (edge.faces.size() != 2) {
            summary.closedManifold = false;
            if (edge.faces.size() > 2) {
                summary.consistentlyOriented = false;
            }
            continue;
        }

        const Face& firstFace = mesh.faces[static_cast<std::size_t>(edge.faces[0])];
        const Face& secondFace = mesh.faces[static_cast<std::size_t>(edge.faces[1])];
        const int firstCorner = edge.faceCorners[0];
        const int secondCorner = edge.faceCorners[1];
        const int firstStart = firstFace.v[static_cast<std::size_t>(firstCorner)];
        const int firstEnd = firstFace.v[static_cast<std::size_t>((firstCorner + 1) % 3)];
        const int secondStart = secondFace.v[static_cast<std::size_t>(secondCorner)];
        const int secondEnd = secondFace.v[static_cast<std::size_t>((secondCorner + 1) % 3)];
        if (firstStart != secondEnd || firstEnd != secondStart) {
            summary.consistentlyOriented = false;
        }
    }

    for (int face = 0; face < topology.faceCount(); ++face) {
        if (components.find(face) == face) {
            ++summary.connectedFaceComponents;
        }
    }
    return summary;
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
