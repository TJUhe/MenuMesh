/**
 * @file src/feature_detection/FeatureCycleRecovery.cpp
 * @brief 实现 ManuMesh 的特征检测模块的特征环恢复功能。
 * @ingroup manumesh_feature_detection
 *
 * @details 恢复经过分叉点或属于小型环基的有界特征环。
 * @algorithm 使用确定性的最短路/环候选，以规范化无向边签名去重，
 *            并在接受环之前通过图元拟合和证据门限校验。
 * @failuremodes 在稠密图上限制搜索深度和候选数量。
 */

#include "detail/FeatureCycleRecovery.h"

#include "common/detail/MeshQueries.h"
#include "detail/FeatureGraph.h"
#include "detail/FeatureLoopBuilder.h"

#include <algorithm>
#include <queue>
#include <unordered_set>

namespace manumesh::feature::detector_detail {
namespace {

/** @brief 图环恢复期间使用的有序链及闭合状态。 */
struct FeatureChain {
    std::vector<int> vertices;
    int loEndpoint = -1;
    int hiEndpoint = -1;
};

std::vector<FeatureChain> traceJunctionChains(const std::vector<std::vector<int>>& adjacency) {
    std::vector<FeatureChain> chains;
    std::vector<char> isJunction(adjacency.size(), 0);
    for (int i = 0; i < static_cast<int>(adjacency.size()); ++i) {
        isJunction[i] = !adjacency[i].empty() && adjacency[i].size() != 2;
    }

    CycleSignatureSet seenChains;
    for (int seed = 0; seed < static_cast<int>(adjacency.size()); ++seed) {
        if (!isJunction[seed]) {
            continue;
        }
        for (int nb : adjacency[seed]) {
            std::vector<int> chain;
            chain.push_back(seed);
            int previous = seed;
            int current = nb;
            while (true) {
                chain.push_back(current);
                if (current != seed && isJunction[current]) {
                    break;
                }
                if (adjacency[current].size() != 2) {
                    break;
                }
                int next = -1;
                for (int candidate : adjacency[current]) {
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

            const int end = chain.back();
            if (end == seed || !isJunction[end]) {
                continue;
            }
            if (!seenChains.insert(cycleSignature(chain)).second) {
                continue;
            }
            FeatureChain featureChain;
            featureChain.loEndpoint = std::min(seed, end);
            featureChain.hiEndpoint = std::max(seed, end);
            if (seed == featureChain.loEndpoint) {
                featureChain.vertices = std::move(chain);
            } else {
                featureChain.vertices.assign(chain.rbegin(), chain.rend());
            }
            chains.push_back(std::move(featureChain));
        }
    }
    return chains;
}

std::vector<int> treePathCycle(int u, int v, const std::vector<int>& parent, const std::vector<int>& depth) {
    std::vector<int> uPath;
    std::vector<int> vPath;
    int a = u;
    int b = v;
    while (a != b) {
        if (depth[a] >= depth[b]) {
            uPath.push_back(a);
            a = parent[a];
        } else {
            vPath.push_back(b);
            b = parent[b];
        }
        if (a < 0 || b < 0) {
            return {};
        }
    }
    uPath.push_back(a);
    std::vector<int> cycle = std::move(uPath);
    for (auto it = vPath.rbegin(); it != vPath.rend(); ++it) {
        cycle.push_back(*it);
    }
    return cycle;
}

bool componentHasWeakEvidenceEdge(const std::vector<int>& component, const TraceGraph& trace) {
    for (int v : component) {
        for (int nb : trace.adjacency[v]) {
            if (v >= nb) {
                continue;
            }
            const TraceEdgeAttrs* attrs = traceEdgeAttrs(trace, v, nb);
            if (attrs != nullptr && (attrs->normalTensor || attrs->smoothCurvature)) {
                return true;
            }
        }
    }
    return false;
}

} // 匿名命名空间

void recoverCircularCyclesThroughJunctions(
    const Mesh& mesh, const FeatureOptions& options, const TraceGraph& trace, FeatureAnalysis& analysis, int& loopId
) {
    const std::vector<FeatureChain> chains = traceJunctionChains(trace.adjacency);
    CycleSignatureSet seenCycles;
    for (int i = 0; i < static_cast<int>(chains.size()); ++i) {
        for (int j = i + 1; j < static_cast<int>(chains.size()); ++j) {
            if (chains[i].loEndpoint != chains[j].loEndpoint || chains[i].hiEndpoint != chains[j].hiEndpoint) {
                continue;
            }
            std::vector<int> cycle = chains[i].vertices;
            for (int k = static_cast<int>(chains[j].vertices.size()) - 2; k > 0; --k) {
                cycle.push_back(chains[j].vertices[k]);
            }
            addRecoveredCycle(
                RecoveredCycleKind::Circular, std::move(cycle), seenCycles, mesh, options, trace, analysis, loopId
            );
        }
    }
}

void recoverSmallCycleBasis(
    const Mesh& mesh, const FeatureOptions& options, const TraceGraph& trace, FeatureAnalysis& analysis, int& loopId
) {
    const std::vector<std::vector<int>>& adjacency = trace.adjacency;
    std::vector<char> componentVisited(mesh.vertices.size(), 0);
    CycleSignatureSet seenCycles;
    for (const FeatureLoop& loop : analysis.loops) {
        if (loop.closed) {
            seenCycles.insert(cycleSignature(loop.vertices));
        }
    }

    constexpr int kMaxCycleComponentVertices = 160;
    constexpr int kMaxCycleComponentEdges = 240;
    constexpr int kMaxCycleRank = 32;
    constexpr int kMaxCycleVertices = 80;
    std::vector<int> parent(mesh.vertices.size(), -1);
    std::vector<int> depth(mesh.vertices.size(), 0);
    for (int seed = 0; seed < static_cast<int>(adjacency.size()); ++seed) {
        if (componentVisited[seed] || adjacency[seed].empty()) {
            continue;
        }

        std::vector<int> component;
        std::queue<int> queue;
        queue.push(seed);
        componentVisited[seed] = 1;
        parent[seed] = seed;
        depth[seed] = 0;
        int edgeCount2x = 0;
        std::unordered_set<std::uint64_t> treeEdges;
        while (!queue.empty()) {
            const int v = queue.front();
            queue.pop();
            component.push_back(v);
            edgeCount2x += static_cast<int>(adjacency[v].size());
            // 按排序后的顺序访问邻点，使 BFS 树及由此得到的每个基本环
            // 不依赖邻接表的插入顺序。
            std::vector<int> neighbors = adjacency[v];
            std::sort(neighbors.begin(), neighbors.end());
            for (int nb : neighbors) {
                if (!componentVisited[nb]) {
                    componentVisited[nb] = 1;
                    parent[nb] = v;
                    depth[nb] = depth[v] + 1;
                    treeEdges.insert(manumesh::common::meshEdgeKey(v, nb));
                    queue.push(nb);
                }
            }
        }

        const int edgeCount = edgeCount2x / 2;
        const int cycleRank = edgeCount - static_cast<int>(component.size()) + 1;
        if (componentHasWeakEvidenceEdge(component, trace)) {
            continue;
        }
        if (static_cast<int>(component.size()) < options.minFeatureLoopVertices ||
            static_cast<int>(component.size()) > kMaxCycleComponentVertices || edgeCount > kMaxCycleComponentEdges ||
            cycleRank <= 0 || cycleRank > kMaxCycleRank) {
            continue;
        }

        std::sort(component.begin(), component.end());
        for (int v : component) {
            std::vector<int> neighbors = adjacency[v];
            std::sort(neighbors.begin(), neighbors.end());
            for (int nb : neighbors) {
                if (v >= nb || treeEdges.find(manumesh::common::meshEdgeKey(v, nb)) != treeEdges.end()) {
                    continue;
                }
                std::vector<int> cycle = treePathCycle(v, nb, parent, depth);
                if (static_cast<int>(cycle.size()) <= kMaxCycleVertices) {
                    addRecoveredCycle(
                        RecoveredCycleKind::Polygonal,
                        std::move(cycle),
                        seenCycles,
                        mesh,
                        options,
                        trace,
                        analysis,
                        loopId
                    );
                }
            }
        }
    }
}

} // 命名空间 manumesh::feature::detector_detail
