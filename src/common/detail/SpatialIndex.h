/**
 * @file src/common/detail/SpatialIndex.h
 * @brief Declares spatial index facilities for ManuMesh's common-geometry module.
 * @ingroup manumesh_common
 *
 * @details The routines here are policy-free geometry foundations shared by feature detection, simplification, analysis, and mesh editing.
 */

#pragma once

#include "core/Mesh.h"

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace manumesh::common {

struct CellCoord {
    int x = 0;
    int y = 0;
    int z = 0;

    bool operator==(const CellCoord& other) const { return x == other.x && y == other.y && z == other.z; }
};

struct CellCoordHash {
    std::size_t operator()(const CellCoord& cell) const {
        std::size_t seed = 1469598103934665603ull;
        auto mix = [&](int value) {
            seed ^= static_cast<std::size_t>(static_cast<std::uint32_t>(value));
            seed *= 1099511628211ull;
        };
        mix(cell.x);
        mix(cell.y);
        mix(cell.z);
        return seed;
    }
};

class UniformAabbCandidateGrid {
public:
    void clear();
    void reset(const Vec3& lo, const Vec3& hi, int expectedItems);
    /// Registers an item under its AABB. Re-inserting an already registered
    /// itemId is idempotent: any stale cell registration from a previous
    /// insert is removed first, so no cell keeps an outdated entry.
    void insert(int itemId, const Vec3& lo, const Vec3& hi);
    void remove(int itemId);
    void update(int itemId, const Vec3& lo, const Vec3& hi);
    std::vector<int> queryCandidates(const Vec3& lo, const Vec3& hi) const;
    /// Allocation-friendly overload: writes the deduplicated candidate ids
    /// into outCandidates (cleared first) reusing member scratch buffers.
    /// Candidate order is unspecified, matching the by-value overload.
    void queryCandidates(const Vec3& lo, const Vec3& hi, std::vector<int>& outCandidates) const;
    bool enabled() const { return enabled_; }

private:
    CellCoord coordFor(const Vec3& p) const;
    std::vector<CellCoord> cellsForAabb(const Vec3& lo, const Vec3& hi) const;
    void cellsForAabb(const Vec3& lo, const Vec3& hi, std::vector<CellCoord>& outCells) const;

    bool enabled_ = false;
    Vec3 origin_ = Vec3::Zero();
    double cellSize_ = 0.0;
    std::unordered_map<CellCoord, std::unordered_set<int>, CellCoordHash> cells_;
    std::unordered_set<int> overflowItems_;
    std::unordered_set<int> activeItems_;
    std::vector<std::vector<CellCoord>> itemCells_;
    // Query scratch state: reused by the const query paths, hence mutable.
    // The version-stamp array deduplicates candidates without per-query
    // allocation; a query only treats stamps equal to queryStamp_ as seen.
    mutable std::vector<CellCoord> queryCellsScratch_;
    mutable std::vector<std::uint32_t> candidateStamps_;
    mutable std::uint32_t queryStamp_ = 0;
};

} // namespace manumesh::common

namespace manumesh {
// Transitional alias: manumesh::detail was renamed to manumesh::common
// (architecture v2, R6). New code must use manumesh::common; this alias is
// removed after one minor version.
namespace detail = common;
} // namespace manumesh
