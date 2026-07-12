#pragma once

#include "detail/SimplificationTypes.h"
#include "mesh_edit/detail/DynamicTopology.h"

#include <vector>

namespace manumesh::simplification {

// Placement strategies for edge collapses. This unit owns "where does the
// merged vertex go" policies; legality lives in CollapseTopology/
// CollapseLegality and feature-curve constraints live in FeatureConstraints.

struct BoundaryProjectionInput {
    CollapseEdge edge;
    const BoundaryCollapseDecision& decision;
    const std::vector<VertexState>& vertices;
    const std::vector<FaceState>& faces;
    const mesh_edit::DynamicTopology& topology;
};

/// Places a boundary-edge collapse using the Lindstrom-Turk boundary
/// preservation constraint (M032 4.2.2): the placement is projected onto the
/// line that minimizes the change of the boundary's directed area over the
/// incident boundary chain, then clamped to the collapsing edge's shadow on
/// that line. Falls back to clamping onto the segment [keep, remove] when the
/// local boundary chain is degenerate.
bool projectBoundaryPlacement(const BoundaryProjectionInput& input, Vec3& position);

} // namespace manumesh::simplification
