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
#include <mutex>
#include <numeric>
#include <new>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace manumesh {
namespace {

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

/**
 * @brief MeshTopology 的紧凑邻接存储。
 *
 * 边和顶点不再各自拥有小 vector；所有入射索引位于连续数组，offsets 描述每个
 * 实体的半开区间。旧的 vector 结构仅在兼容 API 被调用时按需生成。
 */
struct MeshTopology::Impl {
    int vertexCount = 0;
    int faceCount = 0;
    int boundaryEdgeCount = 0;
    int nonManifoldEdgeCount = 0;

    std::vector<std::array<int, 2>> edgeVertices;
    std::vector<std::size_t> edgeFaceOffsets;
    std::vector<int> edgeFaces;
    std::vector<std::uint8_t> edgeFaceCorners;

    std::vector<std::size_t> vertexFaceOffsets;
    std::vector<int> vertexFaces;
    std::vector<std::size_t> vertexEdgeOffsets;
    std::vector<int> vertexEdges;

    // Legacy vector-returning methods are intentionally lazy. The mutex keeps those
    // caches safe for concurrent read-only callers without affecting compact views.
    mutable std::mutex materializationMutex;
    mutable bool allEdgesMaterialized = false;
    mutable std::vector<TopologyEdge> materializedEdges;
    mutable std::unordered_map<int, std::unique_ptr<VertexTopology>> materializedVertexItems;

    Impl()
        : edgeFaceOffsets(1, 0),
          vertexFaceOffsets(1, 0),
          vertexEdgeOffsets(1, 0) {}

    // Caches are deliberately omitted from copies. They are compatibility artifacts;
    // copying the immutable compact payload is enough and avoids multiplying legacy
    // allocations merely because a caller queried the source object first.
    Impl(const Impl& other)
        : vertexCount(other.vertexCount),
          faceCount(other.faceCount),
          boundaryEdgeCount(other.boundaryEdgeCount),
          nonManifoldEdgeCount(other.nonManifoldEdgeCount),
          edgeVertices(other.edgeVertices),
          edgeFaceOffsets(other.edgeFaceOffsets),
          edgeFaces(other.edgeFaces),
          edgeFaceCorners(other.edgeFaceCorners),
          vertexFaceOffsets(other.vertexFaceOffsets),
          vertexFaces(other.vertexFaces),
          vertexEdgeOffsets(other.vertexEdgeOffsets),
          vertexEdges(other.vertexEdges) {}

    TopologyEdge makeEdge(int edgeId) const {
        TopologyEdge result;
        result.vertices = edgeVertices[static_cast<std::size_t>(edgeId)];
        const std::size_t begin = edgeFaceOffsets[static_cast<std::size_t>(edgeId)];
        const std::size_t end = edgeFaceOffsets[static_cast<std::size_t>(edgeId) + 1];
        result.faces.assign(edgeFaces.begin() + static_cast<std::ptrdiff_t>(begin),
                            edgeFaces.begin() + static_cast<std::ptrdiff_t>(end));
        result.faceCorners.reserve(end - begin);
        for (std::size_t index = begin; index < end; ++index) {
            result.faceCorners.push_back(static_cast<int>(edgeFaceCorners[index]));
        }
        return result;
    }

