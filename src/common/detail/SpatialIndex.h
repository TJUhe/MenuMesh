/**
 * @file src/common/detail/SpatialIndex.h
 * @brief 声明 ManuMesh 公共几何模块的空间索引设施。
 * @ingroup manumesh_common
 *
 * @details 此处的例程是无策略几何基础，由特征检测、简化、分析和网格编辑共享。
 */

#pragma once

#include "core/Mesh.h"

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace manumesh {
namespace common {

/**
 * @brief 一个均匀网格单元的整数坐标。
 */
struct CellCoord {
    int x = 0;
    int y = 0;
    int z = 0;

    /** @brief 比较三个整数坐标。 */
    bool operator==(const CellCoord& other) const { return x == other.x && y == other.y && z == other.z; }
};

/**
 * @brief 稀疏网格使用的 CellCoord 键的稳定哈希。
 */
struct CellCoordHash {
    /** @brief 将所有单元坐标组合为一个确定性哈希。 */
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
 * @brief 用于保守 AABB 候选查找的稀疏均匀网格。
 *
 * 跨越过多单元的项目保存在溢出集合中，以保持查询的保守性。查询方法复用可变
 * 临时存储，因此不能在同一实例上并发调用。
 */
class UniformAabbCandidateGrid {
public:
    /** @brief 移除所有项目并禁用网格。 */
    void clear();
    /** @brief 为新的项目集合重新初始化网格边界和存储。 */
    void reset(const Vec3& lo, const Vec3& hi, int expectedItems);
    /**
     * @brief 根据项目的 AABB 注册项目。重新插入已注册的 itemId 是幂等操作：
     * 会先移除之前插入留下的过期单元注册，因此不会有单元保留旧条目。
     */
    void insert(int itemId, const Vec3& lo, const Vec3& hi);
    /** @brief 从每个占用单元和溢出集合中移除项目。 */
    void remove(int itemId);
    /** @brief 替换项目当前的 AABB 注册。 */
    void update(int itemId, const Vec3& lo, const Vec3& hi);
    /** @brief 返回网格单元与 AABB 重叠的去重 id。 */
    std::vector<int> queryCandidates(const Vec3& lo, const Vec3& hi) const;
    /**
     * @brief 节省分配的重载：复用成员临时缓冲区，将去重后的候选 id 写入
     * outCandidates（先清空）。候选顺序未指定，与按值返回的重载一致。
     */
    void queryCandidates(const Vec3& lo, const Vec3& hi, std::vector<int>& outCandidates) const;
    /** @brief 报告 reset() 是否生成了活动空间网格。 */
    bool enabled() const { return enabled_; }

private:
    /** @brief 将点映射到其所在的整数网格单元。 */
    CellCoord coordFor(const Vec3& p) const;
    /** @brief 将与 AABB 重叠的单元枚举到新向量中。 */
    std::vector<CellCoord> cellsForAabb(const Vec3& lo, const Vec3& hi) const;
    /** @brief 将与 AABB 重叠的单元枚举到可复用存储中。 */
    void cellsForAabb(const Vec3& lo, const Vec3& hi, std::vector<CellCoord>& outCells) const;

    bool enabled_ = false;
    Vec3 origin_ = Vec3::Zero();
    double cellSize_ = 0.0;
    std::unordered_map<CellCoord, std::unordered_set<int>, CellCoordHash> cells_;
    std::unordered_set<int> overflowItems_;
    std::unordered_set<int> activeItems_;
    std::vector<std::vector<CellCoord>> itemCells_;
    // 查询临时状态：由 const 查询路径复用，因此声明为 mutable。
    // 版本戳数组无需为每次查询分配内存即可去重候选；查询仅将等于 queryStamp_
    // 的戳视为已见。
    mutable std::vector<CellCoord> queryCellsScratch_;
    mutable std::vector<std::uint32_t> candidateStamps_;
    mutable std::uint32_t queryStamp_ = 0;
};

} // namespace common
} // namespace manumesh
