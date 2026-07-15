#pragma once

#include "algorithms/simplification/SimplificationTypes.h"
#include "detail/SimplificationTypes.h"
#include "mesh_edit/detail/DynamicTopology.h"

#include <vector>

namespace manumesh::simplification {

using mesh_edit::activeIncidentFaceCountForEdge;
using mesh_edit::areAdjacent;
using mesh_edit::collectActiveEdges;
using mesh_edit::containsVertex;
using mesh_edit::DynamicTopology;

struct BoundaryCollapseInput {
    CollapseEdge edge;
    const std::vector<FaceState>& faces;
    const std::vector<VertexState>& vertices;
    const DynamicTopology& topology;
    const SimplifyOptions& options;
};

BoundaryCollapseDecision boundaryCollapseDecision(const BoundaryCollapseInput& input);

std::vector<int> activeNeighborsOf(
    int vertex,
    const std::vector<FaceState>& faces,
    const std::vector<VertexState>& vertices,
    const DynamicTopology& topology
);

/// Checks the simplicial link condition link(keep) intersect link(remove) =
/// link(edge), including both vertices and edges in the endpoint links. The
/// boundary extension rejects interior chords whose endpoints are both on an
/// open boundary and collapses that would erase an isolated open triangle.
bool collapseWouldPreserveLinkCondition(
    int keep,
    int remove,
    const std::vector<FaceState>& faces,
    const std::vector<VertexState>& vertices,
    const DynamicTopology& topology
);

} // namespace manumesh::simplification
