/**
 * @file src/common/SpatialIndex.cpp
 * @brief 实现 ManuMesh 公共几何模块的空间索引设施。
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

namespace manumesh::common {

void UniformAabbCandidateGrid::clear() {
    enabled_ = false;
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

    origin_ = lo;
    const double diag = std::max(1e-12, (hi - lo).norm());
    cellSize_ = std::max(diag / std::max(1.0, std::cbrt(static_cast<double>(expectedItems))), diag * 1e-6);
    enabled_ = std::isfinite(cellSize_) && cellSize_ > 0.0;
}

void UniformAabbCandidateGrid::insert(int itemId, const Vec3& lo, const Vec3& hi) {
    if (!enabled_ || itemId < 0) {
        return;
    }
    // 幂等重新插入：先删除任何过期注册，确保没有单元或溢出集合保留此项目的旧条目。
    remove(itemId);
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
        if (itemId >= static_cast<int>(candidateStamps_.size())) {
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
}

CellCoord UniformAabbCandidateGrid::coordFor(const Vec3& p) const {
    return {
        static_cast<int>(std::floor((p.x() - origin_.x()) / cellSize_)),
        static_cast<int>(std::floor((p.y() - origin_.y()) / cellSize_)),
        static_cast<int>(std::floor((p.z() - origin_.z()) / cellSize_))
    };
}

std::vector<CellCoord> UniformAabbCandidateGrid::cellsForAabb(const Vec3& lo, const Vec3& hi) const {
    std::vector<CellCoord> result;
    cellsForAabb(lo, hi, result);
    return result;
}

void UniformAabbCandidateGrid::cellsForAabb(const Vec3& lo, const Vec3& hi, std::vector<CellCoord>& outCells) const {
    outCells.clear();
    if (!enabled_ || !lo.allFinite() || !hi.allFinite()) {
        return;
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
        return;
    }

    outCells.reserve(static_cast<std::size_t>(count));
    for (int x = minX; x <= maxX; ++x) {
        for (int y = minY; y <= maxY; ++y) {
            for (int z = minZ; z <= maxZ; ++z) {
                outCells.push_back(CellCoord{x, y, z});
            }
        }
    }
}

} // 命名空间 manumesh::common
