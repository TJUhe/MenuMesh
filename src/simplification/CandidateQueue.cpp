#include "detail/CandidateQueue.h"

#include "detail/Quadrics.h"

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

void CandidateQueue::pushEdge(int a, int b, const std::vector<VertexState>& vertices, double additionalCost) {
    if (a == b || !vertices[a].active || !vertices[b].active || !std::isfinite(additionalCost)) {
        return;
    }
    const Mat4 q = vertices[a].q + vertices[b].q;
    const SolveResult solve = solveOptimal(q, vertices[a].p, vertices[b].p);
    const int first = std::min(a, b);
    const int second = std::max(a, b);
    queue_.push(
        Candidate{solve.cost + additionalCost, first, second, vertices[first].version, vertices[second].version}
    );
}

} // namespace manumesh::simplification
