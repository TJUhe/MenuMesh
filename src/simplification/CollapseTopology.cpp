/**
 * @file src/simplification/CollapseTopology.cpp
 * @brief Implements collapse topology facilities for ManuMesh's simplification module.
 * @ingroup manumesh_simplification
 *
 * @details Computes local face rewrites and incidence changes for a proposed collapse.
 * @algorithm Identifies removed shared faces, rewrites surviving remove-vertex
 * incidence to the kept vertex, canonicalizes face keys, and provides the
 * affected face/vertex sets consumed by legality and mutation stages.
 * @invariants Planning is side-effect free; application updates both incidence caches atomically.
 */

#include "detail/CollapseTopology.h"

#include "common/detail/MeshQueries.h"

#include <algorithm>
#include <cstdint>
#include <unordered_set>

namespace manumesh::simplification {
namespace {

/** @brief Vertex and edge sets forming the active link of a simplex. */
struct SimplicialLink {
    std::unordered_set<int> vertices;
    std::unordered_set<std::uint64_t> edges;
};

bool isActiveVertex(int vertex, const std::vector<VertexState>& vertices) {
    return vertex >= 0 && vertex < static_cast<int>(vertices.size()) && vertices[vertex].active;
}

SimplicialLink activeLinkOf(
    int vertex,
    const std::vector<FaceState>& faces,
    const std::vector<VertexState>& vertices,
    const DynamicTopology& topology
) {
    SimplicialLink link;
    if (!isActiveVertex(vertex, vertices) || vertex >= static_cast<int>(topology.vertexFaces.size())) {
        return link;
    }

    for (int faceId : topology.vertexFaces[vertex]) {
        if (faceId < 0 || faceId >= static_cast<int>(faces.size())) {
            continue;
        }
        const FaceState& face = faces[faceId];
        if (!face.active || !containsVertex(face, vertex)) {
            continue;
        }

        int opposite[2] = {-1, -1};
        int oppositeCount = 0;
        for (int neighbor : face.v) {
            if (neighbor == vertex || !isActiveVertex(neighbor, vertices)) {
                continue;
            }
            link.vertices.insert(neighbor);
            if (oppositeCount < 2) {
                opposite[oppositeCount++] = neighbor;
            }
        }
        if (oppositeCount == 2 && opposite[0] != opposite[1]) {
            link.edges.insert(common::meshEdgeKey(opposite[0], opposite[1]));
        }
    }
    return link;
}

bool vertexIntersectionEqualsEdgeLink(
    const SimplicialLink& keepLink, const SimplicialLink& removeLink, const std::unordered_set<int>& edgeLink
) {
    std::size_t intersectionSize = 0;
    const auto& smaller =
        keepLink.vertices.size() <= removeLink.vertices.size() ? keepLink.vertices : removeLink.vertices;
    const auto& larger =
        keepLink.vertices.size() <= removeLink.vertices.size() ? removeLink.vertices : keepLink.vertices;
    for (int vertex : smaller) {
        if (larger.find(vertex) == larger.end()) {
            continue;
        }
        ++intersectionSize;
        if (edgeLink.find(vertex) == edgeLink.end()) {
            return false;
        }
    }
    return intersectionSize == edgeLink.size();
}

bool endpointLinksShareEdge(const SimplicialLink& keepLink, const SimplicialLink& removeLink) {
    const auto& smaller = keepLink.edges.size() <= removeLink.edges.size() ? keepLink.edges : removeLink.edges;
    const auto& larger = keepLink.edges.size() <= removeLink.edges.size() ? removeLink.edges : keepLink.edges;
    for (std::uint64_t edge : smaller) {
        if (larger.find(edge) != larger.end()) {
            return true;
        }
    }
    return false;
}

bool isIsolatedOpenTriangleEdge(
    int keep, int remove, int opposite, const std::vector<FaceState>& faces, const DynamicTopology& topology
) {
    return activeIncidentFaceCountForEdge(keep, opposite, faces, topology) == 1 &&
           activeIncidentFaceCountForEdge(remove, opposite, faces, topology) == 1;
}

} // namespace

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

bool collapseWouldPreserveLinkCondition(
    int keep,
    int remove,
    const std::vector<FaceState>& faces,
    const std::vector<VertexState>& vertices,
    const DynamicTopology& topology
) {
    if (keep == remove || !isActiveVertex(keep, vertices) || !isActiveVertex(remove, vertices) ||
        keep >= static_cast<int>(topology.vertexFaces.size()) ||
        remove >= static_cast<int>(topology.vertexFaces.size())) {
        return false;
    }

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
        if (faceId < 0 || faceId >= static_cast<int>(faces.size())) {
            continue;
        }
        const FaceState& face = faces[faceId];
        if (!face.active || !containsVertex(face, keep) || !containsVertex(face, remove)) {
            continue;
        }
        ++incidentFaceCount;
        for (int vertex : face.v) {
            if (vertex != keep && vertex != remove && isActiveVertex(vertex, vertices)) {
                edgeLink.insert(vertex);
            }
        }
    }

    if (incidentFaceCount <= 0 || incidentFaceCount > 2 ||
        edgeLink.size() != static_cast<std::size_t>(incidentFaceCount)) {
        return false;
    }

    // Capping an isolated open triangle with the virtual boundary vertex
    // produces a tetrahedron. Collapsing any of its real edges would otherwise
    // erase the entire two-dimensional component in the triangle-only mesh.
    if (incidentFaceCount == 1 && isIsolatedOpenTriangleEdge(keep, remove, *edgeLink.begin(), faces, topology)) {
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

    const SimplicialLink keepLink = activeLinkOf(keep, faces, vertices, topology);
    const SimplicialLink removeLink = activeLinkOf(remove, faces, vertices, topology);
    return vertexIntersectionEqualsEdgeLink(keepLink, removeLink, edgeLink) &&
           !endpointLinksShareEdge(keepLink, removeLink);
}

} // namespace manumesh::simplification
