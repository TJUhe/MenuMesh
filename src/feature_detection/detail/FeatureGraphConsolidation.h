/**
 * @file src/feature_detection/detail/FeatureGraphConsolidation.h
 * @brief Declares feature graph consolidation facilities for ManuMesh's feature-detection module.
 * @ingroup manumesh_feature_detection
 *
 * @details This file is part of the deterministic triangle-surface feature pipeline. Local evidence is kept separate from graph cleanup, tracing, primitive recovery, and patch segmentation so each stage has an explicit contract.
 */

#pragma once

#include "FeatureDetectionCache.h"
#include "FeatureDetectionTypes.h"
#include "algorithms/feature_detection/FeatureTypes.h"

namespace manumesh::feature::detector_detail {

/// Bridges aligned, evidence-compatible endpoints across nearby graph components.
void consolidateFeatureGraph(
    const Mesh& mesh,
    const FeatureOptions& options,
    FeatureDetectionCache& cache,
    TraceGraph& trace,
    FeatureAnalysis& analysis
);

} // namespace manumesh::feature::detector_detail
