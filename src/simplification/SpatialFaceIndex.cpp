/**
 * @file src/simplification/SpatialFaceIndex.cpp
 * @brief Implements spatial face index facilities for ManuMesh's simplification module.
 * @ingroup manumesh_simplification
 *
 * @details Maintains broad-phase AABB registrations for active faces during local edits.
 * @algorithm The uniform grid removes stale registrations before insertion,
 * updates only affected faces, and returns deduplicated potential intersection
 * candidates for exact triangle predicates.
 */

#include "detail/SpatialFaceIndex.h"

#include "common/detail/GeometryPredicates.h"

#include <limits>

namespace manumesh::simplification {

void SpatialFaceIndex::rebuild(const std::vector<FaceState>& faces, const std::vector<VertexState>& vertices) {
    grid_.clear();
    if (faces.empty() || vertices.empty()) {
        return;
    }

    Vec3 lo = Vec3::Constant(std::numeric_limits<double>::infinity());
    Vec3 hi = Vec3::Constant(-std::numeric_limits<double>::infinity());
    int activeFaces = 0;
    for (const FaceState& face : faces) {
        if (!face.active) {
            continue;
        }
        ++activeFaces;
        for (int id : face.v) {
            lo = lo.cwiseMin(vertices[id].p);
            hi = hi.cwiseMax(vertices[id].p);
        }
    }
    if (activeFaces <= 0 || !lo.allFinite() || !hi.allFinite()) {
        return;
    }

    grid_.reset(lo, hi, activeFaces);
    if (!grid_.enabled()) {
        return;
    }

    for (int fi = 0; fi < static_cast<int>(faces.size()); ++fi) {
        if (faces[fi].active) {
            insertFace(fi, faces[fi], vertices);
        }
    }
}

void SpatialFaceIndex::removeFace(int faceId) { grid_.remove(faceId); }

void SpatialFaceIndex::updateFace(int faceId, const FaceState& face, const std::vector<VertexState>& vertices) {
    removeFace(faceId);
    if (face.active) {
        insertFace(faceId, face, vertices);
    }
}

std::vector<int> SpatialFaceIndex::query(const Vec3& lo, const Vec3& hi) const { return grid_.queryCandidates(lo, hi); }

void SpatialFaceIndex::insertFace(int faceId, const FaceState& face, const std::vector<VertexState>& vertices) {
    const std::array<Vec3, 3> tri = {vertices[face.v[0]].p, vertices[face.v[1]].p, vertices[face.v[2]].p};
    const auto [lo, hi] = manumesh::common::triangleAabb(tri);
    grid_.insert(faceId, lo, hi);
}

} // namespace manumesh::simplification
