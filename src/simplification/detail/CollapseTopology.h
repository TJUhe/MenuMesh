/**
 * @file src/simplification/detail/CollapseTopology.h
 * @brief Declares collapse topology facilities for ManuMesh's simplification module.
 * @ingroup manumesh_simplification
 *
 * @details This file is part of the feature-aware edge-collapse pipeline. Quadric costs rank candidates; topology, geometry, feature, boundary, error, and optional texture policies decide whether a placement may mutate the mesh.
 */

#pragma once

#include "algorithms/simplification/SimplificationTypes.h"
#include "detail/SimplificationTypes.h"
#include "mesh_edit/detail/DynamicTopology.h"

#include <vector>

namespace manumesh::simplification {

/** @copydoc mesh_edit::activeIncidentFaceCountForEdge */
using mesh_edit::activeIncidentFaceCountForEdge;
/** @copydoc mesh_edit::areAdjacent */
using mesh_edit::areAdjacent;
/** @copydoc mesh_edit::collectActiveEdges */
using mesh_edit::collectActiveEdges;
/** @copydoc mesh_edit::containsVertex */
using mesh_edit::containsVertex;
using mesh_edit::DynamicTopology;

/**
 * @brief Local incidence and positions needed to decide an open-boundary contraction.
 */
struct BoundaryCollapseInput {
    CollapseEdge edge;
    const std::vector<FaceState>& faces;
    const std::vector<VertexState>& vertices;
    const DynamicTopology& topology;
    const SimplifyOptions& options;
};

/**
 * @brief Classifies and, when possible, constrains a boundary collapse.
 */
BoundaryCollapseDecision boundaryCollapseDecision(const BoundaryCollapseInput& input);

/** @brief Returns sorted active one-ring neighbors of a vertex. */
std::vector<int> activeNeighborsOf(
    int vertex,
    const std::vector<FaceState>& faces,
    const std::vector<VertexState>& vertices,
    const DynamicTopology& topology
);

/**
 * @brief Checks the simplicial link condition link(keep) intersect link(remove) =
 * link(edge), including both vertices and edges in the endpoint links. The
 * boundary extension rejects interior chords whose endpoints are both on an
 * open boundary and collapses that would erase an isolated open triangle.
 */
bool collapseWouldPreserveLinkCondition(
    int keep,
    int remove,
    const std::vector<FaceState>& faces,
    const std::vector<VertexState>& vertices,
    const DynamicTopology& topology
);

} // namespace manumesh::simplification
