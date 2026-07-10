#pragma once

#include "core/Mesh.h"

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace manumesh::detail {

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
    void insert(int itemId, const Vec3& lo, const Vec3& hi);
    void remove(int itemId);
    void update(int itemId, const Vec3& lo, const Vec3& hi);
    std::vector<int> queryCandidates(const Vec3& lo, const Vec3& hi) const;
    bool enabled() const { return enabled_; }

private:
    CellCoord coordFor(const Vec3& p) const;
    std::vector<CellCoord> cellsForAabb(const Vec3& lo, const Vec3& hi) const;

    bool enabled_ = false;
    Vec3 origin_ = Vec3::Zero();
    double cellSize_ = 0.0;
    std::unordered_map<CellCoord, std::unordered_set<int>, CellCoordHash> cells_;
    std::unordered_set<int> overflowItems_;
    std::unordered_set<int> activeItems_;
    std::vector<std::vector<CellCoord>> itemCells_;
};

} // namespace manumesh::detail
