#include "simplification/SpatialFaceIndex.h"

#include "simplification/GeometryPredicates.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace lq {

void SpatialFaceIndex::rebuild(const std::vector<FaceState>& faces,
                               const std::vector<VertexState>& vertices) {
  cells_.clear();
  overflowFaces_.clear();
  activeFaces_.clear();
  faceCells_.assign(faces.size(), {});
  enabled_ = false;
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

  origin_ = lo;
  const double diag = std::max(1e-12, (hi - lo).norm());
  cellSize_ = std::max(
      diag / std::max(1.0, std::cbrt(static_cast<double>(activeFaces))), diag * 1e-6);
  enabled_ = std::isfinite(cellSize_) && cellSize_ > 0.0;
  if (!enabled_) {
    return;
  }

  for (int fi = 0; fi < static_cast<int>(faces.size()); ++fi) {
    if (faces[fi].active) {
      insertFace(fi, faces[fi], vertices);
    }
  }
}

void SpatialFaceIndex::removeFace(int faceId) {
  if (!enabled_ || faceId < 0 || faceId >= static_cast<int>(faceCells_.size())) {
    return;
  }
  activeFaces_.erase(faceId);
  for (const CellCoord& cell : faceCells_[faceId]) {
    auto it = cells_.find(cell);
    if (it == cells_.end()) {
      continue;
    }
    it->second.erase(faceId);
    if (it->second.empty()) {
      cells_.erase(it);
    }
  }
  faceCells_[faceId].clear();
  overflowFaces_.erase(faceId);
}

void SpatialFaceIndex::updateFace(int faceId, const FaceState& face,
                                  const std::vector<VertexState>& vertices) {
  removeFace(faceId);
  if (face.active) {
    insertFace(faceId, face, vertices);
  }
}

std::vector<int> SpatialFaceIndex::query(const Vec3& lo, const Vec3& hi) const {
  std::unordered_set<int> result;
  if (!enabled_) {
    return {};
  }
  const std::vector<CellCoord> queryCells = cellsForAabb(lo, hi);
  if (queryCells.empty()) {
    return std::vector<int>(activeFaces_.begin(), activeFaces_.end());
  }
  for (const CellCoord& cell : queryCells) {
    const auto it = cells_.find(cell);
    if (it == cells_.end()) {
      continue;
    }
    result.insert(it->second.begin(), it->second.end());
  }
  result.insert(overflowFaces_.begin(), overflowFaces_.end());
  return std::vector<int>(result.begin(), result.end());
}

void SpatialFaceIndex::insertFace(int faceId, const FaceState& face,
                                  const std::vector<VertexState>& vertices) {
  if (!enabled_ || faceId < 0 || faceId >= static_cast<int>(faceCells_.size())) {
    return;
  }
  activeFaces_.insert(faceId);
  const std::array<Vec3, 3> tri = {vertices[face.v[0]].p, vertices[face.v[1]].p,
                                   vertices[face.v[2]].p};
  const auto [lo, hi] = triangleAabb(tri);
  std::vector<CellCoord> cells = cellsForAabb(lo, hi);
  if (cells.empty()) {
    overflowFaces_.insert(faceId);
    return;
  }
  faceCells_[faceId] = cells;
  for (const CellCoord& cell : cells) {
    cells_[cell].insert(faceId);
  }
}

CellCoord SpatialFaceIndex::coordFor(const Vec3& p) const {
  return {static_cast<int>(std::floor((p.x() - origin_.x()) / cellSize_)),
          static_cast<int>(std::floor((p.y() - origin_.y()) / cellSize_)),
          static_cast<int>(std::floor((p.z() - origin_.z()) / cellSize_))};
}

std::vector<CellCoord> SpatialFaceIndex::cellsForAabb(const Vec3& lo,
                                                      const Vec3& hi) const {
  if (!enabled_ || !lo.allFinite() || !hi.allFinite()) {
    return {};
  }
  const CellCoord c0 = coordFor(lo);
  const CellCoord c1 = coordFor(hi);
  const int minX = std::min(c0.x, c1.x);
  const int maxX = std::max(c0.x, c1.x);
  const int minY = std::min(c0.y, c1.y);
  const int maxY = std::max(c0.y, c1.y);
  const int minZ = std::min(c0.z, c1.z);
  const int maxZ = std::max(c0.z, c1.z);
  const long long count = static_cast<long long>(maxX - minX + 1) *
                          static_cast<long long>(maxY - minY + 1) *
                          static_cast<long long>(maxZ - minZ + 1);
  constexpr long long kMaxCellsPerFace = 512;
  if (count <= 0 || count > kMaxCellsPerFace) {
    return {};
  }

  std::vector<CellCoord> result;
  result.reserve(static_cast<std::size_t>(count));
  for (int x = minX; x <= maxX; ++x) {
    for (int y = minY; y <= maxY; ++y) {
      for (int z = minZ; z <= maxZ; ++z) {
        result.push_back(CellCoord{x, y, z});
      }
    }
  }
  return result;
}

} // namespace lq
