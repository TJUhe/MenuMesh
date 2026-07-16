/**
 * @file src/feature_detection/detail/FeatureCircularRecovery.h
 * @brief Declares feature circular recovery facilities for ManuMesh's feature-detection module.
 * @ingroup manumesh_feature_detection
 *
 * @details This file is part of the deterministic triangle-surface feature pipeline. Local evidence is kept separate from graph cleanup, tracing, primitive recovery, and patch segmentation so each stage has an explicit contract.
 */

#pragma once

#include "FeatureDetectionTypes.h"
#include "algorithms/feature_detection/FeatureTypes.h"

namespace manumesh::feature::detector_detail {

/**
 * @brief Recovers closed circular loops from spatially coherent feature-vertex clusters.
 * @param[in] mesh Source mesh.
 * @param[in] options Primitive residual and minimum-size gates.
 * @param[in] trace Cleaned feature graph.
 * @param[in,out] analysis Destination loop/ownership records and diagnostics.
 * @param[in,out] loopId Monotonic id assigned to accepted loops.
 */
void recoverCircularVertexClusters(
    const Mesh& mesh, const FeatureOptions& options, const TraceGraph& trace, FeatureAnalysis& analysis, int& loopId
);

} // namespace manumesh::feature::detector_detail
