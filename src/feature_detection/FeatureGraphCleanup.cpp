/**
 * @file src/feature_detection/FeatureGraphCleanup.cpp
 * @brief Implements feature graph cleanup facilities for ManuMesh's feature-detection module.
 * @ingroup manumesh_feature_detection
 *
 * @details Removes weak graph noise and bridges short compatible local gaps.
 * @algorithm Cleanup prunes dangling weak-only chains by edge count or
 * integrated scale-normalized strength, then joins nearby endpoints only when
 * distance, continuation alignment, evidence kind, and signed kind agree.
 * @failuremodes A work cap bounds dense-graph recovery; skipped work is
 * surfaced in analysis diagnostics instead of causing unbounded runtime.
 */

#include "detail/FeatureGraphCleanup.h"

#include "common/detail/MeshQueries.h"
#include "detail/FeatureGraph.h"
#include "detail/FeatureGraphCompatibility.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <queue>
#include <unordered_map>
#include <unordered_set>

namespace manumesh::feature::detector_detail {
namespace {

/** @brief Open graph endpoint eligible for cleanup or gap bridging. */
struct EndpointCandidate {
    int vertex = -1;
    int neighbor = -1;
    Vec3 outward = Vec3(1.0, 0.0, 0.0);
    double scale = 0.0;
};

/** @brief Ranked pair of endpoints proposed for a cleanup bridge. */
struct GapCandidate {
    int a = -1;
    int b = -1;
    double distance = 0.0;
    /**
     * @brief Direction-aware ranking key: distance divided by the mean tangential
     * alignment of the two endpoint tangents with the connecting segment.
     */
    double score = 0.0;
    int signedKind = 0;
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
    const TraceEdgeAttrs* attrs = traceEdgeAttrs(trace, a, b);
    if (attrs == nullptr) {
        return false;
    }
    return (attrs->normalTensor || attrs->smoothCurvature) && !attrs->boundary && !attrs->dihedral &&
           !attrs->nonManifold && !attrs->cleanupBridge;
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

/**
 * @brief Dimensionless Yoshizawa-style curve strength T = (integral ds) * (integral
 * strength ds), with ds measured in local average-edge-length units and the
 * per-edge strength taken as the persistence score relative to its channel
 * threshold. Long-but-faint chains score high through the length factor while
 * short-but-strong noise spikes stay low, matching the "long weak lines beat
 * strong short spurs" design target (M021 Eq.5-6).
 */
double weakSpurStrength(
    const std::vector<std::pair<int, int>>& path,
    const Mesh& mesh,
    const FeatureOptions& options,
    const TraceGraph& trace,
    const std::vector<double>& localScale,
    double fallbackScale
) {
    const double tensorThreshold = std::max(1e-12, options.normalTensorFeatureThreshold);
    const double curvatureThreshold = std::max(1e-12, options.smoothCurvatureFeatureThreshold);
    double lengthSum = 0.0;
    double strengthSum = 0.0;
    for (const auto& [a, b] : path) {
        if (a < 0 || b < 0 || a >= static_cast<int>(mesh.vertices.size()) ||
            b >= static_cast<int>(mesh.vertices.size())) {
            continue;
        }
        const double scale = std::max(
            1e-12, 0.5 * (vertexScale(localScale, a, fallbackScale) + vertexScale(localScale, b, fallbackScale))
        );
        const double lengthNorm = (mesh.vertices[a] - mesh.vertices[b]).norm() / scale;
        const TraceEdgeAttrs* attrs = traceEdgeAttrs(trace, a, b);
        double strength = 0.0;
        if (attrs != nullptr) {
            strength =
                std::max(attrs->tensorPersistence / tensorThreshold, attrs->curvaturePersistence / curvatureThreshold);
        }
        lengthSum += lengthNorm;
        strengthSum += strength * lengthNorm;
    }
    return lengthSum * strengthSum;
}

void removeWeakSpurs(
    const Mesh& mesh,
    const FeatureOptions& options,
    FeatureDetectionCache& cache,
    TraceGraph& trace,
    FeatureAnalysis& analysis
) {
    const int maxEdges = std::max(0, options.featureGraphMaxWeakSpurEdges);
    if (maxEdges <= 0) {
        return;
    }
    const bool useStrength =
        std::isfinite(options.featureGraphMinWeakSpurStrength) && options.featureGraphMinWeakSpurStrength > 0.0;
    // With strength filtering enabled, spurs longer than the legacy edge cap
    // are still examined (up to a fixed horizon) so that medium-length noise
    // chains can be pruned, while any spur whose integrated strength clears
    // the threshold is kept even when it is short.
    constexpr int kStrengthTraceEdgeCap = 64;
    const int traceCap = useStrength ? std::max(maxEdges, kStrengthTraceEdgeCap) : maxEdges;

    const std::vector<double>* localScale = nullptr;
    double fallbackScale = 0.0;
    if (useStrength) {
        localScale = &cache.vertexAverageEdgeLength();
        fallbackScale = meanPositiveScale(*localScale, std::max(1e-12, mesh.bboxDiag() * 1e-3));
    }

    std::unordered_map<std::uint64_t, int> graphEdgeIndex;
    graphEdgeIndex.reserve(analysis.graph.edges.size());
    for (int edgeId = 0; edgeId < static_cast<int>(analysis.graph.edges.size()); ++edgeId) {
        const FeatureGraphEdge& edge = analysis.graph.edges[edgeId];
        graphEdgeIndex[manumesh::common::meshEdgeKey(edge.a, edge.b)] = edgeId;
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
            const std::vector<std::pair<int, int>> spur = traceShortWeakSpur(trace, seed, traceCap);
            if (spur.empty()) {
                continue;
            }
            if (useStrength) {
                if (weakSpurStrength(spur, mesh, options, trace, *localScale, fallbackScale) >=
                    options.featureGraphMinWeakSpurStrength) {
                    continue;
                }
            } else if (static_cast<int>(spur.size()) > maxEdges) {
                continue;
            }
            for (const auto& edge : spur) {
                const std::uint64_t key = manumesh::common::meshEdgeKey(edge.first, edge.second);
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
            const auto graphEdge = graphEdgeIndex.find(manumesh::common::meshEdgeKey(a, b));
            if (graphEdge != graphEdgeIndex.end()) {
                analysis.graph.edges[graphEdge->second].removedByCleanup = true;
            }
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

/** @brief Tangent-alignment measurements for a proposed endpoint gap. */
struct GapAlignment {
    bool compatible = false;
    double meanAlignment = 0.0;
};

/**
 * @brief Yoshizawa gap-jumping angle rule (M021 p.3, Fig.4): the connecting segment
 * must continue both chain tangents (each within 60 degrees) and the two
 * outward tangents must point away from each other, so only breaks along one
 * underlying curve are bridged and parallel chains are never merged into each other.
 */
GapAlignment endpointGapAlignment(const EndpointCandidate& a, const EndpointCandidate& b, const Mesh& mesh) {
    GapAlignment result;
    Vec3 direction = mesh.vertices[b.vertex] - mesh.vertices[a.vertex];
    if (direction.norm() <= 1e-20) {
        return result;
    }
    direction.normalize();
    const double alignA = a.outward.dot(direction);
    const double alignB = b.outward.dot(-direction);
    result.compatible = std::min(alignA, alignB) >= 0.5 && a.outward.dot(b.outward) <= 0.0;
    result.meanAlignment = 0.5 * (alignA + alignB);
    return result;
}

void bridgeEndpointGaps(
    const Mesh& mesh,
    const FeatureOptions& options,
    FeatureDetectionCache& cache,
    TraceGraph& trace,
    FeatureAnalysis& analysis
) {
    if (options.featureGraphGapLengthRatio <= 0.0) {
        return;
    }

    const std::vector<double>& localScale = cache.vertexAverageEdgeLength();
    const double fallbackScale = meanPositiveScale(localScale, std::max(1e-12, mesh.bboxDiag() * 1e-3));
    const std::vector<EndpointCandidate> endpoints = collectEndpoints(mesh, trace, localScale, fallbackScale);
    constexpr int kMaxGapEndpoints = 512;
    if (endpoints.size() < 2) {
        return;
    }
    if (static_cast<int>(endpoints.size()) > kMaxGapEndpoints) {
        ++analysis.graphCleanupSkippedByCap;
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
            if (distance > allowed) {
                continue;
            }
            const GapAlignment alignment = endpointGapAlignment(a, b, mesh);
            if (!alignment.compatible) {
                continue;
            }
            const TraceEdgeAttrs* aAttrs = traceEdgeAttrs(trace, a.vertex, a.neighbor);
            const TraceEdgeAttrs* bAttrs = traceEdgeAttrs(trace, b.vertex, b.neighbor);
            if (!compatibleFeatureEvidence(aAttrs, bAttrs)) {
                continue;
            }
            const double score = distance / std::max(0.5, alignment.meanAlignment);
            candidates.push_back(
                GapCandidate{a.vertex, b.vertex, distance, score, compatibleSignedKind(aAttrs, bAttrs)}
            );
        }
    }

    std::sort(candidates.begin(), candidates.end(), [](const GapCandidate& lhs, const GapCandidate& rhs) {
        if (lhs.score != rhs.score) {
            return lhs.score < rhs.score;
        }
        if (lhs.distance != rhs.distance) {
            return lhs.distance < rhs.distance;
        }
        return lhs.a != rhs.a ? lhs.a < rhs.a : lhs.b < rhs.b;
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
        bridge.signedKind = candidate.signedKind;
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
    const Mesh& mesh,
    const FeatureOptions& options,
    FeatureDetectionCache& cache,
    TraceGraph& trace,
    FeatureAnalysis& analysis
) {
    if (options.featureGraphGapLengthRatio <= 0.0) {
        return;
    }
    const std::vector<double>& localScale = cache.vertexAverageEdgeLength();
    const double fallbackScale = meanPositiveScale(localScale, std::max(1e-12, mesh.bboxDiag() * 1e-3));

    std::vector<int> junctions;
    for (int id = 0; id < static_cast<int>(trace.adjacency.size()); ++id) {
        if (id < static_cast<int>(mesh.vertices.size()) && trace.adjacency[id].size() > 2) {
            junctions.push_back(id);
        }
    }
    constexpr int kMaxGapJunctions = 256;
    if (junctions.size() < 2) {
        return;
    }
    if (static_cast<int>(junctions.size()) > kMaxGapJunctions) {
        ++analysis.graphCleanupSkippedByCap;
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
                const ContinuationBranch aBranch = bestContinuationBranch(mesh, trace, a, b, 0.35);
                const ContinuationBranch bBranch = bestContinuationBranch(mesh, trace, b, a, 0.35);
                if (aBranch.neighbor < 0 || bBranch.neighbor < 0 ||
                    !compatibleFeatureEvidence(aBranch.attrs, bBranch.attrs)) {
                    continue;
                }
                const double alignmentPenalty = 0.5 * (2.0 - aBranch.alignment - bBranch.alignment);
                candidates.push_back(
                    GapCandidate{
                        a,
                        b,
                        distance,
                        distance + allowed * alignmentPenalty,
                        compatibleSignedKind(aBranch.attrs, bBranch.attrs),
                    }
                );
            }
        }
    }

    std::sort(candidates.begin(), candidates.end(), [](const GapCandidate& lhs, const GapCandidate& rhs) {
        if (lhs.score != rhs.score) {
            return lhs.score < rhs.score;
        }
        if (lhs.distance != rhs.distance) {
            return lhs.distance < rhs.distance;
        }
        return lhs.a != rhs.a ? lhs.a < rhs.a : lhs.b < rhs.b;
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
        bridge.signedKind = candidate.signedKind;
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
    const double curvatureScore = std::clamp(
        component.meanCurvaturePersistence / std::max(1e-12, options.smoothCurvatureFeatureThreshold), 0.0, 1.0
    );
    const double weakSupportScore = std::max(tensorScore, curvatureScore);
    const double evidenceScore = std::max(component.strongEvidenceRatio, 0.80 * weakSupportScore);
    const double residualScore =
        hasPrimitiveResidual ? std::clamp(1.0 - component.meanPrimitiveResidual / 0.12, 0.0, 1.0) : 0.5;
    const double junctionPenalty = component.junctionVertices > 2
                                       ? std::min(0.25, 0.04 * static_cast<double>(component.junctionVertices - 2))
                                       : 0.0;
    return std::clamp(
        0.45 * evidenceScore + 0.25 * component.closureRate + 0.20 * residualScore + 0.10 * weakSupportScore -
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

void cleanupTraceGraph(
    const Mesh& mesh,
    const FeatureOptions& options,
    FeatureDetectionCache& cache,
    TraceGraph& trace,
    FeatureAnalysis& analysis
) {
    if (!options.cleanupFeatureGraph || trace.graphEdges.empty()) {
        return;
    }
    removeWeakSpurs(mesh, options, cache, trace, analysis);
    bridgeCloseJunctions(mesh, options, cache, trace, analysis);
    bridgeEndpointGaps(mesh, options, cache, trace, analysis);
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
        double tensorPersistenceSum = 0.0;
        double curvaturePersistenceSum = 0.0;
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
            if (trace.adjacency[v].size() > 2) {
                ++component.junctionVertices;
            }
            for (int nb : trace.adjacency[v]) {
                if (v < nb) {
                    ++component.edgeCount;
                    const TraceEdgeAttrs* attrs = traceEdgeAttrs(trace, v, nb);
                    if (attrs != nullptr) {
                        if (attrs->boundary)
                            ++component.boundaryEdges;
                        if (attrs->dihedral)
                            ++component.dihedralEdges;
                        if (attrs->normalTensor) {
                            ++component.normalTensorEdges;
                            tensorPersistenceSum += attrs->tensorPersistence;
                        }
                        if (attrs->smoothCurvature) {
                            ++component.smoothCurvatureEdges;
                            curvaturePersistenceSum += attrs->curvaturePersistence;
                        }
                        if (attrs->nonManifold)
                            ++component.nonManifoldEdges;
                        if (attrs->cleanupBridge)
                            ++component.cleanupBridgeEdges;
                        if (attrs->consolidationBridge)
                            ++component.consolidationBridgeEdges;
                    }
                }
                if (!visited[nb]) {
                    visited[nb] = 1;
                    queue.push(nb);
                }
            }
        }

        component.strongEvidenceEdges = component.boundaryEdges + component.dihedralEdges + component.nonManifoldEdges;
        component.weakEvidenceEdges = component.normalTensorEdges + component.smoothCurvatureEdges +
                                      component.cleanupBridgeEdges + component.consolidationBridgeEdges;
        component.cycleRank = component.edgeCount - static_cast<int>(component.vertices.size()) + 1;
        component.closed = component.endpointVertices == 0 && component.edgeCount > 0 && component.cycleRank >= 0;
        component.closureRate = computeClosureRate(component.endpointVertices, component.cycleRank);
        component.strongEvidenceRatio = component.edgeCount > 0 ? static_cast<double>(component.strongEvidenceEdges) /
                                                                      static_cast<double>(component.edgeCount)
                                                                : 0.0;
        component.meanTensorPersistence = component.normalTensorEdges > 0
                                              ? tensorPersistenceSum / static_cast<double>(component.normalTensorEdges)
                                              : 0.0;
        component.meanCurvaturePersistence =
            component.smoothCurvatureEdges > 0
                ? curvaturePersistenceSum / static_cast<double>(component.smoothCurvatureEdges)
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
