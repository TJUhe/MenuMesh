#pragma once

#include "core/Mesh.h"
#include "detail/SimplificationTypes.h"

#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace manumesh::simplification {

class SpatialFaceIndex {
public:
  void rebuild(const std::vector<FaceState>& faces,
               const std::vector<VertexState>& vertices);
  void removeFace(int faceId);
  void updateFace(int faceId, const FaceState& face,
                  const std::vector<VertexState>& vertices);
  std::vector<int> query(const Vec3& lo, const Vec3& hi) const;
  bool enabled() const { return enabled_; }

private:
  void insertFace(int faceId, const FaceState& face,
                  const std::vector<VertexState>& vertices);
  CellCoord coordFor(const Vec3& p) const;
  std::vector<CellCoord> cellsForAabb(const Vec3& lo, const Vec3& hi) const;

  bool enabled_ = false;
  Vec3 origin_ = Vec3::Zero();
  double cellSize_ = 0.0;
  std::unordered_map<CellCoord, std::unordered_set<int>, CellCoordHash> cells_;
  std::unordered_set<int> overflowFaces_;
  std::unordered_set<int> activeFaces_;
  std::vector<std::vector<CellCoord>> faceCells_;
};

} // namespace manumesh::simplification
