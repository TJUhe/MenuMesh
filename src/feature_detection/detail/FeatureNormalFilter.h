/**
 * @file src/feature_detection/detail/FeatureNormalFilter.h
 * @brief Declares feature normal filter facilities for ManuMesh's feature-detection module.
 * @ingroup manumesh_feature_detection
 *
 * @details This file is part of the deterministic triangle-surface feature pipeline. Local evidence is kept separate from graph cleanup, tracing, primitive recovery, and patch segmentation so each stage has an explicit contract.
 */

#pragma once

#include "algorithms/feature_detection/FeatureTypes.h"
#include "common/detail/MeshQueries.h"

namespace manumesh::feature::detector_detail {

/**
 * @brief Internal normal-filter entry that reuses precomputed edge incidence.
 */
FeatureNormalFilterResult filterFeatureNormalsImpl(
    const Mesh& mesh, const common::MeshEdgeInfoMap& edgeInfo, const FeatureNormalFilterOptions& options
);

} // namespace manumesh::feature::detector_detail
