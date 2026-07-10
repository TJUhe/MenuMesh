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
