#pragma once

#include "FeatureDetectionTypes.h"
#include "algorithms/feature_detection/FeatureTypes.h"

#include <vector>

namespace manumesh::feature::detector_detail {

void initializeFeatureGraph(const std::vector<CandidateEdge>& featureEdges, FeatureAnalysis& analysis);

TraceGraph buildTraceGraph(
    const Mesh& mesh,
    const FeatureOptions& options,
    const std::vector<CandidateEdge>& featureEdges,
    FeatureAnalysis& analysis
);

bool traceEdgeBoundary(const TraceGraph& trace, int a, int b);
bool traceEdgeDihedral(const TraceGraph& trace, int a, int b);
bool traceEdgeNormalTensor(const TraceGraph& trace, int a, int b);
bool traceEdgeSmoothCurvature(const TraceGraph& trace, int a, int b);
bool traceEdgeNonManifold(const TraceGraph& trace, int a, int b);
bool traceEdgeCleanupBridge(const TraceGraph& trace, int a, int b);
int traceEdgeSign(const TraceGraph& trace, int a, int b);
double traceEdgeTensorPersistence(const TraceGraph& trace, int a, int b);
int traceEdgeTensorPersistentScales(const TraceGraph& trace, int a, int b);
double traceEdgeCurvaturePersistence(const TraceGraph& trace, int a, int b);
int traceEdgeCurvaturePersistentScales(const TraceGraph& trace, int a, int b);

bool traceGraphHasEdge(const TraceGraph& trace, int a, int b);
void addTraceGraphEdge(TraceGraph& trace, FeatureAnalysis& analysis, const CandidateEdge& edge);
void removeTraceGraphEdge(TraceGraph& trace, int a, int b);
void rebuildTraceGraphEdges(TraceGraph& trace);

void finalizeFeatureGraphMarkers(FeatureAnalysis& analysis);

} // namespace manumesh::feature::detector_detail
