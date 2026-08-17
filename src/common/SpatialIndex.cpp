/**
 * @file src/common/SpatialIndex.cpp
 * @brief 实现用于 AABB 候选查询的稀疏均匀网格。
 * @ingroup manumesh_common
 *
 * @details 实现用于宽相位 AABB 候选查询的可变均匀网格。
 * @algorithm 根据域范围和预期项目数量推导单元尺寸；较大的 AABB 使用溢出集合。
 * 版本戳无需为每次查询分配集合即可去重查询结果。
 * @failuremodes 无效或不切实际的大网格会禁用加速，并回退到溢出路径，
 * 不改变下游的精确结果。
 */

#include "common/detail/SpatialIndex.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace manumesh {
namespace common {

void UniformAabbCandidateGrid::clear() {
    enabled_ = false;
    conservativeFallback_ = false;
    origin_ = Vec3::Zero();
    cellSize_ = 0.0;
    cells_.clear();
    overflowItems_.clear();
    activeItems_.clear();
    itemCells_.clear();
    queryCellsScratch_.clear();
    candidateStamps_.clear();
    queryStamp_ = 0;
}

void UniformAabbCandidateGrid::reset(const Vec3& lo, const Vec3& hi, int expectedItems) {
    clear();
    if (expectedItems <= 0 || !lo.allFinite() || !hi.allFinite()) {
        return;
    }
    for (int axis = 0; axis < 3; ++axis) {
        if (hi[axis] < lo[axis]) {
            return;
        }
    }

    origin_ = lo;
    const double rawDiag = (hi - lo).stableNorm();
    if (!std::isfinite(rawDiag)) {
        // The domain is valid but too wide to map safely to integer cells.
        // Keep the index active in an all-items fallback mode so broad-phase
        // acceleration can never turn into a false negative.
        enabled_ = true;
        conservativeFallback_ = true;
        cellSize_ = 1.0;
        return;
    }
    const double diag = std::max(1e-12, rawDiag);
    cellSize_ = std::max(diag / std::max(1.0, std::cbrt(static_cast<double>(expectedItems))), diag * 1e-6);
    enabled_ = std::isfinite(cellSize_) && cellSize_ > 0.0;
}

void UniformAabbCandidateGrid::insert(int itemId, const Vec3& lo, const Vec3& hi) {
    if (!enabled_ || itemId < 0) {
        return;
    }
    // 幂等重新插入：先删除任何过期注册，确保没有单元或溢出集合保留此项目的旧条目。
    remove(itemId);
    if (static_cast<std::size_t>(itemId) >= itemCells_.size()) {
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
    if (!enabled_ || itemId < 0 || static_cast<std::size_t>(itemId) >= itemCells_.size()) {
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
    std::vector<int> result;
    queryCandidates(lo, hi, result);
    return result;
}

void UniformAabbCandidateGrid::queryCandidates(const Vec3& lo, const Vec3& hi, std::vector<int>& outCandidates) const {
    outCandidates.clear();
    if (!enabled_) {
        return;
    }

    cellsForAabb(lo, hi, queryCellsScratch_);
    if (queryCellsScratch_.empty()) {
        outCandidates.assign(activeItems_.begin(), activeItems_.end());
        std::sort(outCandidates.begin(), outCandidates.end());
        return;
    }

    if (candidateStamps_.size() < itemCells_.size()) {
        candidateStamps_.resize(itemCells_.size(), 0u);
    }
    ++queryStamp_;
    if (queryStamp_ == 0u) {
        // 戳计数器发生回绕；重置所有戳，避免旧标记发生别名冲突。
        std::fill(candidateStamps_.begin(), candidateStamps_.end(), 0u);
        ++queryStamp_;
    }
    const auto pushUnique = [&](int itemId) {
        if (itemId < 0) {
            return;
        }
        if (static_cast<std::size_t>(itemId) >= candidateStamps_.size()) {
            candidateStamps_.resize(static_cast<std::size_t>(itemId) + 1u, 0u);
        }
        if (candidateStamps_[itemId] == queryStamp_) {
            return;
        }
        candidateStamps_[itemId] = queryStamp_;
        outCandidates.push_back(itemId);
    };

    for (const CellCoord& cell : queryCellsScratch_) {
        const auto it = cells_.find(cell);
        if (it == cells_.end()) {
            continue;
        }
        for (int itemId : it->second) {
            pushUnique(itemId);
        }
    }
    for (int itemId : overflowItems_) {
        pushUnique(itemId);
    }
    std::sort(outCandidates.begin(), outCandidates.end());
}

bool UniformAabbCandidateGrid::coordFor(const Vec3& p, CellCoord& outCoord) const {
    if (!p.allFinite() || !origin_.allFinite() || !std::isfinite(cellSize_) || cellSize_ <= 0.0) {
        return false;
    }
    int coordinates[3] = {};
    const double minInt = static_cast<double>(std::numeric_limits<int>::min());
    const double maxInt = static_cast<double>(std::numeric_limits<int>::max());
    for (int axis = 0; axis < 3; ++axis) {
        const double scaled = (p[axis] - origin_[axis]) / cellSize_;
        if (!std::isfinite(scaled)) {
            return false;
        }
        const double floored = std::floor(scaled);
        if (floored < minInt || floored > maxInt) {
            return false;
        }
        coordinates[axis] = static_cast<int>(floored);
    }
    outCoord = CellCoord{coordinates[0], coordinates[1], coordinates[2]};
    return true;
}

std::vector<CellCoord> UniformAabbCandidateGrid::cellsForAabb(const Vec3& lo, const Vec3& hi) const {
    std::vector<CellCoord> result;
    cellsForAabb(lo, hi, result);
    return result;
}

void UniformAabbCandidateGrid::cellsForAabb(const Vec3& lo, const Vec3& hi, std::vector<CellCoord>& outCells) const {
    outCells.clear();
    if (!enabled_ || conservativeFallback_ || !lo.allFinite() || !hi.allFinite()) {
        return;
    }

    CellCoord c0;
    CellCoord c1;
    if (!coordFor(lo, c0) || !coordFor(hi, c1)) {
        return;
    }
    const int minX = std::min(c0.x, c1.x);
    const int maxX = std::max(c0.x, c1.x);
    const int minY = std::min(c0.y, c1.y);
    const int maxY = std::max(c0.y, c1.y);
    const int minZ = std::min(c0.z, c1.z);
    const int maxZ = std::max(c0.z, c1.z);
    constexpr long long kMaxCellsPerItem = 512;
    const long long countX = static_cast<long long>(maxX) - static_cast<long long>(minX) + 1;
    const long long countY = static_cast<long long>(maxY) - static_cast<long long>(minY) + 1;
    const long long countZ = static_cast<long long>(maxZ) - static_cast<long long>(minZ) + 1;
    if (countX <= 0 || countY <= 0 || countZ <= 0 || countX > kMaxCellsPerItem || countY > kMaxCellsPerItem ||
        countZ > kMaxCellsPerItem) {
        return;
    }
    const long long count = countX * countY * countZ;
    if (count > kMaxCellsPerItem) {
        return;
    }

    outCells.reserve(static_cast<std::size_t>(count));
    for (long long x = minX; x <= static_cast<long long>(maxX); ++x) {
        for (long long y = minY; y <= static_cast<long long>(maxY); ++y) {
            for (long long z = minZ; z <= static_cast<long long>(maxZ); ++z) {
                outCells.push_back(CellCoord{static_cast<int>(x), static_cast<int>(y), static_cast<int>(z)});
            }
        }
    }
}

} // namespace common
} // namespace manumesh
