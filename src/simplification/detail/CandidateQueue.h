#pragma once

#include "detail/SimplificationTypes.h"
#include "line_quadrics_qem/algorithms/simplification/QEMSimplifier.h"

#include <queue>
#include <vector>

namespace lq {

class CandidateQueue {
public:
  void clear();
  bool empty() const;
  Candidate pop();
  void pushEdge(int a, int b, const std::vector<VertexState>& vertices,
                SimplifyReport& report);

private:
  std::priority_queue<Candidate> queue_;
};

} // namespace lq