    VertexTopology makeVertex(int vertexId) const {
        VertexTopology result;
        const std::size_t index = static_cast<std::size_t>(vertexId);
        const std::size_t faceBegin = vertexFaceOffsets[index];
        const std::size_t faceEnd = vertexFaceOffsets[index + 1];
        const std::size_t edgeBegin = vertexEdgeOffsets[index];
        const std::size_t edgeEnd = vertexEdgeOffsets[index + 1];
        result.faces.assign(vertexFaces.begin() + static_cast<std::ptrdiff_t>(faceBegin),
                            vertexFaces.begin() + static_cast<std::ptrdiff_t>(faceEnd));
        result.edges.assign(vertexEdges.begin() + static_cast<std::ptrdiff_t>(edgeBegin),
                            vertexEdges.begin() + static_cast<std::ptrdiff_t>(edgeEnd));
        return result;
    }
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
    try {
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

        const std::size_t vertexCount = mesh.vertices.size();
        const std::size_t faceCount = mesh.faces.size();
        std::vector<std::size_t> vertexFaceCounts(vertexCount, 0);
        std::vector<std::size_t> edgeFaceCounts;
        const std::size_t edgeReserve =
            faceCount <= std::numeric_limits<std::size_t>::max() / 3 ? faceCount * 3 / 2 : faceCount;
        topology.impl_->edgeVertices.reserve(edgeReserve);
        edgeFaceCounts.reserve(edgeReserve);

        // The hash table is only a construction aid. The resulting topology owns no hash
        // nodes and stores all incidences in flat arrays.
        std::unordered_map<std::uint64_t, int> edgeByKey;
        edgeByKey.reserve(faceCount <= std::numeric_limits<std::size_t>::max() / 3 ? faceCount * 3 : faceCount);

        // First pass establishes stable first-seen edge IDs and all prefix-sum counts.
        for (int fi = 0; fi < static_cast<int>(faceCount); ++fi) {
            const Face& face = mesh.faces[static_cast<std::size_t>(fi)];
            for (int corner = 0; corner < 3; ++corner) {
                const int id = face.v[corner];
                if (validate && (id < 0 || id >= static_cast<int>(vertexCount))) {
                    return Status::invalidArgument("Mesh face references an invalid vertex index.");
                }
                if (id >= 0 && id < static_cast<int>(vertexCount)) {
                    ++vertexFaceCounts[static_cast<std::size_t>(id)];
                }
            }

            for (int corner = 0; corner < 3; ++corner) {
                const int a = face.v[corner];
                const int b = face.v[(corner + 1) % 3];
                const std::uint64_t key = topologyEdgeKey(a, b);
                const auto found = edgeByKey.find(key);
                int edgeId = -1;
                if (found == edgeByKey.end()) {
                    if (topology.impl_->edgeVertices.size() >=
                        static_cast<std::size_t>(std::numeric_limits<int>::max())) {
                        return Status::invalidArgument("Mesh unique edge count exceeds the supported int-id range.");
                    }
                    edgeId = static_cast<int>(topology.impl_->edgeVertices.size());
                    edgeByKey.emplace(key, edgeId);
                    topology.impl_->edgeVertices.push_back({{std::min(a, b), std::max(a, b)}});
                    edgeFaceCounts.push_back(0);
                } else {
                    edgeId = found->second;
                }
                ++edgeFaceCounts[static_cast<std::size_t>(edgeId)];
            }
        }

        auto makeOffsets = [](const std::vector<std::size_t>& counts, std::vector<std::size_t>& offsets) {
            offsets.resize(counts.size() + 1);
            offsets[0] = 0;
            for (std::size_t index = 0; index < counts.size(); ++index) {
                if (counts[index] > std::numeric_limits<std::size_t>::max() - offsets[index]) {
                    return false;
                }
                offsets[index + 1] = offsets[index] + counts[index];
            }
            return true;
        };

        if (!makeOffsets(edgeFaceCounts, topology.impl_->edgeFaceOffsets) ||
            !makeOffsets(vertexFaceCounts, topology.impl_->vertexFaceOffsets)) {
            return Status::invalidArgument("Mesh topology incidence size overflows the supported range.");
        }
        topology.impl_->edgeFaces.resize(topology.impl_->edgeFaceOffsets.back());
        topology.impl_->edgeFaceCorners.resize(topology.impl_->edgeFaceOffsets.back());
        topology.impl_->vertexFaces.resize(topology.impl_->vertexFaceOffsets.back());

        // Reuse the count arrays as write cursors, avoiding an additional offsets-sized
        // allocation while filling the contiguous incidence payload.
        std::copy(
            topology.impl_->edgeFaceOffsets.begin(), topology.impl_->edgeFaceOffsets.end() - 1, edgeFaceCounts.begin()
        );
        std::copy(
            topology.impl_->vertexFaceOffsets.begin(),
            topology.impl_->vertexFaceOffsets.end() - 1,
            vertexFaceCounts.begin()
        );

        // Second pass preserves the old deterministic order: faces follow input order,
        // and each edge's corners follow the order in which that edge was encountered.
        for (int fi = 0; fi < static_cast<int>(faceCount); ++fi) {
            const Face& face = mesh.faces[static_cast<std::size_t>(fi)];
            for (int corner = 0; corner < 3; ++corner) {
                const int id = face.v[corner];
                if (id >= 0 && id < static_cast<int>(vertexCount)) {
                    const std::size_t vertex = static_cast<std::size_t>(id);
                    const std::size_t write = vertexFaceCounts[vertex]++;
                    topology.impl_->vertexFaces[write] = fi;
                }
            }
            for (int corner = 0; corner < 3; ++corner) {
                const int a = face.v[corner];
                const int b = face.v[(corner + 1) % 3];
                const auto found = edgeByKey.find(topologyEdgeKey(a, b));
                if (found == edgeByKey.end()) {
                    return Status::topologyError("Mesh topology edge lookup failed during incidence fill.");
                }
                const std::size_t edge = static_cast<std::size_t>(found->second);
                const std::size_t write = edgeFaceCounts[edge]++;
                topology.impl_->edgeFaces[write] = fi;
                topology.impl_->edgeFaceCorners[write] = static_cast<std::uint8_t>(corner);
            }
        }

        for (std::size_t edge = 0; edge < edgeFaceCounts.size(); ++edge) {
            const std::size_t incidence =
                topology.impl_->edgeFaceOffsets[edge + 1] - topology.impl_->edgeFaceOffsets[edge];
            if (incidence == 1) {
                ++topology.impl_->boundaryEdgeCount;
            } else if (incidence > 2) {
                ++topology.impl_->nonManifoldEdgeCount;
            }
        }

        std::unordered_map<std::uint64_t, int>().swap(edgeByKey);
        std::vector<std::size_t>().swap(edgeFaceCounts);

        // The vertex-face cursors are no longer needed, so reuse their allocation for
        // vertex-edge counts. On a large mesh this avoids retaining a second V-sized
        // temporary during the final CSR allocation.
        std::fill(vertexFaceCounts.begin(), vertexFaceCounts.end(), 0);
        std::vector<std::size_t>& vertexEdgeCounts = vertexFaceCounts;
        for (const std::array<int, 2>& vertices : topology.impl_->edgeVertices) {
            for (int vertex : vertices) {
                if (vertex >= 0 && vertex < static_cast<int>(vertexCount)) {
                    ++vertexEdgeCounts[static_cast<std::size_t>(vertex)];
                }
            }
        }
        if (!makeOffsets(vertexEdgeCounts, topology.impl_->vertexEdgeOffsets)) {
            return Status::invalidArgument("Mesh vertex-edge incidence size overflows the supported range.");
        }
        topology.impl_->vertexEdges.resize(topology.impl_->vertexEdgeOffsets.back());
        std::copy(
            topology.impl_->vertexEdgeOffsets.begin(),
            topology.impl_->vertexEdgeOffsets.end() - 1,
            vertexEdgeCounts.begin()
        );
        for (std::size_t edge = 0; edge < topology.impl_->edgeVertices.size(); ++edge) {
            for (int vertex : topology.impl_->edgeVertices[edge]) {
                if (vertex >= 0 && vertex < static_cast<int>(vertexCount)) {
                    const std::size_t index = static_cast<std::size_t>(vertex);
                    topology.impl_->vertexEdges[vertexEdgeCounts[index]++] = static_cast<int>(edge);
                }
            }
        }

        return topology;
    } catch (const std::bad_alloc&) {
        return Status(StatusCode::OutOfMemory, "Insufficient memory to build mesh topology.");
    } catch (const std::length_error&) {
        return Status(StatusCode::OutOfMemory, "Mesh topology allocation exceeds the supported size range.");
    }
}

Result<MeshTopologySummary> summarizeMeshTopology(const Mesh& mesh) {
    const Result<MeshTopology> topologyResult = MeshTopology::build(mesh);
    if (!topologyResult.ok()) {
        return topologyResult.status();
    }

    const MeshTopology& topology = topologyResult.value();
    MeshTopologySummary summary;
    summary.uniqueEdges = static_cast<std::size_t>(topology.edgeCount());
    summary.boundaryEdges = static_cast<std::size_t>(topology.boundaryEdgeCount());
    summary.nonManifoldEdges = static_cast<std::size_t>(topology.nonManifoldEdgeCount());
    if (topology.faceCount() == 0) {
        return summary;
    }

    summary.closedManifold = true;
    summary.consistentlyOriented = true;
    FaceComponents components(topology.faceCount());
    for (int edgeId = 0; edgeId < topology.edgeCount(); ++edgeId) {
        const TopologyEdgeView edge = topology.edgeView(EdgeId{edgeId});
        if (edge.faces.empty() || edge.faces.size() != edge.faceCornerCount()) {
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
        const int firstCorner = edge.faceCorner(0);
        const int secondCorner = edge.faceCorner(1);
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

int MeshTopology::edgeCount() const { return impl_ ? static_cast<int>(impl_->edgeVertices.size()) : 0; }

int MeshTopology::boundaryEdgeCount() const { return impl_ ? impl_->boundaryEdgeCount : 0; }

int MeshTopology::nonManifoldEdgeCount() const { return impl_ ? impl_->nonManifoldEdgeCount : 0; }

const std::vector<TopologyEdge>& MeshTopology::edges() const {
    static const std::vector<TopologyEdge> empty;
    if (!impl_) {
        return empty;
    }
    std::lock_guard<std::mutex> lock(impl_->materializationMutex);
    if (!impl_->allEdgesMaterialized) {
        std::vector<TopologyEdge> materialized;
        materialized.reserve(impl_->edgeVertices.size());
        for (int edgeId = 0; edgeId < static_cast<int>(impl_->edgeVertices.size()); ++edgeId) {
            materialized.push_back(impl_->makeEdge(edgeId));
        }
        impl_->materializedEdges = std::move(materialized);
        impl_->allEdgesMaterialized = true;
    }
    return impl_->materializedEdges;
}

bool MeshTopology::hasEdge(EdgeId id) const {
    return impl_ && id.id >= 0 && id.id < static_cast<int>(impl_->edgeVertices.size());
}

bool MeshTopology::hasVertex(VertexId id) const {
    return impl_ && id.id >= 0 && id.id < impl_->vertexCount;
}

const TopologyEdge& MeshTopology::edge(EdgeId id) const {
    if (!hasEdge(id)) {
        throw std::out_of_range("MeshTopology edge id is out of range.");
    }
    return edges()[static_cast<std::size_t>(id.id)];
}

const VertexTopology& MeshTopology::vertex(VertexId id) const {
    if (!hasVertex(id)) {
        throw std::out_of_range("MeshTopology vertex id is out of range.");
    }
    std::lock_guard<std::mutex> lock(impl_->materializationMutex);
    const auto found = impl_->materializedVertexItems.find(id.id);
    if (found != impl_->materializedVertexItems.end()) {
        return *found->second;
    }
    std::unique_ptr<VertexTopology> value = std::make_unique<VertexTopology>(impl_->makeVertex(id.id));
    const VertexTopology* result = value.get();
    impl_->materializedVertexItems.emplace(id.id, std::move(value));
    return *result;
}

TopologyEdgeView MeshTopology::edgeView(EdgeId id) const {
    if (!hasEdge(id)) {
        throw std::out_of_range("MeshTopology edge id is out of range.");
    }
    const std::size_t index = static_cast<std::size_t>(id.id);
    const std::size_t begin = impl_->edgeFaceOffsets[index];
    const std::size_t end = impl_->edgeFaceOffsets[index + 1];
    TopologyEdgeView result;
    result.vertices = impl_->edgeVertices[index];
    result.faces = TopologyIndexView(impl_->edgeFaces.data() + begin, end - begin);
    result.faceCorners_ = impl_->edgeFaceCorners.data() + begin;
    return result;
}

VertexTopologyView MeshTopology::vertexView(VertexId id) const {
    if (!hasVertex(id)) {
        throw std::out_of_range("MeshTopology vertex id is out of range.");
    }
    const std::size_t index = static_cast<std::size_t>(id.id);
    const std::size_t faceBegin = impl_->vertexFaceOffsets[index];
    const std::size_t faceEnd = impl_->vertexFaceOffsets[index + 1];
    const std::size_t edgeBegin = impl_->vertexEdgeOffsets[index];
    const std::size_t edgeEnd = impl_->vertexEdgeOffsets[index + 1];
    VertexTopologyView result;
    result.faces = TopologyIndexView(
        faceBegin == faceEnd ? nullptr : impl_->vertexFaces.data() + faceBegin,
        faceEnd - faceBegin
    );
    result.edges = TopologyIndexView(
        edgeBegin == edgeEnd ? nullptr : impl_->vertexEdges.data() + edgeBegin,
        edgeEnd - edgeBegin
    );
    return result;
}

} // 命名空间 manumesh
