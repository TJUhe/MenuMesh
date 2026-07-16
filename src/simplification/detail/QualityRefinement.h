/**
 * @file src/simplification/detail/QualityRefinement.h
 * @brief Declares quality refinement facilities for ManuMesh's simplification module.
 * @ingroup manumesh_simplification
 *
 * @details This file is part of the feature-aware edge-collapse pipeline. Quadric costs rank candidates; topology, geometry, feature, boundary, error, and optional texture policies decide whether a placement may mutate the mesh.
 */

#pragma once

#include "algorithms/simplification/SimplificationTypes.h"
#include "common/detail/MeshDistanceIndex.h"
#include "detail/CollapseTopology.h"
#include "detail/FeatureConstraints.h"
#include "detail/SpatialFaceIndex.h"

#include <vector>

namespace manumesh::simplification {

/**
 * @brief Immutable constraints and mutable mesh passed to fixed-topology refinement.
 */
struct QualityRefinementInput {
    const SimplifyOptions& options;
    std::vector<VertexState>& vertices;
    const std::vector<FaceState>& faces;
    const DynamicTopology& topology;
    const FeatureConstraintPolicy& featurePolicy;
    const std::vector<FeatureCurveConstraint>& featureCurves;
    const std::vector<FeaturePrimitiveFit>& primitiveFits;
    SpatialFaceIndex* spatialIndex = nullptr;
    const manumesh::common::MeshDistanceIndex* referenceSurface = nullptr;
    double meshDiagonal = 0.0;
    double areaEps = 0.0;
    double minNormalDot = -1.0;
    double maxLocalError = 0.0;
};

/**
 * @brief Runs bounded tangential relocation and updates refinement diagnostics.
 */
void runQualityRefinement(const QualityRefinementInput& input, SimplifyReport& report);

} // namespace manumesh::simplification
