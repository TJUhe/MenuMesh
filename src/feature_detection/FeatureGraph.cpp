#include "detail/FeatureGraph.h"

#include "common/detail/MeshQueries.h"

#include <algorithm>

namespace manumesh::feature::detector_detail {
namespace {

void removeNeighbor(std::vector<int>& neighbors, int id) {
  neighbors.erase(std::remove(neighbors.begin(), neighbors.end(), id), neighbors.end());
}

void appendFeatureGraphEdge(FeatureAnalysis& analysis, const CandidateEdge& edge) {
  FeatureGraphEdge graphEdge;
  graphEdge.a = edge.a;
  graphEdge.b = edge.b;
  graphEdge.boundary = edge.boundary;
  graphEdge.dihedral = edge.dihedral;
  graphEdge.normalTensor = edge.normalTensor;
  graphEdge.nonManifold = edge.nonManifold;
  graphEdge.cleanupBridge = edge.cleanupBridge;
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

void addTraceGraphStorage(TraceGraph& trace, const CandidateEdge& edge) {
  if (edge.a < 0 || edge.b < 0 || edge.a == edge.b ||
      edge.a >= static_cast<int>(trace.adjacency.size()) ||
      edge.b >= static_cast<int>(trace.adjacency.size()) ||
      traceGraphHasEdge(trace, edge.a, edge.b)) {
    return;
  }
  trace.adjacency[edge.a].push_back(edge.b);
  trace.adjacency[edge.b].push_back(edge.a);
  trace.traceVertex[edge.a] = 1;
  trace.traceVertex[edge.b] = 1;
  const std::uint64_t key = manumesh::detail::meshEdgeKey(edge.a, edge.b);
  trace.edgeIsBoundary[key] = edge.boundary;
  trace.edgeIsDihedral[key] = edge.dihedral;
  trace.edgeIsNormalTensor[key] = edge.normalTensor;
  trace.edgeIsNonManifold[key] = edge.nonManifold;
  trace.edgeIsCleanupBridge[key] = edge.cleanupBridge;
  trace.edgeSignedKind[key] = edge.signedKind;
  trace.edgeTensorPersistence[key] = edge.tensorPersistentScore;
  trace.edgeTensorPersistentScales[key] = edge.tensorPersistentScales;
  trace.graphEdges.emplace_back(edge.a, edge.b);
}

} // namespace

void initializeFeatureGraph(const std::vector<CandidateEdge>& featureEdges,
                            FeatureAnalysis& analysis) {
  analysis.graph.vertices.assign(analysis.vertices.size(), FeatureGraphVertex{});
  analysis.graph.edges.reserve(featureEdges.size());
  for (const CandidateEdge& edge : featureEdges) {
    appendFeatureGraphEdge(analysis, edge);
  }
}

TraceGraph buildTraceGraph(const Mesh& mesh, const FeatureOptions& options,
                           const std::vector<CandidateEdge>& featureEdges,
                           FeatureAnalysis& analysis) {
  TraceGraph trace;
  trace.adjacency.resize(mesh.vertices.size());
  trace.traceVertex.assign(mesh.vertices.size(), 0);
  trace.edgeIsBoundary.reserve(featureEdges.size());
  trace.edgeIsDihedral.reserve(featureEdges.size());
  trace.edgeIsNormalTensor.reserve(featureEdges.size());
  trace.edgeIsNonManifold.reserve(featureEdges.size());
  trace.edgeIsCleanupBridge.reserve(featureEdges.size());
  trace.edgeSignedKind.reserve(featureEdges.size());
  trace.edgeTensorPersistence.reserve(featureEdges.size());
  trace.edgeTensorPersistentScales.reserve(featureEdges.size());
  trace.graphEdges.reserve(featureEdges.size());

  const double traceAngleDeg = options.loopTraceAngleDeg < 0.0
                                   ? options.featureAngleDeg
                                   : options.loopTraceAngleDeg;
  const double loopTraceAngle = traceAngleDeg * kPi / 180.0;
  for (const CandidateEdge& edge : featureEdges) {
    const bool traceEdge = edge.boundary || edge.nonManifold || edge.normalTensor ||
                           (edge.dihedral && edge.angleRad >= loopTraceAngle);
    if (!traceEdge) {
      ++analysis.untracedFeatureEdges;
      continue;
    }
    ++analysis.tracedFeatureEdges;
    addTraceGraphStorage(trace, edge);
  }
  return trace;
}

bool traceEdgeBoundary(const TraceGraph& trace, int a, int b) {
  const auto it = trace.edgeIsBoundary.find(manumesh::detail::meshEdgeKey(a, b));
  return it != trace.edgeIsBoundary.end() && it->second;
}

bool traceEdgeDihedral(const TraceGraph& trace, int a, int b) {
  const auto it = trace.edgeIsDihedral.find(manumesh::detail::meshEdgeKey(a, b));
  return it != trace.edgeIsDihedral.end() && it->second;
}

bool traceEdgeNormalTensor(const TraceGraph& trace, int a, int b) {
  const auto it = trace.edgeIsNormalTensor.find(manumesh::detail::meshEdgeKey(a, b));
  return it != trace.edgeIsNormalTensor.end() && it->second;
}

bool traceEdgeNonManifold(const TraceGraph& trace, int a, int b) {
  const auto it = trace.edgeIsNonManifold.find(manumesh::detail::meshEdgeKey(a, b));
  return it != trace.edgeIsNonManifold.end() && it->second;
}

bool traceEdgeCleanupBridge(const TraceGraph& trace, int a, int b) {
  const auto it = trace.edgeIsCleanupBridge.find(manumesh::detail::meshEdgeKey(a, b));
  return it != trace.edgeIsCleanupBridge.end() && it->second;
}

int traceEdgeSign(const TraceGraph& trace, int a, int b) {
  const auto it = trace.edgeSignedKind.find(manumesh::detail::meshEdgeKey(a, b));
  return it == trace.edgeSignedKind.end() ? 0 : it->second;
}

double traceEdgeTensorPersistence(const TraceGraph& trace, int a, int b) {
  const auto it = trace.edgeTensorPersistence.find(manumesh::detail::meshEdgeKey(a, b));
  return it == trace.edgeTensorPersistence.end() ? 0.0 : it->second;
}

int traceEdgeTensorPersistentScales(const TraceGraph& trace, int a, int b) {
  const auto it =
      trace.edgeTensorPersistentScales.find(manumesh::detail::meshEdgeKey(a, b));
  return it == trace.edgeTensorPersistentScales.end() ? 0 : it->second;
}

bool traceGraphHasEdge(const TraceGraph& trace, int a, int b) {
  if (a < 0 || b < 0 || a >= static_cast<int>(trace.adjacency.size()) ||
      b >= static_cast<int>(trace.adjacency.size())) {
    return false;
  }
  const std::vector<int>& neighbors = trace.adjacency[a];
  return std::find(neighbors.begin(), neighbors.end(), b) != neighbors.end();
}

void addTraceGraphEdge(TraceGraph& trace, FeatureAnalysis& analysis,
                       const CandidateEdge& edge) {
  if (edge.a < 0 || edge.b < 0 || edge.a == edge.b ||
      edge.a >= static_cast<int>(trace.adjacency.size()) ||
      edge.b >= static_cast<int>(trace.adjacency.size()) ||
      traceGraphHasEdge(trace, edge.a, edge.b)) {
    return;
  }
  addTraceGraphStorage(trace, edge);
  appendFeatureGraphEdge(analysis, edge);
}

void removeTraceGraphEdge(TraceGraph& trace, int a, int b) {
  if (!traceGraphHasEdge(trace, a, b)) {
    return;
  }
  removeNeighbor(trace.adjacency[a], b);
  removeNeighbor(trace.adjacency[b], a);
  trace.traceVertex[a] = trace.adjacency[a].empty() ? 0 : 1;
  trace.traceVertex[b] = trace.adjacency[b].empty() ? 0 : 1;
  const std::uint64_t key = manumesh::detail::meshEdgeKey(a, b);
  trace.edgeIsBoundary.erase(key);
  trace.edgeIsDihedral.erase(key);
  trace.edgeIsNormalTensor.erase(key);
  trace.edgeIsNonManifold.erase(key);
  trace.edgeIsCleanupBridge.erase(key);
  trace.edgeSignedKind.erase(key);
  trace.edgeTensorPersistence.erase(key);
  trace.edgeTensorPersistentScales.erase(key);
}

void rebuildTraceGraphEdges(TraceGraph& trace) {
  trace.graphEdges.clear();
  for (int v = 0; v < static_cast<int>(trace.adjacency.size()); ++v) {
    std::sort(trace.adjacency[v].begin(), trace.adjacency[v].end());
    trace.adjacency[v].erase(
        std::unique(trace.adjacency[v].begin(), trace.adjacency[v].end()),
        trace.adjacency[v].end());
    trace.traceVertex[v] = trace.adjacency[v].empty() ? 0 : 1;
    for (int nb : trace.adjacency[v]) {
      if (v < nb) {
        trace.graphEdges.emplace_back(v, nb);
      }
    }
  }
}

void finalizeFeatureGraphMarkers(FeatureAnalysis& analysis) {
  analysis.graph.junctionVertices.clear();
  analysis.graph.sharedVertices.clear();
  for (int id = 0; id < static_cast<int>(analysis.graph.vertices.size()); ++id) {
    FeatureGraphVertex& vertex = analysis.graph.vertices[id];
    const int activeIncidentEdges = static_cast<int>(std::count_if(
        vertex.incidentEdges.begin(), vertex.incidentEdges.end(), [&](int edgeId) {
          return edgeId >= 0 &&
                 edgeId < static_cast<int>(analysis.graph.edges.size()) &&
                 !analysis.graph.edges[edgeId].removedByCleanup;
        }));
    vertex.junction = activeIncidentEdges != 2 || vertex.loopIds.size() > 1 ||
                      (id < static_cast<int>(analysis.vertices.size()) &&
                       analysis.vertices[id].junction);
    vertex.shared = vertex.loopIds.size() > 1;
    if (vertex.junction && activeIncidentEdges > 0) {
      analysis.graph.junctionVertices.push_back(id);
    }
    if (vertex.shared) {
      analysis.graph.sharedVertices.push_back(id);
    }
  }
}

} // namespace manumesh::feature::detector_detail
