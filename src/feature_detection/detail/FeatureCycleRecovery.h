#pragma once

#include "FeatureDetectionTypes.h"
#include "manumesh/algorithms/feature_detection/FeatureTypes.h"

namespace manumesh::feature::detector_detail {

void recoverCircularCyclesThroughJunctions(const Mesh& mesh,
                                           const FeatureOptions& options,
                                           const TraceGraph& trace,
                                           FeatureAnalysis& analysis, int& loopId);

void recoverSmallCycleBasis(const Mesh& mesh, const FeatureOptions& options,
                            const TraceGraph& trace, FeatureAnalysis& analysis,
                            int& loopId);

} // namespace manumesh::feature::detector_detail
