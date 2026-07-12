#include "mesh_edit/detail/DynamicTopology.h"

#include <algorithm>
#include <cstdint>

namespace manumesh::mesh_edit {

bool containsVertex(const EditableFace& face, int vertex) {
    return face.v[0] == vertex || face.v[1] == vertex || face.v[2] == vertex;
}

DynamicTopology::DynamicTopology(const std::vector<EditableFace>& faces, int vertexCount) {
    vertexFaces.resize(vertexCount);
    for (int faceId = 0; faceId < static_cast<int>(faces.size()); ++faceId) {
        if (faces[faceId].active) {
            addFace(faceId, faces[faceId]);
        }
    }
}

void DynamicTopology::addFace(int faceId, const EditableFace& face) {
    // Reject faces with any out-of-range vertex entirely so vertexFaces and
    // facesByKey never disagree about which faces are registered.
    for (int vertex : face.v) {
        if (vertex < 0 || vertex >= static_cast<int>(vertexFaces.size())) {
            return;
        }
    }
    for (int vertex : face.v) {
        vertexFaces[vertex].insert(faceId);
    }
    facesByKey[common::sortedFaceKey(face.v)].insert(faceId);
}

void DynamicTopology::removeFace(int faceId, const EditableFace& face) {
    for (int vertex : face.v) {
        if (vertex >= 0 && vertex < static_cast<int>(vertexFaces.size())) {
            vertexFaces[vertex].erase(faceId);
        }
    }
    const auto key = common::sortedFaceKey(face.v);
    auto it = facesByKey.find(key);
    if (it == facesByKey.end()) {
        return;
    }
    it->second.erase(faceId);
    if (it->second.empty()) {
        facesByKey.erase(it);
    }
}

bool DynamicTopology::hasDuplicateFace(int faceId, const EditableFace& face) const {
    const auto it = facesByKey.find(common::sortedFaceKey(face.v));
    if (it == facesByKey.end()) {
        return false;
    }
    for (int otherFaceId : it->second) {
        if (otherFaceId != faceId) {
            return true;
        }
    }
    return false;
}

std::vector<std::pair<int, int>> collectActiveEdges(const std::vector<EditableFace>& faces) {
    std::unordered_set<std::uint64_t> seen;
    std::vector<std::pair<int, int>> edges;
    for (const EditableFace& face : faces) {
        if (!face.active) {
            continue;
        }
        for (int edge = 0; edge < 3; ++edge) {
            int a = face.v[edge];
            int b = face.v[(edge + 1) % 3];
            if (a == b) {
                continue;
            }
            const std::uint64_t key = common::meshEdgeKey(a, b);
            if (seen.insert(key).second) {
                if (a > b) {
                    std::swap(a, b);
                }
                edges.emplace_back(a, b);
            }
        }
    }
    return edges;
}

bool areAdjacent(int a, int b, const std::vector<EditableFace>& faces, const DynamicTopology& topology) {
    if (a < 0 || b < 0 || a >= static_cast<int>(topology.vertexFaces.size()) ||
        b >= static_cast<int>(topology.vertexFaces.size())) {
        return false;
    }
    const auto& aFaces = topology.vertexFaces[a];
    const auto& bFaces = topology.vertexFaces[b];
    const auto& smaller = aFaces.size() <= bFaces.size() ? aFaces : bFaces;
    const auto& larger = aFaces.size() <= bFaces.size() ? bFaces : aFaces;
    for (int faceId : smaller) {
        if (larger.find(faceId) != larger.end() && faces[faceId].active) {
            return true;
        }
    }
    return false;
}

int activeIncidentFaceCountForEdge(
    int a, int b, const std::vector<EditableFace>& faces, const DynamicTopology& topology
) {
    if (a < 0 || b < 0 || a >= static_cast<int>(topology.vertexFaces.size()) ||
        b >= static_cast<int>(topology.vertexFaces.size())) {
        return 0;
    }
    const auto& aFaces = topology.vertexFaces[a];
    const auto& bFaces = topology.vertexFaces[b];
    const auto& smaller = aFaces.size() <= bFaces.size() ? aFaces : bFaces;
    const auto& larger = aFaces.size() <= bFaces.size() ? bFaces : aFaces;
    int count = 0;
    for (int faceId : smaller) {
        if (larger.find(faceId) != larger.end() && faces[faceId].active) {
            ++count;
        }
    }
    return count;
}

} // namespace manumesh::mesh_edit
