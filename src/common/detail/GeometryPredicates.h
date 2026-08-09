/**
 * @file src/common/detail/GeometryPredicates.h
 * @brief 声明 ManuMesh 公共几何模块的几何谓词设施。
 * @ingroup manumesh_common
 *
 * @details 此处的例程是无策略几何基础，由特征检测、简化、分析和网格编辑共享。
 */

#pragma once

#include "core/Mesh.h"

#include <array>
#include <utility>

namespace manumesh::common {

double triangleQuality(const Vec3& a, const Vec3& b, const Vec3& c);
double pointTriangleDistanceSquared(const Vec3& p, const Vec3& a, const Vec3& b, const Vec3& c);
double pointAabbDistanceSquared(const Vec3& p, const Vec3& lo, const Vec3& hi);
std::pair<Vec3, Vec3> triangleAabb(const std::array<Vec3, 3>& tri, double padding = 0.0);
/**
 * @brief 使用相对容差的三角形相交测试。
 *
 * eps 无量纲：每个内部比较都按两个三角形的局部几何尺度归一化（长度按尺度、
 * 面积/方向按 scale^2、Moller-Trumbore 行列式按自身因子范数），因此输入一致
 * 缩放时判定不变。典型 eps：1e-9 .. 1e-12。
 */
bool trianglesIntersect(const std::array<Vec3, 3>& lhs, const std::array<Vec3, 3>& rhs, double eps);

/**
 * @brief 可能共享顶点 id 的网格三角形相交测试。允许限定在声明共享顶点或共享边
 * 内的接触；超出该共享拓扑的重叠或交叉会报告为相交。
 */
bool trianglesIntersectBeyondSharedTopology(
    const std::array<int, 3>& lhsIds,
    const std::array<Vec3, 3>& lhs,
    const std::array<int, 3>& rhsIds,
    const std::array<Vec3, 3>& rhs,
    double eps
);

} // 命名空间 manumesh::common

namespace manumesh {
// 过渡别名：manumesh::detail 已重命名为 manumesh::common
// （架构 v2，R6）。新代码必须使用 manumesh::common；此别名将在一个小版本后移除。
namespace detail = common;
} // 命名空间 manumesh
