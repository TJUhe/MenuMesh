#pragma once

#include "FeatureDetectionTypes.h"
#include "algorithms/feature_detection/FeatureTypes.h"

#include <vector>

namespace manumesh::feature::detector_detail {

void initializeFeatureGraph(const std::vector<CandidateEdge>& featureEdges,
                            FeatureAnalysis& analysis);

TraceGraph buildTraceGraph(const Mesh& mesh, const FeatureOptions& options,
                           const std::vector<CandidateEdge>& featureEdges,
                           FeatureAnalysis& analysis);

bool traceEdgeBoundary(const TraceGraph& trace, int a, int b);
bool traceEdgeNormalTensor(const TraceGraph& trace, int a, int b);
int traceEdgeSign(const TraceGraph& trace, int a, int b);

void finalizeFeatureGraphMarkers(FeatureAnalysis& analysis);

} // namespace manumesh::feature::detector_detail
