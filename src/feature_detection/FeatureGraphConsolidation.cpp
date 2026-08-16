/**
 * @file src/feature_detection/FeatureGraphConsolidation.cpp
 * @brief 在局部清理后桥接跨连通分量的兼容弱特征间隙。
 * @ingroup manumesh_feature_detection
 *
 * @details 在局部清理后执行分量级弱特征恢复。
 * @algorithm 从不同且兼容的分量中索引端点位置，并依据局部尺度距离、切线对齐度、
 *            证据类别及凸凹符号配对；接受的桥会补充图支持。
 * @failuremodes 对含大量端点的歧义密集图设置恢复上限。
 */

#include "detail/FeatureGraphConsolidation.h"

#include "detail/FeatureGraph.h"
#include "detail/FeatureGraphCompatibility.h"

#include <algorithm>
#include <queue>
#include <unordered_set>
#include <vector>

namespace manumesh {
namespace feature {
namespace detector_detail {
namespace {

/** @brief 带有位置局部尺度的合并端点。 */
struct Endpoint {
    int vertex = -1;
    int component = -1;
    double scale = 0.0;
};

/** @brief 待合并的兼容分量桥接候选及其排序分数。 */
struct ConsolidationCandidate {
    int first = -1;
    int second = -1;
    int signedKind = 0;
    double distance = 0.0;
    double score = 0.0;
};

std::vector<int> componentLabels(const TraceGraph& trace) {
    std::vector<int> labels(trace.adjacency.size(), -1);
    int nextComponent = 0;
    for (int seed = 0; seed < static_cast<int>(trace.adjacency.size()); ++seed) {
        if (labels[seed] >= 0 || trace.adjacency[seed].empty()) {
            continue;
        }
        std::queue<int> queue;
        queue.push(seed);
        labels[seed] = nextComponent;
        while (!queue.empty()) {
            const int vertex = queue.front();
            queue.pop();
            for (int neighbor : trace.adjacency[vertex]) {
                if (labels[neighbor] < 0) {
                    labels[neighbor] = nextComponent;
                    queue.push(neighbor);
                }
            }
        }
        ++nextComponent;
    }
    return labels;
}

double fallbackScale(const std::vector<double>& scales, double fallback) {
    double sum = 0.0;
    int count = 0;
    for (double scale : scales) {
        if (scale > 0.0) {
            sum += scale;
            ++count;
        }
    }
    return count > 0 ? sum / static_cast<double>(count) : fallback;
}

} // 匿名命名空间

void consolidateFeatureGraph(
    const Mesh& mesh,
    const FeatureOptions& options,
    FeatureDetectionCache& cache,
    TraceGraph& trace,
    FeatureAnalysis& analysis
) {
    if (!options.graphConsolidation.enabled || trace.graphEdges.empty() ||
        options.graphConsolidation.maxGapLengthRatio <= 0.0) {
        return;
    }

    const std::vector<int> labels = componentLabels(trace);
    const std::vector<double>& localScale = cache.vertexAverageEdgeLength();
    const double meanScale = fallbackScale(localScale, std::max(1e-12, mesh.bboxDiag() * 1e-3));
    std::vector<Endpoint> endpoints;
    for (int vertex = 0; vertex < static_cast<int>(trace.adjacency.size()); ++vertex) {
        if (trace.adjacency[vertex].size() == 1 && labels[vertex] >= 0) {
            const double scale = vertex < static_cast<int>(localScale.size()) && localScale[vertex] > 0.0
                                     ? localScale[vertex]
                                     : meanScale;
            endpoints.push_back({vertex, labels[vertex], scale});
        }
    }

    constexpr int kMaxConsolidationEndpoints = 512;
    if (static_cast<int>(endpoints.size()) > kMaxConsolidationEndpoints) {
        ++analysis.graphConsolidationSkippedByCap;
        return;
    }

    std::vector<ConsolidationCandidate> candidates;
    for (int first = 0; first < static_cast<int>(endpoints.size()); ++first) {
        for (int second = first + 1; second < static_cast<int>(endpoints.size()); ++second) {
            const Endpoint& lhs = endpoints[first];
            const Endpoint& rhs = endpoints[second];
            if (lhs.component == rhs.component || traceGraphHasEdge(trace, lhs.vertex, rhs.vertex)) {
                continue;
            }
            const double allowed = options.graphConsolidation.maxGapLengthRatio * 0.5 * (lhs.scale + rhs.scale);
            const double distance = (mesh.vertices[lhs.vertex] - mesh.vertices[rhs.vertex]).norm();
            if (distance > allowed) {
                continue;
            }

            const ContinuationBranch lhsBranch =
                bestContinuationBranch(mesh, trace, lhs.vertex, rhs.vertex, options.graphConsolidation.minAlignment);
            const ContinuationBranch rhsBranch =
                bestContinuationBranch(mesh, trace, rhs.vertex, lhs.vertex, options.graphConsolidation.minAlignment);
            if (lhsBranch.neighbor < 0 || rhsBranch.neighbor < 0 ||
                !compatibleFeatureEvidence(lhsBranch.attrs, rhsBranch.attrs)) {
                continue;
            }
            const double normalizedDistance = distance / std::max(allowed, 1e-12);
            const double alignmentPenalty = 0.5 * (2.0 - lhsBranch.alignment - rhsBranch.alignment);
            candidates.push_back({
                lhs.vertex,
                rhs.vertex,
                compatibleSignedKind(lhsBranch.attrs, rhsBranch.attrs),
                distance,
                normalizedDistance + alignmentPenalty,
            });
        }
    }

    std::sort(
        candidates.begin(), candidates.end(), [](const ConsolidationCandidate& lhs, const ConsolidationCandidate& rhs) {
            if (lhs.score != rhs.score) {
                return lhs.score < rhs.score;
            }
            if (lhs.distance != rhs.distance) {
                return lhs.distance < rhs.distance;
            }
            return lhs.first != rhs.first ? lhs.first < rhs.first : lhs.second < rhs.second;
        }
    );

    std::unordered_set<int> used;
    for (const ConsolidationCandidate& candidate : candidates) {
        if (used.find(candidate.first) != used.end() || used.find(candidate.second) != used.end()) {
            continue;
        }
        CandidateEdge bridge;
        bridge.a = candidate.first;
        bridge.b = candidate.second;
        bridge.consolidationBridge = true;
        bridge.signedKind = candidate.signedKind;
        addTraceGraphEdge(trace, analysis, bridge);
        used.insert(candidate.first);
        used.insert(candidate.second);
        ++analysis.graphConsolidationBridges;
    }
    if (!used.empty()) {
        rebuildTraceGraphEdges(trace);
    }
}

} // namespace detector_detail
} // namespace feature
} // namespace manumesh
