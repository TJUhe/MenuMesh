/**
 * @file src/feature_detection/detail/FeatureCycleRecovery.h
 * @brief Declares feature cycle recovery facilities for ManuMesh's feature-detection module.
 * @ingroup manumesh_feature_detection
 *
 * @details This file is part of the deterministic triangle-surface feature pipeline. Local evidence is kept separate from graph cleanup, tracing, primitive recovery, and patch segmentation so each stage has an explicit contract.
 */

#pragma once

#include "FeatureDetectionTypes.h"
#include "algorithms/feature_detection/FeatureTypes.h"

namespace manumesh::feature::detector_detail {

/// Finds primitive-valid cycles that traverse vertices with degree greater than two.
void recoverCircularCyclesThroughJunctions(
    const Mesh& mesh, const FeatureOptions& options, const TraceGraph& trace, FeatureAnalysis& analysis, int& loopId
);

/// Recovers a bounded deterministic cycle basis for still-unowned graph edges.
void recoverSmallCycleBasis(
    const Mesh& mesh, const FeatureOptions& options, const TraceGraph& trace, FeatureAnalysis& analysis, int& loopId
);

} // namespace manumesh::feature::detector_detail
