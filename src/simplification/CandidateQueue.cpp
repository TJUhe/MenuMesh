/**
 * @file src/simplification/CandidateQueue.cpp
 * @brief 实现 ManuMesh 的简化模块的候选队列功能。
 * @ingroup manumesh_simplification
 *
 * @details 维护带缓存放置解的最小代价候选堆。
 * @algorithm 候选项保存端点版本戳和按升序排列的放置代价。队列本身不求解二次误差；过期候选的检测和局部重建策略由 SimplificationRun 负责。
 * @complexity 入队和出队的复杂度均为 O(log E)。
 */

#include "detail/CandidateQueue.h"

#include <algorithm>
#include <cmath>

namespace manumesh {
namespace simplification {

void CandidateQueue::clear() { queue_ = std::priority_queue<Candidate>(); }

bool CandidateQueue::empty() const { return queue_.empty(); }

Candidate CandidateQueue::pop() {
    Candidate candidate = queue_.top();
    queue_.pop();
    return candidate;
}

void CandidateQueue::pushEdge(
    int a,
    int b,
    const std::vector<VertexState>& vertices,
    const std::vector<SolveResult>& placements,
    double additionalCost
) {
    if (a == b || !vertices[a].active || !vertices[b].active || placements.empty() || !std::isfinite(additionalCost)) {
        return;
    }
    const int first = std::min(a, b);
    const int second = std::max(a, b);
    // 特征优先级增益只会重新排序队列（Wang 2008 的解耦策略），不会修改用于放置求解的二次误差矩阵。
    const double priorityScale = std::max(vertices[first].priorityScale, vertices[second].priorityScale);
    Candidate candidate{
        placements.front().cost * priorityScale + additionalCost,
        first,
        second,
        vertices[first].version,
        vertices[second].version,
        {},
        0
    };
    candidate.placementCount =
        std::min(static_cast<int>(candidate.placements.size()), static_cast<int>(placements.size()));
    for (int i = 0; i < candidate.placementCount; ++i) {
        candidate.placements[static_cast<std::size_t>(i)] = placements[static_cast<std::size_t>(i)];
    }
    queue_.push(candidate);
}

} // namespace simplification
} // namespace manumesh
