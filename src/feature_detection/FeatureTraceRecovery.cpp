/**
 * @file src/feature_detection/FeatureTraceRecovery.cpp
 * @brief Implements feature trace recovery facilities for ManuMesh's feature-detection module.
 * @ingroup manumesh_feature_detection
 *
 * @details Traces remaining degree-two chains and loops after stronger recovery stages.
 * @algorithm Walks sorted adjacency while marking undirected edges visited;
 * endpoints seed open chains before unvisited degree-two cycles are consumed.
 * @invariants Every active graph edge is traced at most once by this stage.
 */

#include "detail/FeatureTraceRecovery.h"

#include "common/detail/MeshQueries.h"
#include "detail/FeatureGraph.h"
#include "detail/FeatureLoopBuilder.h"

#include <unordered_set>

namespace manumesh::feature::detector_detail {
namespace {

bool traceEdgeVisited(const std::unordered_set<std::uint64_t>& visitedEdges, int a, int b) {
    return visitedEdges.find(manumesh::common::meshEdgeKey(a, b)) != visitedEdges.end();
}

void markTraceEdge(std::unordered_set<std::uint64_t>& visitedEdges, int a, int b) {
    visitedEdges.insert(manumesh::common::meshEdgeKey(a, b));
}

void accumulateTraceEdgeStats(const TraceGraph& trace, int a, int b, TraceLoopStats& stats) {
    ++stats.edgeCount;
    const TraceEdgeAttrs* attrs = traceEdgeAttrs(trace, a, b);
    if (attrs == nullptr) {
        ++stats.unknownSignedEdges;
        return;
    }
    if (attrs->boundary) {
        ++stats.boundaryEdges;
    }
    if (attrs->dihedral) {
        ++stats.dihedralEdges;
    }
    if (attrs->normalTensor) {
        ++stats.normalTensorEdges;
    }
    if (attrs->smoothCurvature) {
        ++stats.smoothCurvatureEdges;
    }
    if (attrs->nonManifold) {
        ++stats.nonManifoldEdges;
    }
    if (attrs->cleanupBridge) {
        ++stats.cleanupBridgeEdges;
    }
    if (attrs->signedKind > 0)
        ++stats.convexEdges;
    if (attrs->signedKind < 0)
        ++stats.concaveEdges;
    if (attrs->signedKind == 0 && !attrs->boundary) {
        ++stats.unknownSignedEdges;
    }
}

std::vector<int> traceOpenChain(
    const TraceGraph& trace,
    int seed,
    int firstNeighbor,
    std::unordered_set<std::uint64_t>& visitedEdges,
    TraceLoopStats& stats
) {
    std::vector<int> vertices;
    vertices.push_back(seed);

    int previous = seed;
    int current = firstNeighbor;
    while (true) {
        if (traceEdgeVisited(visitedEdges, previous, current)) {
            break;
        }
        markTraceEdge(visitedEdges, previous, current);
        accumulateTraceEdgeStats(trace, previous, current, stats);
        vertices.push_back(current);
        if (current == seed) {
            vertices.pop_back();
            stats.closed = true;
            break;
        }

        if (trace.adjacency[current].size() != 2) {
            break;
        }

        int next = -1;
        for (int candidate : trace.adjacency[current]) {
            if (candidate != previous && !traceEdgeVisited(visitedEdges, current, candidate)) {
                next = candidate;
                break;
            }
        }
        if (next < 0) {
            break;
        }
        previous = current;
        current = next;
    }

    return vertices;
}

std::vector<int> traceClosedLoop(
    const TraceGraph& trace,
    int seed,
    int firstNeighbor,
    std::unordered_set<std::uint64_t>& visitedEdges,
    TraceLoopStats& stats
) {
    std::vector<int> vertices;
    vertices.push_back(seed);

    int previous = seed;
    int current = firstNeighbor;
    while (true) {
        if (traceEdgeVisited(visitedEdges, previous, current)) {
            break;
        }
        markTraceEdge(visitedEdges, previous, current);
        accumulateTraceEdgeStats(trace, previous, current, stats);
        if (current == seed) {
            stats.closed = true;
            break;
        }
        vertices.push_back(current);

        int next = -1;
        for (int candidate : trace.adjacency[current]) {
            if (candidate != previous) {
                next = candidate;
                break;
            }
        }
        if (next < 0) {
            break;
        }
        previous = current;
        current = next;
    }

    return vertices;
}

} // namespace

void traceRemainingFeatureLoops(
    const Mesh& mesh, const FeatureOptions& options, const TraceGraph& trace, FeatureAnalysis& analysis, int& loopId
) {
    std::unordered_set<std::uint64_t> visitedEdges;
    visitedEdges.reserve(trace.graphEdges.size());

    for (int seed = 0; seed < static_cast<int>(trace.adjacency.size()); ++seed) {
        if (trace.adjacency[seed].empty() || trace.adjacency[seed].size() == 2) {
            continue;
        }
        for (int nb : trace.adjacency[seed]) {
            if (traceEdgeVisited(visitedEdges, seed, nb)) {
                continue;
            }
            TraceLoopStats stats;
            std::vector<int> vertices = traceOpenChain(trace, seed, nb, visitedEdges, stats);
            addTracedLoop(mesh, options, trace.adjacency, std::move(vertices), stats, analysis, loopId);
        }
    }

    for (const auto& [a, b] : trace.graphEdges) {
        if (traceEdgeVisited(visitedEdges, a, b)) {
            continue;
        }
        TraceLoopStats stats;
        std::vector<int> vertices = traceClosedLoop(trace, a, b, visitedEdges, stats);
        addTracedLoop(mesh, options, trace.adjacency, std::move(vertices), stats, analysis, loopId);
    }
}

} // namespace manumesh::feature::detector_detail
