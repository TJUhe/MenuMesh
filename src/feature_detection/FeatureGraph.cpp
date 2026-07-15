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
    graphEdge.smoothCurvature = edge.smoothCurvature;
    graphEdge.nonManifold = edge.nonManifold;
    graphEdge.cleanupBridge = edge.cleanupBridge;
    graphEdge.consolidationBridge = edge.consolidationBridge;
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
    if (edge.a < 0 || edge.b < 0 || edge.a == edge.b || edge.a >= static_cast<int>(trace.adjacency.size()) ||
        edge.b >= static_cast<int>(trace.adjacency.size()) || traceGraphHasEdge(trace, edge.a, edge.b)) {
        return;
    }
    trace.adjacency[edge.a].push_back(edge.b);
    trace.adjacency[edge.b].push_back(edge.a);
    trace.traceVertex[edge.a] = 1;
    trace.traceVertex[edge.b] = 1;
    const std::uint64_t key = manumesh::common::meshEdgeKey(edge.a, edge.b);
    TraceEdgeAttrs& attrs = trace.edgeAttrs[key];
    attrs.boundary = edge.boundary;
    attrs.dihedral = edge.dihedral;
    attrs.normalTensor = edge.normalTensor;
    attrs.smoothCurvature = edge.smoothCurvature;
    attrs.nonManifold = edge.nonManifold;
    attrs.cleanupBridge = edge.cleanupBridge;
    attrs.consolidationBridge = edge.consolidationBridge;
    attrs.signedKind = edge.signedKind;
    attrs.tensorPersistence = edge.tensorPersistentScore;
    attrs.tensorPersistentScales = edge.tensorPersistentScales;
    attrs.curvaturePersistence = edge.curvaturePersistentScore;
    attrs.curvaturePersistentScales = edge.curvaturePersistentScales;
    trace.graphEdges.emplace_back(edge.a, edge.b);
}

} // namespace

void initializeFeatureGraph(const std::vector<CandidateEdge>& featureEdges, FeatureAnalysis& analysis) {
    analysis.graph.vertices.assign(analysis.vertices.size(), FeatureGraphVertex{});
    analysis.graph.edges.reserve(featureEdges.size());
    for (const CandidateEdge& edge : featureEdges) {
        appendFeatureGraphEdge(analysis, edge);
    }
}

