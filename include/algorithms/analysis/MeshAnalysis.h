/**
 * @file include/algorithms/analysis/MeshAnalysis.h
 * @brief 声明 ManuMesh 分析模块的网格分析设施。
 * @ingroup manumesh_analysis
 *
 * @details 分析例程在文档说明处允许不可用面，并在不修改输入网格的情况下报告测量结果。
 */

#pragma once

#include "Export.h"
#include "core/Mesh.h"

namespace manumesh::analysis {

/**
 * @brief 基本几何和拓扑网格质量指标。
 *
 * 容器计数描述原始输入。几何字段仅使用有限、索引有效且未退化的面。
 * 长度使用模型单位，面积使用模型平方单位，质量位于 [0,1]，
 * edgeLengthCv 为无量纲变异系数。
 */
struct MeshStats {
    int vertices = 0;
    int faces = 0;
    int edges = 0;
    int boundaryEdges = 0;
    int nonManifoldEdges = 0;
    double area = 0.0;
    double meanTriangleQuality = 0.0;
    double minTriangleQuality = 0.0;
    double meanEdgeLength = 0.0;
    double edgeLengthCv = 0.0;
};

/// 两个网格之间的对称采样距离摘要。所有值使用网格自身的长度单位。
struct DistanceStats {
    double meanOriginalToSimplified = 0.0;
    double maxOriginalToSimplified = 0.0;
    double meanSimplifiedToOriginal = 0.0;
    double maxSimplifiedToOriginal = 0.0;
};

/// 计算网格质量和拓扑统计信息。`vertices` 和 `faces` 报告输入容器大小
/// （限制为 INT_MAX）；其他字段仅根据可用面计算，没有可用面时为零。
/// @param[in] mesh 要检查的网格；格式错误的面会跳过。
/// @return 确定性统计信息。不可用的量为零。
/// @complexity 预期为 O(V + F)。
/// @note 对不同的不可变网格并发调用时，此函数无副作用且线程安全。
MANUMESH_API MeshStats computeMeshStats(const Mesh& mesh);
/// 使用确定性表面采样估计双向网格距离。每个网格中的无效面独立跳过。
/// 当任一网格没有可用表面、maxSamples 非正，或无法产生有限距离采样时，
/// 所有字段均为零。
/// @param[in] original 第一个表面，通常是未简化的参考表面。
/// @param[in] simplified 用于与参考表面比较的第二个表面。
/// @param[in] maxSamples 每个方向抽取的最大确定性样本数。
/// @return 双向平均及最大点到表面距离。
/// @complexity O((F_o + F_s) log(F_o + F_s) + maxSamples log(F_o + F_s))。
MANUMESH_API DistanceStats compareMeshesBySampledDistance(const Mesh& original, const Mesh& simplified, int maxSamples);

} // 命名空间 manumesh::analysis
