/**
 * @file src/simplification/SpatialFaceIndex.cpp
 * @brief 实现 ManuMesh 的简化模块的空间面索引功能。
 * @ingroup manumesh_simplification
 *
 * @details 在局部编辑期间维护活动面的宽相位 AABB 登记。
 * @algorithm 均匀网格会在插入前移除过期登记，只更新受影响的面，并返回去重后的潜在相交候选，供精确三角形谓词检查。
 */

#include "detail/SpatialFaceIndex.h"

#include "common/detail/GeometryPredicates.h"

#include <limits>

namespace manumesh {
namespace simplification {

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
    const std::pair<Vec3, Vec3> bounds = manumesh::common::triangleAabb(tri);
    const Vec3& lo = bounds.first;
    const Vec3& hi = bounds.second;
    grid_.insert(faceId, lo, hi);
}

} // namespace simplification
} // namespace manumesh