TraceGraph buildTraceGraph(
    const Mesh& mesh,
    const FeatureOptions& options,
    const std::vector<CandidateEdge>& featureEdges,
    FeatureAnalysis& analysis
) {
    TraceGraph trace;
    trace.adjacency.resize(mesh.vertices.size());
    trace.traceVertex.assign(mesh.vertices.size(), 0);
    trace.edgeAttrs.reserve(featureEdges.size());
    trace.graphEdges.reserve(featureEdges.size());

    const double traceAngleDeg = options.loopTraceAngleDeg < 0.0 ? options.featureAngleDeg : options.loopTraceAngleDeg;
    const double loopTraceAngle = traceAngleDeg * kPi / 180.0;
    for (const CandidateEdge& edge : featureEdges) {
        const bool traceEdge = edge.boundary || edge.nonManifold || edge.normalTensor || edge.smoothCurvature ||
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

const TraceEdgeAttrs* traceEdgeAttrs(const TraceGraph& trace, int a, int b) {
    const auto it = trace.edgeAttrs.find(manumesh::common::meshEdgeKey(a, b));
    return it == trace.edgeAttrs.end() ? nullptr : &it->second;
}

bool traceEdgeBoundary(const TraceGraph& trace, int a, int b) {
    const TraceEdgeAttrs* attrs = traceEdgeAttrs(trace, a, b);
    return attrs != nullptr && attrs->boundary;
}

bool traceEdgeDihedral(const TraceGraph& trace, int a, int b) {
    const TraceEdgeAttrs* attrs = traceEdgeAttrs(trace, a, b);
    return attrs != nullptr && attrs->dihedral;
}

bool traceEdgeNormalTensor(const TraceGraph& trace, int a, int b) {
    const TraceEdgeAttrs* attrs = traceEdgeAttrs(trace, a, b);
    return attrs != nullptr && attrs->normalTensor;
}

bool traceEdgeSmoothCurvature(const TraceGraph& trace, int a, int b) {
    const TraceEdgeAttrs* attrs = traceEdgeAttrs(trace, a, b);
    return attrs != nullptr && attrs->smoothCurvature;
}

bool traceEdgeNonManifold(const TraceGraph& trace, int a, int b) {
    const TraceEdgeAttrs* attrs = traceEdgeAttrs(trace, a, b);
    return attrs != nullptr && attrs->nonManifold;
}

bool traceEdgeCleanupBridge(const TraceGraph& trace, int a, int b) {
    const TraceEdgeAttrs* attrs = traceEdgeAttrs(trace, a, b);
    return attrs != nullptr && attrs->cleanupBridge;
}

int traceEdgeSign(const TraceGraph& trace, int a, int b) {
    const TraceEdgeAttrs* attrs = traceEdgeAttrs(trace, a, b);
    return attrs == nullptr ? 0 : attrs->signedKind;
}

double traceEdgeTensorPersistence(const TraceGraph& trace, int a, int b) {
    const TraceEdgeAttrs* attrs = traceEdgeAttrs(trace, a, b);
    return attrs == nullptr ? 0.0 : attrs->tensorPersistence;
}

int traceEdgeTensorPersistentScales(const TraceGraph& trace, int a, int b) {
    const TraceEdgeAttrs* attrs = traceEdgeAttrs(trace, a, b);
    return attrs == nullptr ? 0 : attrs->tensorPersistentScales;
}

double traceEdgeCurvaturePersistence(const TraceGraph& trace, int a, int b) {
    const TraceEdgeAttrs* attrs = traceEdgeAttrs(trace, a, b);
    return attrs == nullptr ? 0.0 : attrs->curvaturePersistence;
}

int traceEdgeCurvaturePersistentScales(const TraceGraph& trace, int a, int b) {
    const TraceEdgeAttrs* attrs = traceEdgeAttrs(trace, a, b);
    return attrs == nullptr ? 0 : attrs->curvaturePersistentScales;
}

bool traceGraphHasEdge(const TraceGraph& trace, int a, int b) {
    if (a < 0 || b < 0 || a >= static_cast<int>(trace.adjacency.size()) ||
        b >= static_cast<int>(trace.adjacency.size())) {
        return false;
    }
    const std::vector<int>& neighbors = trace.adjacency[a];
    return std::find(neighbors.begin(), neighbors.end(), b) != neighbors.end();
}

void addTraceGraphEdge(TraceGraph& trace, FeatureAnalysis& analysis, const CandidateEdge& edge) {
    if (edge.a < 0 || edge.b < 0 || edge.a == edge.b || edge.a >= static_cast<int>(trace.adjacency.size()) ||
        edge.b >= static_cast<int>(trace.adjacency.size()) || traceGraphHasEdge(trace, edge.a, edge.b)) {
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
    trace.edgeAttrs.erase(manumesh::common::meshEdgeKey(a, b));
}

void rebuildTraceGraphEdges(TraceGraph& trace) {
    trace.graphEdges.clear();
    for (int v = 0; v < static_cast<int>(trace.adjacency.size()); ++v) {
        std::sort(trace.adjacency[v].begin(), trace.adjacency[v].end());
        trace.adjacency[v].erase(
            std::unique(trace.adjacency[v].begin(), trace.adjacency[v].end()), trace.adjacency[v].end()
        );
        trace.traceVertex[v] = trace.adjacency[v].empty() ? 0 : 1;
        for (int nb : trace.adjacency[v]) {
            if (v < nb) {
                trace.graphEdges.emplace_back(v, nb);
            }
        }
    }
}

void finalizeFeatureGraphMarkers(const Mesh& mesh, FeatureAnalysis& analysis) {
    analysis.graph.junctionVertices.clear();
    analysis.graph.sharedVertices.clear();
    analysis.graph.endpointVertices.clear();
    analysis.junctionBranchPairs = 0;
    analysis.ambiguousJunctions = 0;
    for (int id = 0; id < static_cast<int>(analysis.graph.vertices.size()); ++id) {
        FeatureGraphVertex& vertex = analysis.graph.vertices[id];
        vertex.branches.clear();
        vertex.branchPairs.clear();
        vertex.ambiguousJunction = false;
        const int activeIncidentEdges =
            static_cast<int>(std::count_if(vertex.incidentEdges.begin(), vertex.incidentEdges.end(), [&](int edgeId) {
                return edgeId >= 0 && edgeId < static_cast<int>(analysis.graph.edges.size()) &&
                       !analysis.graph.edges[edgeId].removedByCleanup;
            }));
        // Topological junction = graph branch point (active valence > 2),
        // following M007: a junction is identified by ridge valence >= 3,
        // and its local differential quantities may be entirely trivial.
        // Neither loop membership nor the per-vertex protection flag
        // (VertexFeature::junction, which deliberately also pins vertices
        // shared between overlapping recovered loops) is evidence of a
        // branch: recovered loops routinely overlap along whole valence-2
        // chains, while loops that genuinely cross always meet at a vertex
        // with more than two incident feature edges.
        vertex.junction = activeIncidentEdges > 2;
        vertex.shared = vertex.loopIds.size() > 1;
        vertex.endpoint = activeIncidentEdges == 1;

        for (int edgeId : vertex.incidentEdges) {
            if (edgeId < 0 || edgeId >= static_cast<int>(analysis.graph.edges.size())) {
                continue;
            }
            const FeatureGraphEdge& edge = analysis.graph.edges[edgeId];
            if (edge.removedByCleanup) {
                continue;
            }
            const int neighbor = edge.a == id ? edge.b : edge.b == id ? edge.a : -1;
            if (neighbor < 0 || id >= static_cast<int>(mesh.vertices.size()) ||
                neighbor >= static_cast<int>(mesh.vertices.size())) {
                continue;
            }
            Vec3 tangent = mesh.vertices[neighbor] - mesh.vertices[id];
            if (tangent.squaredNorm() <= 1e-30) {
                continue;
            }
            tangent.normalize();
            vertex.branches.push_back(FeatureGraphBranch{edgeId, neighbor, tangent, edge.signedKind});
        }

        if (vertex.junction) {
            struct PairCandidate {
                int first = -1;
                int second = -1;
                double alignment = 0.0;
            };
            std::vector<PairCandidate> candidates;
            for (int first = 0; first < static_cast<int>(vertex.branches.size()); ++first) {
                for (int second = first + 1; second < static_cast<int>(vertex.branches.size()); ++second) {
                    const FeatureGraphBranch& lhs = vertex.branches[first];
                    const FeatureGraphBranch& rhs = vertex.branches[second];
                    if (lhs.signedKind != 0 && rhs.signedKind != 0 && lhs.signedKind != rhs.signedKind) {
                        continue;
                    }
                    // Branch tangents point away from the junction, so a true
                    // continuation is anti-parallel. Using |dot| would also
                    // pair two nearly coincident branches leaving on the same
                    // side of the junction.
                    const double alignment = -lhs.tangent.dot(rhs.tangent);
                    if (alignment >= 0.65) {
                        candidates.push_back({first, second, alignment});
                    }
                }
            }
            std::sort(candidates.begin(), candidates.end(), [](const PairCandidate& lhs, const PairCandidate& rhs) {
                if (lhs.alignment != rhs.alignment) {
                    return lhs.alignment > rhs.alignment;
                }
                return lhs.first != rhs.first ? lhs.first < rhs.first : lhs.second < rhs.second;
            });
            std::vector<char> paired(vertex.branches.size(), 0);
            for (const PairCandidate& candidate : candidates) {
                if (paired[candidate.first] || paired[candidate.second]) {
                    continue;
                }
                paired[candidate.first] = 1;
                paired[candidate.second] = 1;
                vertex.branchPairs.push_back(
                    FeatureGraphBranchPair{candidate.first, candidate.second, candidate.alignment}
                );
            }
            analysis.junctionBranchPairs += static_cast<int>(vertex.branchPairs.size());
            const int unpairedBranches =
                static_cast<int>(vertex.branches.size()) - 2 * static_cast<int>(vertex.branchPairs.size());
            vertex.ambiguousJunction = unpairedBranches > 1;
            if (vertex.ambiguousJunction) {
                ++analysis.ambiguousJunctions;
            }
        }
        if (vertex.junction && activeIncidentEdges > 0) {
            analysis.graph.junctionVertices.push_back(id);
        }
        if (vertex.shared) {
            analysis.graph.sharedVertices.push_back(id);
        }
        if (vertex.endpoint) {
            analysis.graph.endpointVertices.push_back(id);
        }
    }
}

} // namespace manumesh::feature::detector_detail
