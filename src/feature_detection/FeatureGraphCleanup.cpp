#include "detail/FeatureGraphCleanup.h"

#include "common/detail/MeshQueries.h"
#include "detail/FeatureGraph.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <queue>
#include <unordered_set>

namespace manumesh::feature::detector_detail {
namespace {

struct EndpointCandidate {
    int vertex = -1;
    int neighbor = -1;
    Vec3 outward = Vec3(1.0, 0.0, 0.0);
    double scale = 0.0;
};

struct GapCandidate {
    int a = -1;
    int b = -1;
    double distance = 0.0;
};

double meanPositiveScale(const std::vector<double>& scales, double fallback) {
    double sum = 0.0;
    int count = 0;
    for (double value : scales) {
        if (std::isfinite(value) && value > 1e-12) {
            sum += value;
            ++count;
        }
    }
    return count > 0 ? sum / static_cast<double>(count) : fallback;
}

double vertexScale(const std::vector<double>& scales, int vertex, double fallback) {
    if (vertex >= 0 && vertex < static_cast<int>(scales.size()) && std::isfinite(scales[vertex]) &&
        scales[vertex] > 1e-12) {
        return scales[vertex];
    }
    return fallback;
}

bool isWeakCleanupSpurEdge(const TraceGraph& trace, int a, int b) {
    return traceEdgeNormalTensor(trace, a, b) && !traceEdgeBoundary(trace, a, b) && !traceEdgeDihedral(trace, a, b) &&
           !traceEdgeNonManifold(trace, a, b) && !traceEdgeCleanupBridge(trace, a, b);
}

void markFeatureGraphEdgeRemoved(FeatureAnalysis& analysis, int a, int b) {
    for (FeatureGraphEdge& edge : analysis.graph.edges) {
        const bool sameDirection = edge.a == a && edge.b == b;
        const bool reverseDirection = edge.a == b && edge.b == a;
        if ((sameDirection || reverseDirection) && !edge.removedByCleanup) {
            edge.removedByCleanup = true;
            return;
        }
    }
}

std::vector<std::pair<int, int>> traceShortWeakSpur(const TraceGraph& trace, int seed, int maxEdges) {
    std::vector<std::pair<int, int>> path;
    if (maxEdges <= 0 || seed < 0 || seed >= static_cast<int>(trace.adjacency.size()) ||
        trace.adjacency[seed].size() != 1) {
        return path;
    }

    int previous = -1;
    int current = seed;
    while (static_cast<int>(path.size()) < maxEdges) {
        int next = -1;
        for (int candidate : trace.adjacency[current]) {
            if (candidate != previous) {
                next = candidate;
                break;
            }
        }
        if (next < 0 || !isWeakCleanupSpurEdge(trace, current, next)) {
            path.clear();
            return path;
        }
        path.emplace_back(current, next);
        previous = current;
        current = next;
        if (current < 0 || current >= static_cast<int>(trace.adjacency.size())) {
            path.clear();
            return path;
        }
        if (trace.adjacency[current].size() != 2) {
            return path;
        }
    }

    path.clear();
    return path;
}

void removeWeakSpurs(const FeatureOptions& options, TraceGraph& trace, FeatureAnalysis& analysis) {
    const int maxEdges = std::max(0, options.featureGraphMaxWeakSpurEdges);
    if (maxEdges <= 0) {
        return;
    }

    bool changed = true;
    int passes = 0;
    while (changed && passes++ < 8) {
        changed = false;
        std::unordered_set<std::uint64_t> removeKeys;
        std::vector<std::pair<int, int>> removeEdges;
        for (int seed = 0; seed < static_cast<int>(trace.adjacency.size()); ++seed) {
            if (trace.adjacency[seed].size() != 1) {
                continue;
            }
            for (const auto& edge : traceShortWeakSpur(trace, seed, maxEdges)) {
                const std::uint64_t key = manumesh::detail::meshEdgeKey(edge.first, edge.second);
                if (removeKeys.insert(key).second) {
                    removeEdges.push_back(edge);
                }
            }
        }

        if (removeEdges.empty()) {
            break;
        }
        int removedEdgeCount = 0;
        for (const auto& [a, b] : removeEdges) {
            if (!traceGraphHasEdge(trace, a, b)) {
                continue;
            }
            removeTraceGraphEdge(trace, a, b);
            markFeatureGraphEdgeRemoved(analysis, a, b);
            ++removedEdgeCount;
            changed = true;
        }
        if (changed) {
            analysis.graphCleanupRemovedSpurs += removedEdgeCount;
            rebuildTraceGraphEdges(trace);
        }
    }
}

std::vector<EndpointCandidate> collectEndpoints(
    const Mesh& mesh, const TraceGraph& trace, const std::vector<double>& localScale, double fallbackScale
) {
    std::vector<EndpointCandidate> endpoints;
    for (int id = 0; id < static_cast<int>(trace.adjacency.size()); ++id) {
        if (trace.adjacency[id].size() != 1 || id >= static_cast<int>(mesh.vertices.size())) {
            continue;
        }
        const int nb = trace.adjacency[id].front();
        if (nb < 0 || nb >= static_cast<int>(mesh.vertices.size())) {
            continue;
        }
        Vec3 outward = mesh.vertices[id] - mesh.vertices[nb];
        if (outward.norm() <= 1e-20) {
            continue;
        }
        outward.normalize();
        endpoints.push_back(EndpointCandidate{id, nb, outward, vertexScale(localScale, id, fallbackScale)});
    }
    return endpoints;
}

bool endpointGapDirectionsCompatible(const EndpointCandidate& a, const EndpointCandidate& b, const Mesh& mesh) {
    Vec3 direction = mesh.vertices[b.vertex] - mesh.vertices[a.vertex];
    if (direction.norm() <= 1e-20) {
        return false;
    }
    direction.normalize();
    const double alignA = a.outward.dot(direction);
    const double alignB = b.outward.dot(-direction);
    return std::min(alignA, alignB) >= -0.15;
}

void bridgeEndpointGaps(const Mesh& mesh, const FeatureOptions& options, TraceGraph& trace, FeatureAnalysis& analysis) {
    if (options.featureGraphGapLengthRatio <= 0.0) {
        return;
    }

    const std::vector<double> localScale = manumesh::detail::computeVertexAverageEdgeLength(mesh);
    const double fallbackScale = meanPositiveScale(localScale, std::max(1e-12, mesh.bboxDiag() * 1e-3));
    const std::vector<EndpointCandidate> endpoints = collectEndpoints(mesh, trace, localScale, fallbackScale);
    constexpr int kMaxEndpointPairs = 512;
    if (endpoints.size() < 2 || endpoints.size() > kMaxEndpointPairs) {
        return;
    }

    std::vector<GapCandidate> candidates;
    for (int i = 0; i < static_cast<int>(endpoints.size()); ++i) {
        for (int j = i + 1; j < static_cast<int>(endpoints.size()); ++j) {
            const EndpointCandidate& a = endpoints[i];
            const EndpointCandidate& b = endpoints[j];
            if (a.vertex == b.vertex || a.neighbor == b.vertex || b.neighbor == a.vertex ||
                traceGraphHasEdge(trace, a.vertex, b.vertex)) {
                continue;
            }
            const double distance = (mesh.vertices[a.vertex] - mesh.vertices[b.vertex]).norm();
            const double allowed = options.featureGraphGapLengthRatio * 0.5 * (a.scale + b.scale);
            if (distance > allowed || !endpointGapDirectionsCompatible(a, b, mesh)) {
                continue;
            }
            candidates.push_back(GapCandidate{a.vertex, b.vertex, distance});
        }
    }

    std::sort(candidates.begin(), candidates.end(), [](const GapCandidate& lhs, const GapCandidate& rhs) {
        return lhs.distance < rhs.distance;
    });
    std::unordered_set<int> used;
    for (const GapCandidate& candidate : candidates) {
        if (used.find(candidate.a) != used.end() || used.find(candidate.b) != used.end()) {
            continue;
        }
        CandidateEdge bridge;
        bridge.a = candidate.a;
        bridge.b = candidate.b;
        bridge.cleanupBridge = true;
        addTraceGraphEdge(trace, analysis, bridge);
        used.insert(candidate.a);
        used.insert(candidate.b);
        ++analysis.graphCleanupBridgedGaps;
    }
    if (!used.empty()) {
        rebuildTraceGraphEdges(trace);
    }
}

void bridgeCloseJunctions(
    const Mesh& mesh, const FeatureOptions& options, TraceGraph& trace, FeatureAnalysis& analysis
) {
    if (options.featureGraphGapLengthRatio <= 0.0) {
        return;
    }
    const std::vector<double> localScale = manumesh::detail::computeVertexAverageEdgeLength(mesh);
    const double fallbackScale = meanPositiveScale(localScale, std::max(1e-12, mesh.bboxDiag() * 1e-3));

    std::vector<int> junctions;
    for (int id = 0; id < static_cast<int>(trace.adjacency.size()); ++id) {
        if (id < static_cast<int>(mesh.vertices.size()) && trace.adjacency[id].size() > 2) {
            junctions.push_back(id);
        }
    }
    constexpr int kMaxJunctionPairs = 256;
    if (junctions.size() < 2 || junctions.size() > kMaxJunctionPairs) {
        return;
    }

    std::vector<GapCandidate> candidates;
    for (int i = 0; i < static_cast<int>(junctions.size()); ++i) {
        for (int j = i + 1; j < static_cast<int>(junctions.size()); ++j) {
            const int a = junctions[i];
            const int b = junctions[j];
            if (traceGraphHasEdge(trace, a, b)) {
                continue;
            }
            const double distance = (mesh.vertices[a] - mesh.vertices[b]).norm();
            const double allowed =
                0.5 * options.featureGraphGapLengthRatio *
                (vertexScale(localScale, a, fallbackScale) + vertexScale(localScale, b, fallbackScale));
            if (distance <= allowed) {
                candidates.push_back(GapCandidate{a, b, distance});
            }
        }
    }

    std::sort(candidates.begin(), candidates.end(), [](const GapCandidate& lhs, const GapCandidate& rhs) {
        return lhs.distance < rhs.distance;
    });
    std::unordered_set<int> used;
    for (const GapCandidate& candidate : candidates) {
        if (used.find(candidate.a) != used.end() || used.find(candidate.b) != used.end()) {
            continue;
        }
        CandidateEdge bridge;
        bridge.a = candidate.a;
        bridge.b = candidate.b;
        bridge.cleanupBridge = true;
        addTraceGraphEdge(trace, analysis, bridge);
        used.insert(candidate.a);
        used.insert(candidate.b);
        ++analysis.graphCleanupMergedJunctions;
    }
    if (!used.empty()) {
        rebuildTraceGraphEdges(trace);
    }
}

double primitiveResidual(const Mesh& mesh, const FeatureLoop& loop) {
    const double diag = std::max(1e-12, mesh.bboxDiag());
    switch (loop.primitive) {
    case FeaturePrimitiveType::Circle:
    case FeaturePrimitiveType::NearCircle: {
        const double scale = std::max({loop.radius, diag * 1e-9, 1e-12});
        return (loop.rmsRadialError + loop.rmsPlaneError) / scale;
    }
    case FeaturePrimitiveType::Ellipse: {
        const double scale = std::max({loop.majorRadius, loop.minorRadius, diag * 1e-9, 1e-12});
        return (loop.rmsEllipseError + loop.rmsPlaneError) / scale;
    }
    case FeaturePrimitiveType::PolygonalLoop:
        return loop.rmsPlaneError / diag;
    case FeaturePrimitiveType::Unknown:
        return 0.0;
    }
    return 0.0;
}

double computeClosureRate(int endpointCount, int cycleRank) {
    if (endpointCount <= 0) {
        return 1.0;
    }
    const double endpointPenalty = std::clamp(1.0 - 0.25 * endpointCount, 0.0, 1.0);
    return cycleRank > 0 ? endpointPenalty : 0.5 * endpointPenalty;
}

double computeConfidence(const FeatureComponent& component, const FeatureOptions& options, bool hasPrimitiveResidual) {
    if (component.edgeCount <= 0) {
        return 0.0;
    }
    const double tensorScore =
        std::clamp(component.meanTensorPersistence / std::max(1e-12, options.normalTensorFeatureThreshold), 0.0, 1.0);
    const double evidenceScore = std::max(component.strongEvidenceRatio, 0.75 * tensorScore);
    const double residualScore =
        hasPrimitiveResidual ? std::clamp(1.0 - component.meanPrimitiveResidual / 0.12, 0.0, 1.0) : 0.5;
    const double junctionPenalty = component.junctionVertices > 2
                                       ? std::min(0.25, 0.04 * static_cast<double>(component.junctionVertices - 2))
                                       : 0.0;
    return std::clamp(
        0.45 * evidenceScore + 0.25 * component.closureRate + 0.20 * residualScore + 0.10 * tensorScore -
            junctionPenalty,
        0.0,
        1.0
    );
}

int dominantComponentForLoop(const FeatureLoop& loop, const std::vector<int>& vertexToComponent, int componentCount) {
    if (componentCount <= 0) {
        return -1;
    }
    std::vector<int> votes(componentCount, 0);
    for (int id : loop.vertices) {
        if (id >= 0 && id < static_cast<int>(vertexToComponent.size())) {
            const int component = vertexToComponent[id];
            if (component >= 0 && component < componentCount) {
                ++votes[component];
            }
        }
    }
    int bestComponent = -1;
    int bestVotes = 0;
    for (int i = 0; i < componentCount; ++i) {
        if (votes[i] > bestVotes) {
            bestVotes = votes[i];
            bestComponent = i;
        }
    }
    return bestComponent;
}

} // namespace

void cleanupTraceGraph(const Mesh& mesh, const FeatureOptions& options, TraceGraph& trace, FeatureAnalysis& analysis) {
    if (!options.cleanupFeatureGraph || trace.graphEdges.empty()) {
        return;
    }
    removeWeakSpurs(options, trace, analysis);
    bridgeCloseJunctions(mesh, options, trace, analysis);
    bridgeEndpointGaps(mesh, options, trace, analysis);
}

void summarizeFeatureComponents(
    const Mesh& mesh, const FeatureOptions& options, const TraceGraph& trace, FeatureAnalysis& analysis
) {
    analysis.components.clear();
    std::vector<int> vertexToComponent(trace.adjacency.size(), -1);
    std::vector<char> visited(trace.adjacency.size(), 0);

    for (int seed = 0; seed < static_cast<int>(trace.adjacency.size()); ++seed) {
        if (visited[seed] || trace.adjacency[seed].empty()) {
            continue;
        }

        FeatureComponent component;
        component.id = static_cast<int>(analysis.components.size());
        std::queue<int> queue;
        queue.push(seed);
        visited[seed] = 1;
        while (!queue.empty()) {
            const int v = queue.front();
            queue.pop();
            vertexToComponent[v] = component.id;
            component.vertices.push_back(v);
            if (trace.adjacency[v].size() == 1) {
                ++component.endpointVertices;
            }
            if (trace.adjacency[v].size() != 2) {
                ++component.junctionVertices;
            }
            for (int nb : trace.adjacency[v]) {
                if (v < nb) {
                    ++component.edgeCount;
                    if (traceEdgeBoundary(trace, v, nb))
                        ++component.boundaryEdges;
                    if (traceEdgeDihedral(trace, v, nb))
                        ++component.dihedralEdges;
                    if (traceEdgeNormalTensor(trace, v, nb))
                        ++component.normalTensorEdges;
                    if (traceEdgeNonManifold(trace, v, nb))
                        ++component.nonManifoldEdges;
                    if (traceEdgeCleanupBridge(trace, v, nb))
                        ++component.cleanupBridgeEdges;
                }
                if (!visited[nb]) {
                    visited[nb] = 1;
                    queue.push(nb);
                }
            }
        }

        double tensorPersistenceSum = 0.0;
        for (int v : component.vertices) {
            for (int nb : trace.adjacency[v]) {
                if (v < nb && traceEdgeNormalTensor(trace, v, nb)) {
                    tensorPersistenceSum += traceEdgeTensorPersistence(trace, v, nb);
                }
            }
        }
        component.strongEvidenceEdges = component.boundaryEdges + component.dihedralEdges + component.nonManifoldEdges;
        component.weakEvidenceEdges = component.normalTensorEdges + component.cleanupBridgeEdges;
        component.cycleRank = component.edgeCount - static_cast<int>(component.vertices.size()) + 1;
        component.closed = component.endpointVertices == 0 && component.edgeCount > 0 && component.cycleRank >= 0;
        component.closureRate = computeClosureRate(component.endpointVertices, component.cycleRank);
        component.strongEvidenceRatio = component.edgeCount > 0 ? static_cast<double>(component.strongEvidenceEdges) /
                                                                      static_cast<double>(component.edgeCount)
                                                                : 0.0;
        component.meanTensorPersistence = component.normalTensorEdges > 0
                                              ? tensorPersistenceSum / static_cast<double>(component.normalTensorEdges)
                                              : 0.0;
        analysis.components.push_back(std::move(component));
    }

    std::vector<double> residualSums(analysis.components.size(), 0.0);
    std::vector<int> residualCounts(analysis.components.size(), 0);
    for (FeatureLoop& loop : analysis.loops) {
        const int componentId =
            dominantComponentForLoop(loop, vertexToComponent, static_cast<int>(analysis.components.size()));
        loop.componentId = componentId;
        loop.primitiveResidual = primitiveResidual(mesh, loop);
        if (componentId >= 0) {
            residualSums[componentId] += loop.primitiveResidual;
            ++residualCounts[componentId];
        }
    }

    double confidenceSum = 0.0;
    double minConfidence = std::numeric_limits<double>::infinity();
    analysis.weakFeatureComponents = 0;
    analysis.highConfidenceFeatureComponents = 0;
    for (FeatureComponent& component : analysis.components) {
        const bool hasPrimitiveResidual = residualCounts[component.id] > 0;
        component.meanPrimitiveResidual =
            hasPrimitiveResidual ? residualSums[component.id] / static_cast<double>(residualCounts[component.id]) : 0.0;
        component.confidence = computeConfidence(component, options, hasPrimitiveResidual);
        if (component.weakEvidenceEdges > component.strongEvidenceEdges) {
            ++analysis.weakFeatureComponents;
        }
        if (component.confidence >= options.featureComponentMinConfidence) {
            ++analysis.highConfidenceFeatureComponents;
        }
        confidenceSum += component.confidence;
        minConfidence = std::min(minConfidence, component.confidence);
    }

    if (!analysis.components.empty()) {
        analysis.meanFeatureComponentConfidence = confidenceSum / static_cast<double>(analysis.components.size());
        analysis.minFeatureComponentConfidence = minConfidence;
    } else {
        analysis.meanFeatureComponentConfidence = 0.0;
        analysis.minFeatureComponentConfidence = 0.0;
    }

    for (FeatureLoop& loop : analysis.loops) {
        if (loop.componentId >= 0 && loop.componentId < static_cast<int>(analysis.components.size())) {
            const FeatureComponent& component = analysis.components[loop.componentId];
            loop.componentConfidence = component.confidence;
            loop.weakFeature = component.weakEvidenceEdges > component.strongEvidenceEdges;
        }
    }

    for (int id = 0; id < static_cast<int>(analysis.vertices.size()) && id < static_cast<int>(vertexToComponent.size());
         ++id) {
        const int componentId = vertexToComponent[id];
        if (componentId < 0 || componentId >= static_cast<int>(analysis.components.size())) {
            continue;
        }
        VertexFeature& vertex = analysis.vertices[id];
        if (!vertex.isFeature) {
            continue;
        }
        const FeatureComponent& component = analysis.components[componentId];
        vertex.componentId = componentId;
        vertex.confidence = component.confidence;
        vertex.weakFeature = component.weakEvidenceEdges > component.strongEvidenceEdges;
    }
}

} // namespace manumesh::feature::detector_detail
