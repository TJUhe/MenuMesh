/**
 * @file src/feature_detection/detail/FeatureLoopRecovery.h
 * @brief Declares feature loop recovery facilities for ManuMesh's feature-detection module.
 * @ingroup manumesh_feature_detection
 *
 * @details This file is part of the deterministic triangle-surface feature pipeline. Local evidence is kept separate from graph cleanup, tracing, primitive recovery, and patch segmentation so each stage has an explicit contract.
 */

#pragma once

#include "FeatureDetectionTypes.h"
#include "algorithms/feature_detection/FeatureTypes.h"

namespace manumesh::feature::detector_detail {

/**
 * @brief Executes the ordered loop-recovery stages on a cleaned trace graph.
 */
void recoverFeatureLoops(
    const Mesh& mesh, const FeatureOptions& options, const TraceGraph& trace, FeatureAnalysis& analysis, int& loopId
);

} // namespace manumesh::feature::detector_detail
