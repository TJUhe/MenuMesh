#include "common/detail/SpatialIndex.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace manumesh::detail {

void UniformAabbCandidateGrid::clear() {
    enabled_ = false;
    origin_ = Vec3::Zero();
    cellSize_ = 0.0;
    cells_.clear();
    overflowItems_.clear();
    activeItems_.clear();
    itemCells_.clear();
}

void UniformAabbCandidateGrid::reset(const Vec3& lo, const Vec3& hi, int expectedItems) {
    clear();
    if (expectedItems <= 0 || !lo.allFinite() || !hi.allFinite()) {
        return;
    }

    origin_ = lo;
    const double diag = std::max(1e-12, (hi - lo).norm());
    cellSize_ = std::max(diag / std::max(1.0, std::cbrt(static_cast<double>(expectedItems))), diag * 1e-6);
    enabled_ = std::isfinite(cellSize_) && cellSize_ > 0.0;
}

void UniformAabbCandidateGrid::insert(int itemId, const Vec3& lo, const Vec3& hi) {
    if (!enabled_ || itemId < 0) {
        return;
    }
    if (itemId >= static_cast<int>(itemCells_.size())) {
        itemCells_.resize(static_cast<std::size_t>(itemId) + 1u);
    }

    activeItems_.insert(itemId);
    std::vector<CellCoord> cells = cellsForAabb(lo, hi);
    if (cells.empty()) {
        overflowItems_.insert(itemId);
        return;
    }

    itemCells_[itemId] = cells;
    for (const CellCoord& cell : cells) {
        cells_[cell].insert(itemId);
    }
}

void UniformAabbCandidateGrid::remove(int itemId) {
    if (!enabled_ || itemId < 0 || itemId >= static_cast<int>(itemCells_.size())) {
        return;
    }

    activeItems_.erase(itemId);
    for (const CellCoord& cell : itemCells_[itemId]) {
        auto it = cells_.find(cell);
        if (it == cells_.end()) {
            continue;
        }
        it->second.erase(itemId);
        if (it->second.empty()) {
            cells_.erase(it);
        }
    }
    itemCells_[itemId].clear();
    overflowItems_.erase(itemId);
}

void UniformAabbCandidateGrid::update(int itemId, const Vec3& lo, const Vec3& hi) {
    remove(itemId);
    insert(itemId, lo, hi);
}

std::vector<int> UniformAabbCandidateGrid::queryCandidates(const Vec3& lo, const Vec3& hi) const {
    std::unordered_set<int> result;
    if (!enabled_) {
        return {};
    }

    const std::vector<CellCoord> queryCells = cellsForAabb(lo, hi);
    if (queryCells.empty()) {
        return std::vector<int>(activeItems_.begin(), activeItems_.end());
    }

    for (const CellCoord& cell : queryCells) {
        const auto it = cells_.find(cell);
        if (it == cells_.end()) {
            continue;
        }
        result.insert(it->second.begin(), it->second.end());
    }
    result.insert(overflowItems_.begin(), overflowItems_.end());
    return std::vector<int>(result.begin(), result.end());
}

CellCoord UniformAabbCandidateGrid::coordFor(const Vec3& p) const {
    return {
        static_cast<int>(std::floor((p.x() - origin_.x()) / cellSize_)),
        static_cast<int>(std::floor((p.y() - origin_.y()) / cellSize_)),
        static_cast<int>(std::floor((p.z() - origin_.z()) / cellSize_))
    };
}

std::vector<CellCoord> UniformAabbCandidateGrid::cellsForAabb(const Vec3& lo, const Vec3& hi) const {
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
    const long long count = static_cast<long long>(maxX - minX + 1) * static_cast<long long>(maxY - minY + 1) *
                            static_cast<long long>(maxZ - minZ + 1);
    constexpr long long kMaxCellsPerItem = 512;
    if (count <= 0 || count > kMaxCellsPerItem) {
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

} // namespace manumesh::detail
