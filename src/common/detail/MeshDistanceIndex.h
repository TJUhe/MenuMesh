/**
 * @file src/common/detail/MeshDistanceIndex.h
 * @brief 声明重复点到表面查询使用的三角形 BVH。
 * @ingroup manumesh_common
 *
 * @details 此处的例程是无策略几何基础，由特征检测、简化、分析和网格编辑共享。
 */

#pragma once

#include "core/Mesh.h"

#include <vector>

namespace manumesh {
namespace common {

/**
 * @brief 基于 BVH 的参考网格点到表面距离查询。
 *
 * 契约：索引持有 const Mesh& 引用，不拥有也不复制网格数据。被引用网格的生命周期
 * 必须长于索引，且构造后不得修改或移动；否则查询会读取过期或悬空数据。
 * 构造时校验面顶点索引：引用越界顶点的面（以及退化、近零面积的面）会被跳过并
 * 计数，而不会导致未定义行为。
 */
class MeshDistanceIndex {
public:
    /**
 * @brief 根据 `mesh` 的有效面构建点到三角形的 BVH。
 * @param[in] mesh 生命周期必须长于此索引的参考网格。
     */
    explicit MeshDistanceIndex(const Mesh& mesh);

    /** @brief 报告参考网格是否提供了至少一个有效面。 */
    bool empty() const;
    /**
 * @brief 返回点到参考表面的距离平方。
 * @return 索引为空时返回正无穷。
     */
    double distanceSquared(const Vec3& point) const;
    /**
 * @brief 构造时因引用越界顶点索引或发生退化而跳过的面数量。
     */
    int skippedFaceCount() const { return skippedFaceCount_; }

private:
    /** @brief 一个有效参考三角形的缓存边界和质心。 */
    struct TriangleRef {
        int face = -1;
        Vec3 lo = Vec3::Zero();
        Vec3 hi = Vec3::Zero();
        Vec3 centroid = Vec3::Zero();
    };

    /** @brief 覆盖半开三角形索引范围的二叉 BVH 节点。 */
    struct BvhNode {
        Vec3 lo = Vec3::Zero();
        Vec3 hi = Vec3::Zero();
        int left = -1;
        int right = -1;
        int begin = 0;
        int end = 0;
    };

    /** @brief 在 `[begin,end)` 上构建一个 BVH 子树并返回其节点 id。 */
    int buildRecursive(int begin, int end);
    /** @brief 遍历 BVH 子树时更新 `best`。 */
    void queryRecursive(int nodeId, const Vec3& point, double& best) const;

    const Mesh& mesh_;
    std::vector<TriangleRef> triangles_;
    std::vector<int> order_;
    std::vector<BvhNode> nodes_;
    int skippedFaceCount_ = 0;
};

} // namespace common
} // namespace manumesh
