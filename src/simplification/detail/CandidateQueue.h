/**
 * @file src/simplification/detail/CandidateQueue.h
 * @brief Declares candidate queue facilities for ManuMesh's simplification module.
 * @ingroup manumesh_simplification
 *
 * @details This file is part of the feature-aware edge-collapse pipeline. Quadric costs rank candidates; topology, geometry, feature, boundary, error, and optional texture policies decide whether a placement may mutate the mesh.
 */

#pragma once

#include "detail/SimplificationTypes.h"

#include <queue>
#include <vector>

namespace manumesh::simplification {

/**
 * @brief Min-heap of versioned collapse candidates with cached placement solutions.
 */
class CandidateQueue {
public:
    /**
     * @brief Removes every queued candidate.
     */
    void clear();
    /**
     * @return true when no candidate is queued.
     */
    bool empty() const;
    /**
     * @brief Removes and returns the lowest-cost candidate.
     * @pre empty() is false.
     */
    Candidate pop();
    /**
     * @brief Pushes an edge candidate that reuses placements already solved by the
     * caller (sorted by ascending cost). No quadric solve happens here.
     */
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
