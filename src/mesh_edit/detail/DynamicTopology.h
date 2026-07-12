#pragma once

#include "common/detail/MeshQueries.h"
#include "mesh_edit/detail/MeshEditTypes.h"

#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace manumesh::mesh_edit {

bool containsVertex(const EditableFace& face, int vertex);

/// Mutable face-incidence cache for edit algorithms that keep stable indices.
struct DynamicTopology {
    std::vector<std::unordered_set<int>> vertexFaces;
    std::unordered_map<std::array<int, 3>, std::unordered_set<int>, common::FaceKeyHash> facesByKey;

    DynamicTopology(const std::vector<EditableFace>& faces, int vertexCount);

    /// Registers a face in both caches. A face referencing any out-of-range
    /// vertex index is rejected as a whole (registered in neither cache), so
    /// vertexFaces and facesByKey always stay consistent.
    void addFace(int faceId, const EditableFace& face);
    void removeFace(int faceId, const EditableFace& face);
    bool hasDuplicateFace(int faceId, const EditableFace& face) const;
};

std::vector<std::pair<int, int>> collectActiveEdges(const std::vector<EditableFace>& faces);

bool areAdjacent(int a, int b, const std::vector<EditableFace>& faces, const DynamicTopology& topology);

int activeIncidentFaceCountForEdge(
    int a, int b, const std::vector<EditableFace>& faces, const DynamicTopology& topology
);

} // namespace manumesh::mesh_edit
