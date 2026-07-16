/**
 * @file src/simplification/detail/CollapseLegality.h
 * @brief Declares collapse legality facilities for ManuMesh's simplification module.
 * @ingroup manumesh_simplification
 *
 * @details This file is part of the feature-aware edge-collapse pipeline. Quadric costs rank candidates; topology, geometry, feature, boundary, error, and optional texture policies decide whether a placement may mutate the mesh.
 */

#pragma once

#include "common/detail/MeshDistanceIndex.h"
#include "core/Mesh.h"
#include "detail/CollapseTopology.h"
#include "detail/SpatialFaceIndex.h"

#include <vector>

namespace manumesh::simplification {

/**
 * @brief Read-only active vertex/face/topology view used by legality predicates.
 */
struct MeshStateView {
    const std::vector<FaceState>& faces;
    const std::vector<VertexState>& vertices;
    const DynamicTopology& topology;
};

/**
 * @brief Proposed edge placement plus every enabled geometric acceptance threshold.
 */
struct CollapseLegalityInput {
    CollapseEdge edge;
    Vec3 newPosition = Vec3::Zero();
    MeshStateView mesh;
    double areaEps = 0.0;
    double minTriangleQuality = 0.0;
    double minNormalDot = -1.0;
    double maxLocalError = 0.0;
    bool preventLocalIntersections = false;
    const SpatialFaceIndex* spatialIndex = nullptr;
    const manumesh::common::MeshDistanceIndex* referenceSurface = nullptr;
};

/**
 * @brief Evaluates placement-dependent legality after the edge topology has already
 * passed collapseWouldPreserveLinkCondition().
 * Evaluates topology, degeneracy, normal, quality, error, and intersection gates.
 * @return None when every enabled hard check passes, otherwise the first rejection.
 */
CollapseRejectReason collapsePlacementRejectReason(const CollapseLegalityInput& input);

} // namespace manumesh::simplification
