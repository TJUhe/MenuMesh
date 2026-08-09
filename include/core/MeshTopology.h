/**
 * @file include/core/MeshTopology.h
 * @brief 声明 ManuMesh 核心网格模块的网格拓扑设施。
 * @ingroup manumesh_core
 *
 * @details 核心类型建立所有算法模块共同使用的存储、校验、容差、拓扑和状态契约。
 */

#pragma once

#include "Export.h"
#include "core/Handles.h"
#include "core/Mesh.h"
#include "core/Status.h"

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace manumesh {

/// 带相邻面和局部面角记录的无向边。
struct TopologyEdge {
    std::array<int, 2> vertices{{-1, -1}}; ///< 升序端点索引。
    std::vector<int> faces;                ///< 入射面索引。
    std::vector<int> faceCorners;          ///< 与此边相对的局部面角。

    /// @return 当恰有一个入射面时返回 true。
    MANUMESH_API bool boundary() const;
    /// @return 当恰有两个入射面时返回 true。
    MANUMESH_API bool manifoldInterior() const;
    /// @return 当入射面多于两个时返回 true。
    MANUMESH_API bool nonManifold() const;
};

/// 一次构建并由算法复用的逐顶点入射边/面列表。
struct VertexTopology {
    std::vector<int> edges; ///< 按确定性顺序排列的入射边索引。
    std::vector<int> faces; ///< 按确定性顺序排列的入射面索引。
};

/// 三角网格的不可变拓扑缓存。
///
/// 缓存拥有稠密数组，因此算法可使用整数句柄遍历，而无需重复构建哈希表。
/// 当拓扑需要用于校验、修复、特征、简化或布尔运算预检时，可从 Mesh 构建该缓存。
class MeshTopology {
public:
    MANUMESH_API MeshTopology();
    MANUMESH_API ~MeshTopology();

    MANUMESH_API MeshTopology(const MeshTopology& other);
    MANUMESH_API MeshTopology& operator=(const MeshTopology& other);
    MANUMESH_API MeshTopology(MeshTopology&& other) noexcept;
    MANUMESH_API MeshTopology& operator=(MeshTopology&& other) noexcept;

    /**
     * @brief 为三角网格构建不可变的边和顶点入射关系。
     * @param[in] mesh 源网格；拓扑对象不会保留该对象。
     * @param[in] validate 读取面之前是否执行宽松几何校验。
     * @return 成功时返回拓扑，否则返回 invalid-argument/topology 状态。
     * @complexity 预期为 O(V + F)。
     */
    static MANUMESH_API Result<MeshTopology> build(const Mesh& mesh, bool validate = true);

    MANUMESH_API int vertexCount() const;
    MANUMESH_API int faceCount() const;
    MANUMESH_API int edgeCount() const;
    MANUMESH_API int boundaryEdgeCount() const;
    MANUMESH_API int nonManifoldEdgeCount() const;

    /// @return 由此对象拥有的稠密不可变边存储。
    MANUMESH_API const std::vector<TopologyEdge>& edges() const;
    /// 当 id 表示此拓扑中的边时返回 true。
    MANUMESH_API bool hasEdge(EdgeId id) const;
    /// 当 id 表示此拓扑中的顶点时返回 true。
    MANUMESH_API bool hasVertex(VertexId id) const;
    /// 返回请求的边；id 无效时抛出 std::out_of_range。
    MANUMESH_API const TopologyEdge& edge(EdgeId id) const;
    /// 返回请求的顶点拓扑；id 无效时抛出 std::out_of_range。
    MANUMESH_API const VertexTopology& vertex(VertexId id) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/// 将无向顶点对打包为稳定的 64 位键。
/// @param[in] a 第一个从零开始的顶点索引。
/// @param[in] b 第二个从零开始的顶点索引。
/// @return 较小索引位于高 32 位的键。
MANUMESH_API std::uint64_t topologyEdgeKey(int a, int b);

} // 命名空间 manumesh
