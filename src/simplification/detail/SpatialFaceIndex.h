#pragma once

#include "common/detail/SpatialIndex.h"
#include "core/Mesh.h"
#include "detail/SimplificationTypes.h"

#include <vector>

namespace manumesh::simplification {

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
