/**
 * @file src/simplification/detail/Placement.h
 * @brief Declares placement facilities for ManuMesh's simplification module.
 * @ingroup manumesh_simplification
 *
 * @details This file is part of the feature-aware edge-collapse pipeline. Quadric costs rank candidates; topology, geometry, feature, boundary, error, and optional texture policies decide whether a placement may mutate the mesh.
 */

#pragma once

#include "detail/SimplificationTypes.h"
#include "mesh_edit/detail/DynamicTopology.h"

#include <vector>

namespace manumesh::simplification {

// Placement strategies for edge collapses. This unit owns "where does the
// merged vertex go" policies; legality lives in CollapseTopology/
// CollapseLegality and feature-curve constraints live in FeatureConstraints.

/**
 * @brief Directed boundary-chain geometry used by Lindstrom-Turk placement projection.
 */
struct BoundaryProjectionInput {
    CollapseEdge edge;
    const BoundaryCollapseDecision& decision;
    const std::vector<VertexState>& vertices;
    const std::vector<FaceState>& faces;
    const mesh_edit::DynamicTopology& topology;
};

/**
 * @brief Places a boundary-edge collapse using the Lindstrom-Turk boundary
 * preservation constraint (M032 4.2.2): the placement is projected onto the
 * line that minimizes the change of the boundary's directed area over the
 * incident boundary chain, then clamped to the collapsing edge's shadow on
 * that line. Falls back to clamping onto the segment [keep, remove] when the
 * local boundary chain is degenerate.
 * Projects `position` to the local boundary objective and safety segment.
 * @return true when a finite constrained position was produced.
 */
bool projectBoundaryPlacement(const BoundaryProjectionInput& input, Vec3& position);

} // namespace manumesh::simplification
