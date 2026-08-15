/**
 * @file src/feature_detection/FeatureTraceRecovery.cpp
 * @brief 实现 ManuMesh 的特征检测模块的特征轨迹恢复功能。
 * @ingroup manumesh_feature_detection
 *
 * @details 在更强证据恢复阶段之后，追踪剩余的二度顶点链和环。
 * @algorithm 遍历已排序的邻接表并标记已访问的无向边；先由端点生成开放链，
 *            再消耗尚未访问的二度顶点环。
 * @invariants 本阶段每条活动图边最多被追踪一次。
 */

#include "detail/FeatureTraceRecovery.h"

#include "common/detail/MeshQueries.h"
#include "detail/FeatureGraph.h"
#include "detail/FeatureLoopBuilder.h"

#include <unordered_set>

namespace manumesh {
namespace feature {
namespace detector_detail {
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

} // 匿名命名空间

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

    for (const auto& pairEntry : trace.graphEdges) {
        const auto& a = pairEntry.first;
        const auto& b = pairEntry.second;
        if (traceEdgeVisited(visitedEdges, a, b)) {
            continue;
        }
        TraceLoopStats stats;
        std::vector<int> vertices = traceClosedLoop(trace, a, b, visitedEdges, stats);
        addTracedLoop(mesh, options, trace.adjacency, std::move(vertices), stats, analysis, loopId);
    }
}

} // namespace detector_detail
} // namespace feature
} // namespace manumesh
