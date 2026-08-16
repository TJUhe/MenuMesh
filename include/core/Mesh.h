/**
 * @file include/core/Mesh.h
 * @brief 定义三角网格存储、校验和基础几何查询。
 * @ingroup manumesh_core
 *
 * @details Mesh 拥有顶点、三角面和可选逐角 UV；算法不会在构造时静默修复输入。
 */

#pragma once

#include "Export.h"

#include <Eigen/Dense>
#include <array>
#include <string>
#include <utility>
#include <vector>

namespace manumesh {

/// 公共几何 API 中使用的双精度三维向量。
using Vec3 = Eigen::Vector3d;
/// 用于纹理坐标的双精度二维向量。
using Vec2 = Eigen::Vector2d;
/// 齐次 4x4 矩阵（例如二次误差累积或仿射变换）。
using Mat4 = Eigen::Matrix4d;

/// 存储三个从零开始顶点索引的三角形面。
///
/// 索引顺序决定面法向方向，但存储层不要求逆时针，也不会自动翻转。
/// 下游需要一致法向时，调用方应保持相邻三角面的绕序一致。
struct Face {
    std::array<int, 3> v{}; ///< 指向顶点数组的从零开始索引，顺序按输入保留。
};

/// 一个三角形的逐角纹理坐标。
///
/// 纹理坐标归属于面角而非顶点，因此同一个几何顶点可在相邻 UV 图表中
/// 保留不同坐标。
struct FaceTexCoords {
    std::array<Vec2, 3> uv{}; ///< 每个面角对应的 UV 值。
    bool valid = false;       ///< 此面是否拥有可用的 UV 坐标。
};

/// 简化器和工具使用的最小三角网格容器。
struct Mesh {
    std::vector<Vec3> vertices; ///< 模型单位下的顶点位置。
    std::vector<Face> faces;    ///< 指向 `vertices` 的三角形连接关系。
    /// 网格没有纹理坐标时为空；否则此向量与面对齐，未纹理化的面对应条目
    /// 可能无效。
    std::vector<FaceTexCoords> faceTexCoords;

    /// @return 当顶点或面存储为空时返回 true。
    MANUMESH_API bool empty() const;
    /// @return 轴对齐包围盒最小点；没有顶点时返回零向量。
    MANUMESH_API Vec3 bboxMin() const;
    /// @return 轴对齐包围盒最大点；没有顶点时返回零向量。
    MANUMESH_API Vec3 bboxMax() const;
    /// @return 模型单位下轴对齐包围盒对角线长度。
    MANUMESH_API double bboxDiag() const;
    /// @return 当至少一个面拥有有效逐角坐标时返回 true。
    MANUMESH_API bool hasTextureCoordinates() const;
    /// 删除数据后压缩顶点存储并重写面索引。
    ///
    /// 引用越界顶点索引的面会被静默丢弃，仅被这些面引用的顶点也会移除；
    /// 完成后每个剩余顶点至少被一个有效面使用。
    MANUMESH_API void removeUnusedVertices();
};

/// 检查每个面索引是否都指向 `mesh.vertices`。
/// @param[in] mesh 要检查的网格。
/// @param[out] error 可选诊断信息；成功时清空。
/// @return 当所有索引有效时返回 true。
MANUMESH_API bool validateMeshIndices(const Mesh& mesh, std::string* error = nullptr);
/// 严格校验：当索引无效、顶点坐标非有限、一个面重复顶点索引，或一个面的
/// 面积（数值上）为零时返回 false。用于下游无法表示退化几何的场景
/// （例如 STL 导出）。
/// @param[in] mesh 要校验的网格。
/// @param[out] error 可选诊断信息；成功时清空。
/// @return 仅当索引、坐标、逐面拓扑、UV 和三角形面积满足严格存储契约时返回 true。
MANUMESH_API bool validateMeshGeometry(const Mesh& mesh, std::string* error = nullptr);
/// 面向分析/简化入口的宽松校验：拒绝任何算法都无法处理的输入（索引无效、
/// 坐标非有限、纹理坐标未对齐或非有限、面重复顶点索引），但允许零面积面。
/// 调用方应通过 countDegenerateFaces 报告被容忍的退化情况。
/// @param[in] mesh 要校验的网格。
/// @param[out] error 可选诊断信息；成功时清空。
/// @return 除允许的零面积面外，当网格对算法安全时返回 true。
MANUMESH_API bool validateMeshGeometryLenient(const Mesh& mesh, std::string* error = nullptr);
/// @param[in] mesh 索引已经校验过的网格。
/// @return 重复索引或（数值上）零面积的面数量。
///
/// 统计退化面：重复顶点索引或（数值上）零面积。
/// 假定索引有效（参见 validateMeshIndices）。
MANUMESH_API int countDegenerateFaces(const Mesh& mesh);

/// @param[in] a 三角形第一个位置。
/// @param[in] b 三角形第二个位置。
/// @param[in] c 三角形第三个位置。
/// @return 模型平方单位下的无符号三角形面积。
MANUMESH_API double triangleArea(const Vec3& a, const Vec3& b, const Vec3& c);
/// @param[in] a 三角形第一个位置。
/// @param[in] b 三角形第二个位置。
/// @param[in] c 三角形第三个位置。
/// @return 遵循 `(b-a) x (c-a)` 的单位法向量；退化时返回零向量。
MANUMESH_API Vec3 triangleNormal(const Vec3& a, const Vec3& b, const Vec3& c);
/// @param[in] mesh 面索引有效的网格。
/// @return 每条无向边仅出现一次，并按 `(a,b)` 字典序排列；每个端点对满足 `a < b`。
/// @complexity 预期为 O(F)，其中 F 为面数量。
MANUMESH_API std::vector<std::pair<int, int>> uniqueEdges(const Mesh& mesh);

} // 命名空间 manumesh
