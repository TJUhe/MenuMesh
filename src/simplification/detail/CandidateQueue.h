#pragma once

#include "detail/SimplificationTypes.h"

#include <queue>
#include <vector>

namespace manumesh::simplification {

class CandidateQueue {
public:
    void clear();
    bool empty() const;
    Candidate pop();
    /// Pushes an edge candidate that reuses placements already solved by the
    /// caller (sorted by ascending cost). No quadric solve happens here.
    void pushEdge(
        int a,
        int b,
        const std::vector<VertexState>& vertices,
        const std::vector<SolveResult>& placements,
        double additionalCost = 0.0
    );

private:
    std::priority_queue<Candidate> queue_;
};

} // namespace manumesh::simplification
