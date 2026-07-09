#pragma once

#include "FeatureDetectionTypes.h"
#include "algorithms/feature_detection/FeatureTypes.h"

namespace manumesh::feature::detector_detail {

void cleanupTraceGraph(const Mesh& mesh, const FeatureOptions& options,
                       TraceGraph& trace, FeatureAnalysis& analysis);

void summarizeFeatureComponents(const Mesh& mesh, const FeatureOptions& options,
                                const TraceGraph& trace, FeatureAnalysis& analysis);

} // namespace manumesh::feature::detector_detail
