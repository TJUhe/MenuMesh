/**
 * @file src/feature_detection/detail/FeatureGraph.h
 * @brief Declares feature graph facilities for ManuMesh's feature-detection module.
 * @ingroup manumesh_feature_detection
 *
 * @details This file is part of the deterministic triangle-surface feature pipeline. Local evidence is kept separate from graph cleanup, tracing, primitive recovery, and patch segmentation so each stage has an explicit contract.
 */

#pragma once

#include "FeatureDetectionTypes.h"
#include "algorithms/feature_detection/FeatureTypes.h"

#include <vector>

namespace manumesh::feature::detector_detail {

/**
 * @brief Copies candidate edges into the public graph and initializes vertex storage.
 */
void initializeFeatureGraph(const std::vector<CandidateEdge>& featureEdges, FeatureAnalysis& analysis);

/**
 * @brief Builds deterministic adjacency and packed edge attributes for tracing.
 */
TraceGraph buildTraceGraph(
    const Mesh& mesh,
    const FeatureOptions& options,
    const std::vector<CandidateEdge>& featureEdges,
    FeatureAnalysis& analysis
);

/**
 * @brief Returns the attribute record for edge (a, b), or nullptr when the trace
 * graph does not contain that edge. Hot loops should fetch this once instead
 * of calling several single-attribute helpers.
 */
const TraceEdgeAttrs* traceEdgeAttrs(const TraceGraph& trace, int a, int b);

/**
 * @name Typed trace-edge attribute queries
 * Return false/zero when the requested edge is absent.
 * @{
 */
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
/**
 * @}
 */

/**
 * @return true when the undirected edge is active in the trace graph.
 */
bool traceGraphHasEdge(const TraceGraph& trace, int a, int b);
/**
 * @brief Adds an edge and keeps adjacency/attributes/public diagnostics consistent.
 */
void addTraceGraphEdge(TraceGraph& trace, FeatureAnalysis& analysis, const CandidateEdge& edge);
/**
 * @brief Removes one active edge from adjacency and attributes.
 */
void removeTraceGraphEdge(TraceGraph& trace, int a, int b);
/**
 * @brief Rebuilds the deterministic flat edge list from active attributes.
 */
void rebuildTraceGraphEdges(TraceGraph& trace);

/**
 * @brief Recomputes public feature/junction markers after all graph mutations.
 */
void finalizeFeatureGraphMarkers(const Mesh& mesh, FeatureAnalysis& analysis);

} // namespace manumesh::feature::detector_detail
