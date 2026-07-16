/**
 * @file src/feature_detection/detail/FeatureEvidence.h
 * @brief Declares feature evidence facilities for ManuMesh's feature-detection module.
 * @ingroup manumesh_feature_detection
 *
 * @details This file is part of the deterministic triangle-surface feature pipeline. Local evidence is kept separate from graph cleanup, tracing, primitive recovery, and patch segmentation so each stage has an explicit contract.
 */

#pragma once

#include "FeatureDetectionCache.h"
#include "FeatureDetectionTypes.h"
#include "algorithms/feature_detection/FeatureTypes.h"

#include <vector>

namespace manumesh::feature::detector_detail {

/**
 * @brief Collects typed hard/weak edge evidence and updates source counters.
 * @return Deterministically ordered candidate edges.
 */
std::vector<CandidateEdge> collectFeatureEdges(
    const Mesh& mesh, const FeatureOptions& options, FeatureDetectionCache& cache, FeatureAnalysisBuilder& builder
);

} // namespace manumesh::feature::detector_detail
