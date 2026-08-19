/**
 * @file src/simplification/FeatureConstraintGraph.cpp
 * @brief 构建并维护简化器自己的规范特征约束图。
 * @ingroup manumesh_simplification
 */

#include "detail/FeatureConstraintGraph.h"

#include "algorithms/feature_detection/FeatureDetector.h"
#include "common/detail/MeshQueries.h"

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <unordered_map>

namespace manumesh {
namespace simplification {
namespace {

std::uint64_t constraintEdgeKey(int a, int b) { return common::meshEdgeKey(a, b); }

void sortUnique(std::vector<int>& values) {
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
}

void appendUnique(std::vector<int>& values, int value) {
    if (value >= 0 && std::find(values.begin(), values.end(), value) == values.end()) {
        values.push_back(value);
    }
}

void mergeIds(std::vector<int>& target, const std::vector<int>& source) {
    target.insert(target.end(), source.begin(), source.end());
    sortUnique(target);
}

bool hasDetectorEvidence(const feature::FeatureGraphEdge& edge) {
    return edge.boundary || edge.dihedral || edge.normalTensor || edge.smoothCurvature || edge.nonManifold;
}

void mergeConstraintEdge(FeatureConstraintEdge& target, const FeatureConstraintEdge& source) {
    target.active = target.active || source.active;
    target.inputMeshEdge = target.inputMeshEdge || source.inputMeshEdge;
    target.pathBacked = target.pathBacked || source.pathBacked;
    target.protectedFeature = target.protectedFeature || source.protectedFeature;
    // A canonical edge is synthetic only when every merged provenance is synthetic.
    target.syntheticRecovery = target.syntheticRecovery && source.syntheticRecovery;
    target.removedByCleanup = target.removedByCleanup && source.removedByCleanup;
    target.boundary = target.boundary || source.boundary;
    target.dihedral = target.dihedral || source.dihedral;
    target.normalTensor = target.normalTensor || source.normalTensor;
    target.smoothCurvature = target.smoothCurvature || source.smoothCurvature;
    target.nonManifold = target.nonManifold || source.nonManifold;
    target.cleanupBridge = target.cleanupBridge || source.cleanupBridge;
    target.consolidationBridge = target.consolidationBridge || source.consolidationBridge;
    target.signedKind = target.signedKind == source.signedKind ? target.signedKind : 0;
    target.confidence = std::max(target.confidence, source.confidence);
    mergeIds(target.loopIds, source.loopIds);
    mergeIds(target.componentIds, source.componentIds);
}

bool isProtectedConstraintEdge(const FeatureConstraintEdge& edge) {
    return edge.active && edge.protectedFeature && edge.pathBacked && !edge.syntheticRecovery;
}

void eraseEdgeId(std::vector<int>& edgeIds, int edgeId) {
    const auto position = std::find(edgeIds.begin(), edgeIds.end(), edgeId);
    if (position != edgeIds.end()) {
        edgeIds.erase(position);
    }
}

int loopPairCount(const feature::FeatureLoop& loop) {
    if (loop.vertices.size() < 2) {
        return 0;
    }
    return loop.closed ? static_cast<int>(loop.vertices.size()) : static_cast<int>(loop.vertices.size()) - 1;
}

} // namespace

const FeatureConstraintEdge* FeatureConstraintGraph::findEdge(int a, int b) const {
    if (a < 0 || b < 0 || a == b) {
        return nullptr;
    }
    const auto it = activeEdgeByKey_.find(constraintEdgeKey(a, b));
    if (it == activeEdgeByKey_.end() || it->second < 0 || it->second >= static_cast<int>(edges.size())) {
        return nullptr;
    }
    const FeatureConstraintEdge& edge = edges[static_cast<std::size_t>(it->second)];
    return edge.active ? &edge : nullptr;
}

FeatureConstraintEdge* FeatureConstraintGraph::findMutableEdge(int a, int b) {
    if (a < 0 || b < 0 || a == b) {
        return nullptr;
    }
    const auto it = activeEdgeByKey_.find(constraintEdgeKey(a, b));
    if (it == activeEdgeByKey_.end() || it->second < 0 || it->second >= static_cast<int>(edges.size())) {
        return nullptr;
    }
    FeatureConstraintEdge& edge = edges[static_cast<std::size_t>(it->second)];
    return edge.active ? &edge : nullptr;
}

bool FeatureConstraintGraph::isProtectedPathEdge(int a, int b) const {
    const FeatureConstraintEdge* edge = findEdge(a, b);
    return edge != nullptr && isProtectedConstraintEdge(*edge);
}

bool FeatureConstraintGraph::isSyntheticRecoveryEdge(int a, int b) const {
    const FeatureConstraintEdge* edge = findEdge(a, b);
    return edge != nullptr && edge->syntheticRecovery;
}

bool FeatureConstraintGraph::isOnlyProtectedEdgeInComponent(int a, int b) const {
    return protectedComponentVertexCount(a, b) == 2;
}

int FeatureConstraintGraph::protectedComponentVertexCount(int a, int b) const {
    if (!isProtectedPathEdge(a, b) || a < 0 || b < 0 || a >= static_cast<int>(protectedComponentByVertex_.size()) ||
        b >= static_cast<int>(protectedComponentByVertex_.size())) {
        return 0;
    }
    const int componentId = protectedComponentByVertex_[static_cast<std::size_t>(a)];
    if (componentId < 0 || componentId != protectedComponentByVertex_[static_cast<std::size_t>(b)] ||
        componentId >= static_cast<int>(protectedComponentVertexCounts_.size())) {
        return 0;
    }
    return protectedComponentVertexCounts_[static_cast<std::size_t>(componentId)];
}

bool FeatureConstraintGraph::hasProtectedIncidentEdge(int vertex) const {
    if (vertex < 0 || vertex >= static_cast<int>(vertices.size())) {
        return false;
    }
    return vertices[static_cast<std::size_t>(vertex)].protectedFeature;
}

std::vector<int> FeatureConstraintGraph::protectedNeighbors(int vertex) const {
    std::vector<int> result;
    if (vertex < 0 || vertex >= static_cast<int>(activeEdgeIdsByVertex_.size())) {
        return result;
    }
    for (int edgeId : activeEdgeIdsByVertex_[static_cast<std::size_t>(vertex)]) {
        if (edgeId < 0 || edgeId >= static_cast<int>(edges.size())) {
            continue;
        }
        const FeatureConstraintEdge& edge = edges[static_cast<std::size_t>(edgeId)];
        if (!isProtectedConstraintEdge(edge)) {
            continue;
        }
        if (edge.a == vertex) {
            result.push_back(edge.b);
        } else if (edge.b == vertex) {
            result.push_back(edge.a);
        }
    }
    sortUnique(result);
    return result;
}

void FeatureConstraintGraph::addEdgeToIndexes(int edgeId) {
    if (edgeId < 0 || edgeId >= static_cast<int>(edges.size())) {
        return;
    }
    const FeatureConstraintEdge& edge = edges[static_cast<std::size_t>(edgeId)];
    if (!edge.active || edge.a < 0 || edge.b < 0 || edge.a == edge.b ||
        edge.a >= static_cast<int>(activeEdgeIdsByVertex_.size()) ||
        edge.b >= static_cast<int>(activeEdgeIdsByVertex_.size())) {
        return;
    }
    activeEdgeByKey_[constraintEdgeKey(edge.a, edge.b)] = edgeId;
    activeEdgeIdsByVertex_[static_cast<std::size_t>(edge.a)].push_back(edgeId);
    activeEdgeIdsByVertex_[static_cast<std::size_t>(edge.b)].push_back(edgeId);
}

void FeatureConstraintGraph::removeEdgeFromIndexes(int edgeId) {
    if (edgeId < 0 || edgeId >= static_cast<int>(edges.size())) {
        return;
    }
    FeatureConstraintEdge& edge = edges[static_cast<std::size_t>(edgeId)];
    if (!edge.active) {
        return;
    }
    const std::uint64_t key = constraintEdgeKey(edge.a, edge.b);
    const auto indexed = activeEdgeByKey_.find(key);
    if (indexed != activeEdgeByKey_.end() && indexed->second == edgeId) {
        activeEdgeByKey_.erase(indexed);
    }
    if (edge.a >= 0 && edge.a < static_cast<int>(activeEdgeIdsByVertex_.size())) {
        eraseEdgeId(activeEdgeIdsByVertex_[static_cast<std::size_t>(edge.a)], edgeId);
    }
    if (edge.b >= 0 && edge.b < static_cast<int>(activeEdgeIdsByVertex_.size())) {
        eraseEdgeId(activeEdgeIdsByVertex_[static_cast<std::size_t>(edge.b)], edgeId);
    }
    edge.active = false;
}

void FeatureConstraintGraph::refreshVertex(int vertex) {
    if (vertex < 0 || vertex >= static_cast<int>(vertices.size())) {
        return;
    }
    FeatureConstraintVertex& constraintVertex = vertices[static_cast<std::size_t>(vertex)];
    constraintVertex.protectedFeature = false;
    constraintVertex.junction = false;
    constraintVertex.shared = false;
    constraintVertex.endpoint = false;
    constraintVertex.ambiguousJunction = false;
    constraintVertex.loopIds.clear();
    constraintVertex.componentIds.clear();

    int protectedDegree = 0;
    if (vertex < static_cast<int>(activeEdgeIdsByVertex_.size())) {
        for (int edgeId : activeEdgeIdsByVertex_[static_cast<std::size_t>(vertex)]) {
            if (edgeId < 0 || edgeId >= static_cast<int>(edges.size())) {
                continue;
            }
            const FeatureConstraintEdge& edge = edges[static_cast<std::size_t>(edgeId)];
            if (!isProtectedConstraintEdge(edge)) {
                continue;
            }
            ++protectedDegree;
            mergeIds(constraintVertex.loopIds, edge.loopIds);
            mergeIds(constraintVertex.componentIds, edge.componentIds);
        }
    }
    constraintVertex.protectedFeature = protectedDegree > 0;
    constraintVertex.junction = protectedDegree > 2;
    constraintVertex.shared = constraintVertex.loopIds.size() > 1;
    constraintVertex.endpoint = protectedDegree == 1;
    constraintVertex.ambiguousJunction = constraintVertex.junction && constraintVertex.sourceAmbiguousJunction;
}

void FeatureConstraintGraph::rebuildProtectedComponents() {
    protectedComponentByVertex_.assign(vertices.size(), -1);
    protectedComponentVertexCounts_.clear();
    std::vector<int> stack;
    for (int seed = 0; seed < static_cast<int>(vertices.size()); ++seed) {
        if (!vertices[static_cast<std::size_t>(seed)].protectedFeature ||
            protectedComponentByVertex_[static_cast<std::size_t>(seed)] >= 0) {
            continue;
        }
        const int componentId = static_cast<int>(protectedComponentVertexCounts_.size());
        int vertexCount = 0;
        stack.clear();
        stack.push_back(seed);
        protectedComponentByVertex_[static_cast<std::size_t>(seed)] = componentId;
        while (!stack.empty()) {
            const int vertex = stack.back();
            stack.pop_back();
            ++vertexCount;
            for (int edgeId : activeEdgeIdsByVertex_[static_cast<std::size_t>(vertex)]) {
                const FeatureConstraintEdge& edge = edges[static_cast<std::size_t>(edgeId)];
                if (!isProtectedConstraintEdge(edge)) {
                    continue;
                }
                const int neighbor = edge.a == vertex ? edge.b : edge.a;
                if (protectedComponentByVertex_[static_cast<std::size_t>(neighbor)] >= 0) {
                    continue;
                }
                protectedComponentByVertex_[static_cast<std::size_t>(neighbor)] = componentId;
                stack.push_back(neighbor);
            }
        }
        protectedComponentVertexCounts_.push_back(vertexCount);
    }
}

void FeatureConstraintGraph::rebuildIndex() {
    activeEdgeByKey_.clear();
    for (int edgeId = 0; edgeId < static_cast<int>(edges.size()); ++edgeId) {
        FeatureConstraintEdge& edge = edges[static_cast<std::size_t>(edgeId)];
        if (!edge.active || edge.a < 0 || edge.b < 0 || edge.a == edge.b) {
            continue;
        }
        if (edge.b < edge.a) {
            std::swap(edge.a, edge.b);
        }
        const std::uint64_t key = constraintEdgeKey(edge.a, edge.b);
        const auto existing = activeEdgeByKey_.find(key);
        if (existing == activeEdgeByKey_.end()) {
            activeEdgeByKey_[key] = edgeId;
            continue;
        }
        mergeConstraintEdge(edges[static_cast<std::size_t>(existing->second)], edge);
        edge.active = false;
    }

    activeEdgeIdsByVertex_.assign(vertices.size(), std::vector<int>{});
    for (int edgeId = 0; edgeId < static_cast<int>(edges.size()); ++edgeId) {
        const FeatureConstraintEdge& edge = edges[static_cast<std::size_t>(edgeId)];
        if (!edge.active || edge.a < 0 || edge.b < 0 || edge.a >= static_cast<int>(vertices.size()) ||
            edge.b >= static_cast<int>(vertices.size())) {
            continue;
        }
        activeEdgeIdsByVertex_[static_cast<std::size_t>(edge.a)].push_back(edgeId);
        activeEdgeIdsByVertex_[static_cast<std::size_t>(edge.b)].push_back(edgeId);
    }
    for (int vertex = 0; vertex < static_cast<int>(vertices.size()); ++vertex) {
        refreshVertex(vertex);
    }
    rebuildProtectedComponents();
}

bool FeatureConstraintGraph::contractVertex(int keep, int remove) {
    if (keep < 0 || remove < 0 || keep == remove || keep >= static_cast<int>(vertices.size()) ||
        remove >= static_cast<int>(vertices.size())) {
        return false;
    }
    if (activeEdgeIdsByVertex_.size() != vertices.size()) {
        rebuildIndex();
    }
    const std::vector<int> incidentEdgeIds = activeEdgeIdsByVertex_[static_cast<std::size_t>(remove)];
    if (incidentEdgeIds.empty()) {
        return false;
    }

    const bool contractedProtectedPath = isProtectedPathEdge(keep, remove);
    const int protectedComponentId =
        contractedProtectedPath && keep < static_cast<int>(protectedComponentByVertex_.size())
            ? protectedComponentByVertex_[static_cast<std::size_t>(keep)]
            : -1;
    bool protectedTopologyNeedsRebuild = false;
    std::vector<int> affectedVertices;
    appendUnique(affectedVertices, keep);
    appendUnique(affectedVertices, remove);

    for (int edgeId : incidentEdgeIds) {
        if (edgeId < 0 || edgeId >= static_cast<int>(edges.size())) {
            continue;
        }
        const FeatureConstraintEdge original = edges[static_cast<std::size_t>(edgeId)];
        if (!original.active || (original.a != remove && original.b != remove)) {
            continue;
        }
        const int neighbor = original.a == remove ? original.b : original.a;
        appendUnique(affectedVertices, neighbor);
        protectedTopologyNeedsRebuild =
            protectedTopologyNeedsRebuild || (isProtectedConstraintEdge(original) && !contractedProtectedPath);
        removeEdgeFromIndexes(edgeId);
        if (neighbor == keep) {
            continue;
        }

        FeatureConstraintEdge mapped = original;
        mapped.active = true;
        mapped.a = std::min(keep, neighbor);
        mapped.b = std::max(keep, neighbor);
        mapped.inputMeshEdge = false;
        mapped.pathBacked = contractedProtectedPath && isProtectedConstraintEdge(original);
        mapped.protectedFeature = mapped.pathBacked;
        mapped.syntheticRecovery = original.syntheticRecovery && !mapped.protectedFeature;

        const auto existing = activeEdgeByKey_.find(constraintEdgeKey(mapped.a, mapped.b));
        if (existing != activeEdgeByKey_.end()) {
            mergeConstraintEdge(edges[static_cast<std::size_t>(existing->second)], mapped);
            continue;
        }
        edges[static_cast<std::size_t>(edgeId)] = mapped;
        addEdgeToIndexes(edgeId);
    }

    if (contractedProtectedPath) {
        FeatureConstraintVertex& kept = vertices[static_cast<std::size_t>(keep)];
        const FeatureConstraintVertex& removed = vertices[static_cast<std::size_t>(remove)];
        mergeIds(kept.sourceLoopIds, removed.sourceLoopIds);
        mergeIds(kept.sourceComponentIds, removed.sourceComponentIds);
        kept.sourceJunction = kept.sourceJunction || removed.sourceJunction;
        kept.sourceShared = kept.sourceShared || removed.sourceShared;
        kept.sourceEndpoint = kept.sourceEndpoint || removed.sourceEndpoint;
        kept.sourceAmbiguousJunction = kept.sourceAmbiguousJunction || removed.sourceAmbiguousJunction;
        kept.confidence = std::max(kept.confidence, removed.confidence);
    }

    for (int vertex : affectedVertices) {
        refreshVertex(vertex);
    }
    if (contractedProtectedPath && protectedComponentId >= 0 &&
        protectedComponentId < static_cast<int>(protectedComponentVertexCounts_.size())) {
        protectedComponentByVertex_[static_cast<std::size_t>(remove)] = -1;
        --protectedComponentVertexCounts_[static_cast<std::size_t>(protectedComponentId)];
    } else if (protectedTopologyNeedsRebuild) {
        rebuildProtectedComponents();
    }
    return true;
}

FeatureConstraintGraph buildFeatureConstraintGraph(const Mesh& mesh, const feature::FeatureAnalysis& analysis) {
    FeatureConstraintGraph graph;
    graph.vertices.resize(mesh.vertices.size());
    const common::MeshEdgeInfoMap meshEdges = common::buildMeshEdgeInfo(mesh);

    std::unordered_map<int, double> componentConfidence;
    for (const feature::FeatureComponent& component : analysis.components) {
        componentConfidence[component.id] = component.confidence;
    }

    for (int vertex = 0; vertex < static_cast<int>(graph.vertices.size()); ++vertex) {
        FeatureConstraintVertex& target = graph.vertices[static_cast<std::size_t>(vertex)];
        if (vertex < static_cast<int>(analysis.graph.vertices.size())) {
            const feature::FeatureGraphVertex& source = analysis.graph.vertices[static_cast<std::size_t>(vertex)];
            target.sourceLoopIds = source.loopIds;
            target.sourceJunction = source.junction;
            target.sourceShared = source.shared;
            target.sourceEndpoint = source.endpoint;
            target.sourceAmbiguousJunction = source.ambiguousJunction;
        }
        if (vertex < static_cast<int>(analysis.vertices.size())) {
            const feature::VertexFeature& source = analysis.vertices[static_cast<std::size_t>(vertex)];
            appendUnique(target.sourceLoopIds, source.loopId);
            appendUnique(target.sourceComponentIds, source.componentId);
            target.sourceJunction = target.sourceJunction || source.junction;
            target.confidence = std::max(target.confidence, source.confidence);
        }
        sortUnique(target.sourceLoopIds);
        sortUnique(target.sourceComponentIds);
    }

    graph.edges.reserve(analysis.graph.edges.size());
    for (const feature::FeatureGraphEdge& source : analysis.graph.edges) {
        if (source.a < 0 || source.b < 0 || source.a == source.b ||
            source.a >= static_cast<int>(mesh.vertices.size()) || source.b >= static_cast<int>(mesh.vertices.size())) {
            throw std::invalid_argument("FeatureAnalysis contains an invalid feature graph edge.");
        }
        FeatureConstraintEdge edge;
        edge.a = std::min(source.a, source.b);
        edge.b = std::max(source.a, source.b);
        edge.active = !source.removedByCleanup;
        edge.inputMeshEdge = meshEdges.find(constraintEdgeKey(edge.a, edge.b)) != meshEdges.end();
        edge.cleanupBridge = source.cleanupBridge;
        edge.consolidationBridge = source.consolidationBridge;
        edge.syntheticRecovery = source.cleanupBridge || source.consolidationBridge || !edge.inputMeshEdge;
        edge.removedByCleanup = source.removedByCleanup;
        edge.boundary = source.boundary;
        edge.dihedral = source.dihedral;
        edge.normalTensor = source.normalTensor;
        edge.smoothCurvature = source.smoothCurvature;
        edge.nonManifold = source.nonManifold;
        edge.signedKind = source.signedKind;
        edge.protectedFeature =
            edge.active && edge.inputMeshEdge && !edge.syntheticRecovery && hasDetectorEvidence(source);
        edge.pathBacked = edge.protectedFeature;
        if (source.a < static_cast<int>(analysis.vertices.size())) {
            edge.confidence =
                std::max(edge.confidence, analysis.vertices[static_cast<std::size_t>(source.a)].confidence);
            appendUnique(edge.componentIds, analysis.vertices[static_cast<std::size_t>(source.a)].componentId);
        }
        if (source.b < static_cast<int>(analysis.vertices.size())) {
            edge.confidence =
                std::max(edge.confidence, analysis.vertices[static_cast<std::size_t>(source.b)].confidence);
            appendUnique(edge.componentIds, analysis.vertices[static_cast<std::size_t>(source.b)].componentId);
        }
        graph.edges.push_back(edge);
    }
    graph.rebuildIndex();

    for (const feature::FeatureLoop& loop : analysis.loops) {
        const int pairCount = loopPairCount(loop);
        for (int vertex : loop.vertices) {
            if (vertex < 0 || vertex >= static_cast<int>(graph.vertices.size())) {
                throw std::invalid_argument("FeatureAnalysis contains an invalid feature loop vertex index.");
            }
            FeatureConstraintVertex& target = graph.vertices[static_cast<std::size_t>(vertex)];
            appendUnique(target.sourceLoopIds, loop.id);
            appendUnique(target.sourceComponentIds, loop.componentId);
            target.confidence = std::max(target.confidence, loop.componentConfidence);
        }
        for (int pair = 0; pair < pairCount; ++pair) {
            const int a = loop.vertices[static_cast<std::size_t>(pair)];
            const int b = loop.vertices[static_cast<std::size_t>(pair + 1) % loop.vertices.size()];
            FeatureConstraintEdge* edge = graph.findMutableEdge(a, b);
            if (edge == nullptr) {
                continue;
            }
            appendUnique(edge->loopIds, loop.id);
            appendUnique(edge->componentIds, loop.componentId);
            edge->confidence = std::max(edge->confidence, loop.componentConfidence);
        }
    }

    for (FeatureConstraintVertex& vertex : graph.vertices) {
        for (int componentId : vertex.sourceComponentIds) {
            const auto confidence = componentConfidence.find(componentId);
            if (confidence != componentConfidence.end()) {
                vertex.confidence = std::max(vertex.confidence, confidence->second);
            }
        }
        sortUnique(vertex.sourceLoopIds);
        sortUnique(vertex.sourceComponentIds);
    }
    for (FeatureConstraintEdge& edge : graph.edges) {
        for (int componentId : edge.componentIds) {
            const auto confidence = componentConfidence.find(componentId);
            if (confidence != componentConfidence.end()) {
                edge.confidence = std::max(edge.confidence, confidence->second);
            }
        }
        sortUnique(edge.loopIds);
        sortUnique(edge.componentIds);
    }
    graph.rebuildIndex();
    return graph;
}

} // namespace simplification
} // namespace manumesh
