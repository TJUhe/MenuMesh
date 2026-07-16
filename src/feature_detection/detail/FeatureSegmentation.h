/**
 * @file src/feature_detection/detail/FeatureSegmentation.h
 * @brief Declares feature segmentation facilities for ManuMesh's feature-detection module.
 * @ingroup manumesh_feature_detection
 *
 * @details This file is part of the deterministic triangle-surface feature pipeline. Local evidence is kept separate from graph cleanup, tracing, primitive recovery, and patch segmentation so each stage has an explicit contract.
 */

#pragma once

#include "algorithms/feature_detection/FeatureTypes.h"

namespace manumesh::feature::detector_detail {

/**
 * @brief Flood-fills faces across non-feature manifold adjacencies and summarizes patches.
 */
void buildFeaturePatches(const Mesh& mesh, FeatureAnalysis& analysis, const SurfacePatchOptions& options);

} // namespace manumesh::feature::detector_detail
