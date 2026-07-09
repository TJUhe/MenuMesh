#pragma once

#include "algorithms/simplification/QEMSimplifier.h"
#include "detail/SimplificationTypes.h"

#include <queue>
#include <vector>

namespace manumesh::simplification {

class CandidateQueue {
public:
  void clear();
  bool empty() const;
  Candidate pop();
  void pushEdge(int a, int b, const std::vector<VertexState>& vertices);

private:
  std::priority_queue<Candidate> queue_;
};

} // namespace manumesh::simplification
