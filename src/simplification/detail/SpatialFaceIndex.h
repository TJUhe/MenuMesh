/**
 * @file src/simplification/detail/SpatialFaceIndex.h
 * @brief Declares spatial face index facilities for ManuMesh's simplification module.
 * @ingroup manumesh_simplification
 *
 * @details This file is part of the feature-aware edge-collapse pipeline. Quadric costs rank candidates; topology, geometry, feature, boundary, error, and optional texture policies decide whether a placement may mutate the mesh.
 */

#pragma once

#include "common/detail/SpatialIndex.h"
#include "core/Mesh.h"
#include "detail/SimplificationTypes.h"

#include <vector>

namespace manumesh::simplification {

/// Dynamic broad-phase AABB grid for exact local self-intersection checks.
class SpatialFaceIndex {
public:
    void rebuild(const std::vector<FaceState>& faces, const std::vector<VertexState>& vertices);
    void removeFace(int faceId);
    void updateFace(int faceId, const FaceState& face, const std::vector<VertexState>& vertices);
    std::vector<int> query(const Vec3& lo, const Vec3& hi) const;
    bool enabled() const { return grid_.enabled(); }

private:
    void insertFace(int faceId, const FaceState& face, const std::vector<VertexState>& vertices);

    manumesh::common::UniformAabbCandidateGrid grid_;
};

} // namespace manumesh::simplification
