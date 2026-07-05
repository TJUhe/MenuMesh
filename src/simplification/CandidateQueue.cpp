#include "detail/CandidateQueue.h"

#include "detail/Quadrics.h"

#include <algorithm>

namespace lq {

void CandidateQueue::clear() {
  queue_ = std::priority_queue<Candidate>();
}

bool CandidateQueue::empty() const {
  return queue_.empty();
}

Candidate CandidateQueue::pop() {
  Candidate candidate = queue_.top();
  queue_.pop();
  return candidate;
}

void CandidateQueue::pushEdge(int a, int b, const std::vector<VertexState>& vertices) {
  if (a == b || !vertices[a].active || !vertices[b].active) {
    return;
  }
  const Mat4 q = vertices[a].q + vertices[b].q;
  const SolveResult solve = solveOptimal(q, vertices[a].p, vertices[b].p);
  queue_.push(Candidate{solve.cost, std::min(a, b), std::max(a, b), vertices[a].version,
                        vertices[b].version});
}

} // namespace lq
