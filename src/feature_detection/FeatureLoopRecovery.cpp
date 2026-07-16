/**
 * @file src/feature_detection/FeatureLoopRecovery.cpp
 * @brief Implements feature loop recovery facilities for ManuMesh's feature-detection module.
 * @ingroup manumesh_feature_detection
 *
 * @details This file is part of the deterministic triangle-surface feature pipeline. Local evidence is kept separate from graph cleanup, tracing, primitive recovery, and patch segmentation so each stage has an explicit contract.
 */

#include "detail/FeatureLoopRecovery.h"

#include "detail/FeatureCircularRecovery.h"
#include "detail/FeatureCycleRecovery.h"
#include "detail/FeaturePrimitiveRecovery.h"
#include "detail/FeatureTraceRecovery.h"

namespace manumesh::feature::detector_detail {

void recoverFeatureLoops(
    const Mesh& mesh, const FeatureOptions& options, const TraceGraph& trace, FeatureAnalysis& analysis, int& loopId
) {
    recoverCircularCyclesThroughJunctions(mesh, options, trace, analysis, loopId);
    recoverSmallCycleBasis(mesh, options, trace, analysis, loopId);
    traceRemainingFeatureLoops(mesh, options, trace, analysis, loopId);
    recoverPrimitiveComponents(mesh, options, trace, analysis, loopId);
    recoverCircularVertexClusters(mesh, options, trace, analysis, loopId);
}

} // namespace manumesh::feature::detector_detail
