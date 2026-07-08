#include "detail/FeatureGraph.h"

#include "common/detail/MeshQueries.h"

#include <algorithm>

namespace manumesh::feature::detector_detail {

void initializeFeatureGraph(const std::vector<CandidateEdge>& featureEdges,
                            FeatureAnalysis& analysis) {
  analysis.graph.vertices.assign(analysis.vertices.size(), FeatureGraphVertex{});
  analysis.graph.edges.reserve(featureEdges.size());
  for (const CandidateEdge& edge : featureEdges) {
    FeatureGraphEdge graphEdge;
    graphEdge.a = edge.a;
    graphEdge.b = edge.b;
    graphEdge.boundary = edge.boundary;
    graphEdge.dihedral = edge.dihedral;
    graphEdge.normalTensor = edge.normalTensor;
    graphEdge.nonManifold = edge.nonManifold;
    graphEdge.signedKind = edge.signedKind;
    const int edgeId = static_cast<int>(analysis.graph.edges.size());
    analysis.graph.edges.push_back(graphEdge);
    if (edge.a >= 0 && edge.a < static_cast<int>(analysis.graph.vertices.size())) {
      analysis.graph.vertices[edge.a].incidentEdges.push_back(edgeId);
    }
    if (edge.b >= 0 && edge.b < static_cast<int>(analysis.graph.vertices.size())) {
      analysis.graph.vertices[edge.b].incidentEdges.push_back(edgeId);
    }
  }
}

TraceGraph buildTraceGraph(const Mesh& mesh, const FeatureOptions& options,
                           const std::vector<CandidateEdge>& featureEdges) {
  TraceGraph trace;
  trace.adjacency.resize(mesh.vertices.size());
  trace.traceVertex.assign(mesh.vertices.size(), 0);
  trace.edgeIsBoundary.reserve(featureEdges.size());
  trace.edgeSignedKind.reserve(featureEdges.size());
  trace.graphEdges.reserve(featureEdges.size());

  const double loopTraceAngle =
      std::max(options.featureAngleDeg * kPi / 180.0, 40.0 * kPi / 180.0);
  for (const CandidateEdge& edge : featureEdges) {
    const bool traceEdge = edge.boundary || edge.nonManifold || edge.normalTensor ||
                           (edge.dihedral && edge.angleRad >= loopTraceAngle);
    if (!traceEdge) {
      continue;
    }
    trace.adjacency[edge.a].push_back(edge.b);
    trace.adjacency[edge.b].push_back(edge.a);
    trace.traceVertex[edge.a] = 1;
    trace.traceVertex[edge.b] = 1;
    trace.edgeIsBoundary[manumesh::detail::meshEdgeKey(edge.a, edge.b)] = edge.boundary;
    trace.edgeSignedKind[manumesh::detail::meshEdgeKey(edge.a, edge.b)] =
        edge.signedKind;
    trace.graphEdges.emplace_back(edge.a, edge.b);
  }
  return trace;
}

bool traceEdgeBoundary(const TraceGraph& trace, int a, int b) {
  const auto it = trace.edgeIsBoundary.find(manumesh::detail::meshEdgeKey(a, b));
  return it != trace.edgeIsBoundary.end() && it->second;
}

int traceEdgeSign(const TraceGraph& trace, int a, int b) {
  const auto it = trace.edgeSignedKind.find(manumesh::detail::meshEdgeKey(a, b));
  return it == trace.edgeSignedKind.end() ? 0 : it->second;
}

void finalizeFeatureGraphMarkers(FeatureAnalysis& analysis) {
  analysis.graph.junctionVertices.clear();
  analysis.graph.sharedVertices.clear();
  for (int id = 0; id < static_cast<int>(analysis.graph.vertices.size()); ++id) {
    FeatureGraphVertex& vertex = analysis.graph.vertices[id];
    vertex.junction = vertex.incidentEdges.size() != 2 || vertex.loopIds.size() > 1 ||
                      (id < static_cast<int>(analysis.vertices.size()) &&
                       analysis.vertices[id].junction);
    vertex.shared = vertex.loopIds.size() > 1;
    if (vertex.junction && !vertex.incidentEdges.empty()) {
      analysis.graph.junctionVertices.push_back(id);
    }
    if (vertex.shared) {
      analysis.graph.sharedVertices.push_back(id);
    }
  }
}

} // namespace manumesh::feature::detector_detail
