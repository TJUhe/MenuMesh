/**
 * @file src/common/detail/MeshQueries.h
 * @brief 声明 ManuMesh 公共几何模块的网格查询设施。
 * @ingroup manumesh_common
 *
 * @details 此处的例程是无策略几何基础，由特征检测、简化、分析和网格编辑共享。
 */

#pragma once

#include "core/Mesh.h"
#include "core/MeshTopology.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

namespace manumesh::common {

/**
 * @brief 一条无向网格边的相邻面列表。
 */
struct MeshEdgeInfo {
    std::vector<int> faces;
};

using MeshEdgeInfoMap = std::unordered_map<std::uint64_t, MeshEdgeInfo>;

/**
 * @brief 一个流形内部边的绕序感知二面角分类。
 */
struct OrientedDihedralAngle {
    double angleRad = 0.0;
    bool inconsistentWinding = false;
    /**
     * @brief +1 表示凸脊，-1 表示凹谷，0 表示平坦/未知。
     */
    int signedKind = 0;
};

/**
 * @brief 将无向顶点对打包为稳定整数键。
 *
 * 保留为指向核心 topologyEdgeKey 的内联转发，使所有模块共享同一打包方案；
 * 现有 common::meshEdgeKey 调用方可保持不变。
 */
inline std::uint64_t meshEdgeKey(int a, int b) { return topologyEdgeKey(a, b); }

/**
 * @brief 解包由 meshEdgeKey 创建的键。
 */
std::pair<int, int> unpackMeshEdgeKey(std::uint64_t key);

/**
 * @brief 返回用于重复面查找的排序键。
 */
std::array<int, 3> sortedFaceKey(std::array<int, 3> ids);

/**
 * @brief 规范排序三角形顶点 id 的稳定哈希。
 */
struct FaceKeyHash {
    /** @brief 对三个规范顶点 id 进行哈希。 */
    std::size_t operator()(const std::array<int, 3>& ids) const;
};

/**
 * @brief 为需要局部拓扑的算法一次性构建边到面的入射关系。
 */
MeshEdgeInfoMap buildMeshEdgeInfo(const Mesh& mesh);

/**
 * @brief 为每个面计算一个单位法向；退化三角形返回零向量。
 */
std::vector<Vec3> computeFaceNormals(const Mesh& mesh);

/**
 * @brief 构建确定性的逐面翻转标记，在不修改输入网格的情况下协调每个可定向
 * 流形连通分量内的绕序。
 */
std::vector<char> harmonizeFaceWindings(const Mesh& mesh, const MeshEdgeInfoMap& edges);

/**
 * @brief 计算两面边的绕序感知二面角。无法解析的方向冲突会回退到无符号法向角，
 * 并设置诊断标记。
 */
OrientedDihedralAngle computeOrientedDihedralAngle(
    const Mesh& mesh,
    const std::vector<Vec3>& normals,
    const std::vector<char>& windingFlip,
    const MeshEdgeInfo& info,
    int a,
    int b
);

/**
 * @brief 计算一个三角形面的质心。
 */
Vec3 faceCentroid(const Mesh& mesh, const Face& face);

/**
 * @brief 构建去重的一环顶点邻接。
 *
 * 每个顶点的邻居列表按升序排序，使遍历顺序（以及浮点归约顺序）在不同平台和
 * 标准库实现之间保持确定性。
 */
std::vector<std::vector<int>> buildVertexNeighbors(const Mesh& mesh);

/**
 * @brief 计算每个顶点的平均入射边长。
 *
 * 孤立顶点在全局平均边长可用时取该值；无边网格取 0。算法将其用作局部采样密度尺度。
 */
std::vector<double> computeVertexAverageEdgeLength(const Mesh& mesh);

/**
 * @brief 标记与边界边相接的顶点。
 */
std::vector<char> computeBoundaryVertices(const Mesh& mesh);

} // 命名空间 manumesh::common

namespace manumesh {
// 过渡别名：manumesh::detail 已重命名为 manumesh::common
// （架构 v2，R6）。新代码必须使用 manumesh::common；此别名将在一个小版本后移除。
namespace detail = common;
} // 命名空间 manumesh
