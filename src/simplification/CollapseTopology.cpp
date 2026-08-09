/**
 * @file src/simplification/CollapseTopology.cpp
 * @brief 实现 ManuMesh 的简化模块的折叠拓扑功能。
 * @ingroup manumesh_simplification
 *
 * @details 计算候选折叠对应的局部面重写和关联变化。
 * @algorithm 识别被移除的共享面，将保留面中对 remove 顶点的引用改写为 keep 顶点，规范化面键，并提供合法性检查与修改阶段所需的受影响面/顶点集合。
 * @invariants 规划阶段无副作用；应用阶段以原子方式更新两个关联缓存。
 */

#include "detail/CollapseTopology.h"

#include "common/detail/MeshQueries.h"

#include <algorithm>
#include <cstdint>
#include <unordered_set>

namespace manumesh::simplification {
namespace {

/** @brief 构成一个单纯形活动链接的顶点集合和边集合。*/
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

} // 结束匿名命名空间

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
    // 排序后再更新下游队列和计算质心，使结果不依赖 unordered_set 的遍历顺序。
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

    // 用虚拟边界顶点封闭孤立开放三角形会形成四面体。在仅含三角形的网格中，折叠其任意真实边都会删除整个二维连通分量。
    if (incidentFaceCount == 1 && isIsolatedOpenTriangleEdge(keep, remove, *edgeLink.begin(), faces, topology)) {
        return false;
    }

    // 开放边界网格的扩展链接条件（Hoppe 等人的 Progressive Meshes）：用连接到每个边界顶点的虚拟顶点封闭曲面。若两个端点都在边界上但边本身位于内部（有两个相邻面），则虚拟顶点同时出现在两个顶点链接中，却不在边链接中；折叠这条边界弦会把曲面挤压成非流形顶点。
    if (incidentFaceCount == 2 && vertices[keep].isBoundary && vertices[remove].isBoundary) {
        return false;
    }

    const SimplicialLink keepLink = activeLinkOf(keep, faces, vertices, topology);
    const SimplicialLink removeLink = activeLinkOf(remove, faces, vertices, topology);
    return vertexIntersectionEqualsEdgeLink(keepLink, removeLink, edgeLink) &&
           !endpointLinksShareEdge(keepLink, removeLink);
}

} // 结束 manumesh::simplification 命名空间
