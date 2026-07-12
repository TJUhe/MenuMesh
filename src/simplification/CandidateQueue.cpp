#include "detail/CandidateQueue.h"

#include <algorithm>
#include <cmath>

namespace manumesh::simplification {

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
    // Feature-priority boost only reorders the queue (Wang 2008 decoupling);
    // it never touches the quadric used for the placement solve.
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

} // namespace manumesh::simplification
