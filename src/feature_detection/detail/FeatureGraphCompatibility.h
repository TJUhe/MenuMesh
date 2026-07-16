/**
 * @file src/feature_detection/detail/FeatureGraphCompatibility.h
 * @brief Declares feature graph compatibility facilities for ManuMesh's feature-detection module.
 * @ingroup manumesh_feature_detection
 *
 * @details This file is part of the deterministic triangle-surface feature pipeline. Local evidence is kept separate from graph cleanup, tracing, primitive recovery, and patch segmentation so each stage has an explicit contract.
 */

#pragma once

#include "FeatureDetectionTypes.h"
#include "core/Mesh.h"

namespace manumesh::feature::detector_detail {

struct ContinuationBranch {
    int neighbor = -1;                     ///< Neighbor vertex, or -1 when no branch qualifies.
    const TraceEdgeAttrs* attrs = nullptr; ///< Attributes of the selected graph edge.
    double alignment = 0.0;                ///< Absolute tangent alignment in [0,1].
};

/// Selects the most collinear continuation from `vertex` toward `target`.
ContinuationBranch
bestContinuationBranch(const Mesh& mesh, const TraceGraph& trace, int vertex, int target, double minAlignment);

/// @return true when two edges may participate in one recovered continuation.
bool compatibleFeatureEvidence(const TraceEdgeAttrs* lhs, const TraceEdgeAttrs* rhs);

/// @return Compatible convex/concave sign, or zero for unknown/incompatible input.
int compatibleSignedKind(const TraceEdgeAttrs* lhs, const TraceEdgeAttrs* rhs);

} // namespace manumesh::feature::detector_detail
