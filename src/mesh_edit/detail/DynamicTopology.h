/**
 * @file src/mesh_edit/detail/DynamicTopology.h
 * @brief Declares dynamic topology facilities for ManuMesh's mesh-editing module.
 * @ingroup manumesh_mesh_edit
 *
 * @details Edit-time indices remain stable while faces and vertices are marked inactive; deterministic compaction creates the final dense mesh.
 */

#pragma once

#include "common/detail/MeshQueries.h"
#include "mesh_edit/detail/MeshEditTypes.h"

#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace manumesh::mesh_edit {

/** @brief Reports whether an editable face references a vertex id. */
bool containsVertex(const EditableFace& face, int vertex);

/**
 * @brief Mutable face-incidence cache for edit algorithms that keep stable indices.
 */
struct DynamicTopology {
    std::vector<std::unordered_set<int>> vertexFaces;
    std::unordered_map<std::array<int, 3>, std::unordered_set<int>, common::FaceKeyHash> facesByKey;

    /** @brief Builds incidence caches for all initially active valid faces. */
    DynamicTopology(const std::vector<EditableFace>& faces, int vertexCount);

    /**
     * @brief Registers a face in both caches. A face referencing any out-of-range
     * vertex index is rejected as a whole (registered in neither cache), so
     * vertexFaces and facesByKey always stay consistent.
     */
    void addFace(int faceId, const EditableFace& face);
    /** @brief Removes a face from vertex incidence and duplicate-key caches. */
    void removeFace(int faceId, const EditableFace& face);
    /** @brief Reports whether another active face has the same canonical key. */
    bool hasDuplicateFace(int faceId, const EditableFace& face) const;
};

/** @brief Collects canonical undirected edges from all active faces. */
std::vector<std::pair<int, int>> collectActiveEdges(const std::vector<EditableFace>& faces);

/** @brief Reports whether two vertices share an active face edge. */
bool areAdjacent(int a, int b, const std::vector<EditableFace>& faces, const DynamicTopology& topology);

/** @brief Counts active faces incident to one undirected edge. */
int activeIncidentFaceCountForEdge(
    int a, int b, const std::vector<EditableFace>& faces, const DynamicTopology& topology
);

} // namespace manumesh::mesh_edit
