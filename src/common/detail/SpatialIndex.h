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

/**
 * @brief Integer coordinates of one uniform-grid cell.
 */
struct CellCoord {
    int x = 0;
    int y = 0;
    int z = 0;

    /** @brief Compares all three integer coordinates. */
    bool operator==(const CellCoord& other) const { return x == other.x && y == other.y && z == other.z; }
};

/**
 * @brief Stable hash for CellCoord keys used by the sparse grid.
 */
struct CellCoordHash {
    /** @brief Combines all cell coordinates into one deterministic hash. */
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

/**
 * @brief Sparse uniform grid for conservative AABB candidate lookup.
 *
 * Items spanning too many cells are kept in an overflow set so queries remain
 * conservative. Query methods reuse mutable scratch storage and are therefore
 * not safe to call concurrently on the same instance.
 */
class UniformAabbCandidateGrid {
public:
    /** @brief Removes all items and disables the grid. */
    void clear();
    /** @brief Reinitializes the grid bounds and storage for a new item set. */
    void reset(const Vec3& lo, const Vec3& hi, int expectedItems);
    /**
     * @brief Registers an item under its AABB. Re-inserting an already registered
     * itemId is idempotent: any stale cell registration from a previous
     * insert is removed first, so no cell keeps an outdated entry.
     */
    void insert(int itemId, const Vec3& lo, const Vec3& hi);
    /** @brief Removes an item from every occupied cell and the overflow set. */
    void remove(int itemId);
    /** @brief Replaces an item's current AABB registration. */
    void update(int itemId, const Vec3& lo, const Vec3& hi);
    /** @brief Returns deduplicated ids whose grid cells overlap an AABB. */
    std::vector<int> queryCandidates(const Vec3& lo, const Vec3& hi) const;
    /**
     * @brief Allocation-friendly overload: writes the deduplicated candidate ids
     * into outCandidates (cleared first) reusing member scratch buffers.
     * Candidate order is unspecified, matching the by-value overload.
     */
    void queryCandidates(const Vec3& lo, const Vec3& hi, std::vector<int>& outCandidates) const;
    /** @brief Reports whether reset() produced an active spatial grid. */
    bool enabled() const { return enabled_; }

private:
    /** @brief Maps a point to its containing integer grid cell. */
    CellCoord coordFor(const Vec3& p) const;
    /** @brief Enumerates cells overlapped by an AABB into a new vector. */
    std::vector<CellCoord> cellsForAabb(const Vec3& lo, const Vec3& hi) const;
    /** @brief Enumerates cells overlapped by an AABB into reusable storage. */
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
