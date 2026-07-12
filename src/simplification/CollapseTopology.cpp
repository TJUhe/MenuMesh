#include "detail/CollapseTopology.h"

#include <algorithm>

namespace manumesh::simplification {

BoundaryCollapseDecision boundaryCollapseDecision(const BoundaryCollapseInput& input) {
    const int keep = input.edge.keep;
    const int remove = input.edge.remove;
    if (!input.options.preserveBoundary) {
        return {};
    }

    const bool keepBoundary = input.vertices[keep].isBoundary;
    const bool removeBoundary = input.vertices[remove].isBoundary;
    if (!keepBoundary && !removeBoundary) {
        return {};
    }
    if (keepBoundary != removeBoundary) {
        return {false, false};
    }

    const bool boundaryEdge = activeIncidentFaceCountForEdge(keep, remove, input.faces, input.topology) == 1;
    return {boundaryEdge, boundaryEdge};
}

std::vector<int> activeNeighborsOf(
    int vertex,
    const std::vector<FaceState>& faces,
    const std::vector<VertexState>& vertices,
    const DynamicTopology& topology
) {
    std::unordered_set<int> seen;
    if (vertex < 0 || vertex >= static_cast<int>(topology.vertexFaces.size())) {
        return {};
    }
    for (int faceId : topology.vertexFaces[vertex]) {
        const FaceState& face = faces[faceId];
        if (!face.active) {
            continue;
        }
        for (int neighbor : face.v) {
            if (neighbor != vertex && vertices[neighbor].active) {
                seen.insert(neighbor);
            }
        }
    }
    // Sort so downstream queue updates and centroid sums are deterministic
    // regardless of the unordered_set iteration order.
    std::vector<int> neighbors(seen.begin(), seen.end());
    std::sort(neighbors.begin(), neighbors.end());
    return neighbors;
}

std::unordered_set<int> activeLinkOf(
    int vertex,
    const std::vector<FaceState>& faces,
    const std::vector<VertexState>& vertices,
    const DynamicTopology& topology,
    int excludedVertex
) {
    std::unordered_set<int> link;
    if (vertex < 0 || vertex >= static_cast<int>(topology.vertexFaces.size())) {
        return link;
    }
    for (int faceId : topology.vertexFaces[vertex]) {
        const FaceState& face = faces[faceId];
        if (!face.active) {
            continue;
        }
        for (int neighbor : face.v) {
            if (neighbor != vertex && neighbor != excludedVertex && vertices[neighbor].active) {
                link.insert(neighbor);
            }
        }
    }
    return link;
}

bool collapseWouldPreserveLinkCondition(
    int keep,
    int remove,
    const std::vector<FaceState>& faces,
    const std::vector<VertexState>& vertices,
    const DynamicTopology& topology
) {
    std::unordered_set<int> edgeLink;
    int incidentFaceCount = 0;
    const auto& keepFaces = topology.vertexFaces[keep];
    const auto& removeFaces = topology.vertexFaces[remove];
    const auto& smaller = keepFaces.size() <= removeFaces.size() ? keepFaces : removeFaces;
    const auto& larger = keepFaces.size() <= removeFaces.size() ? removeFaces : keepFaces;
    for (int faceId : smaller) {
        if (larger.find(faceId) == larger.end()) {
            continue;
        }
        const FaceState& face = faces[faceId];
        if (!face.active) {
            continue;
        }
        ++incidentFaceCount;
        for (int vertex : face.v) {
            if (vertex != keep && vertex != remove && vertices[vertex].active) {
                edgeLink.insert(vertex);
            }
        }
    }

    if (incidentFaceCount <= 0 || incidentFaceCount > 2 ||
        edgeLink.size() != static_cast<std::size_t>(incidentFaceCount)) {
        return false;
    }

    // Extended link condition for meshes with open boundary (Hoppe et al.,
    // Progressive Meshes): close the surface with a virtual vertex joined to
    // every boundary vertex. If both endpoints lie on the boundary but the
    // edge itself is interior (two incident faces), the virtual vertex is in
    // both vertex links yet not in the edge link, so collapsing this boundary
    // chord would pinch the surface into a non-manifold vertex.
    if (incidentFaceCount == 2 && vertices[keep].isBoundary && vertices[remove].isBoundary) {
        return false;
    }

    const std::unordered_set<int> keepLink = activeLinkOf(keep, faces, vertices, topology, remove);
    const std::unordered_set<int> removeLink = activeLinkOf(remove, faces, vertices, topology, keep);
    std::unordered_set<int> intersection;
    for (int vertex : keepLink) {
        if (removeLink.find(vertex) != removeLink.end()) {
            intersection.insert(vertex);
        }
    }

    if (intersection.size() != edgeLink.size()) {
        return false;
    }
    for (int vertex : intersection) {
        if (edgeLink.find(vertex) == edgeLink.end()) {
            return false;
        }
    }
    return true;
}

} // namespace manumesh::simplification
