/**
 * @file include/algorithms/simplification/Metrics.h
 * @brief 声明 ManuMesh 简化模块的度量设施。
 * @ingroup manumesh_simplification
 *
 * @details 此文件属于面向特征的边坍缩管线。二次误差代价负责候选排序；拓扑、几何、特征、边界、误差和可选纹理策略共同决定位置是否可以修改网格。
 */

#pragma once

#include "algorithms/analysis/MeshAnalysis.h"

#include <string>

namespace manumesh::simplification {

using MeshStats = manumesh::analysis::MeshStats;
using DistanceStats = manumesh::analysis::DistanceStats;

/// @deprecated 请使用 manumesh::analysis::computeMeshStats。
MANUMESH_API MeshStats computeMeshStats(const Mesh& mesh);
/// @deprecated 请使用
/// manumesh::analysis::compareMeshesBySampledDistance。
MANUMESH_API DistanceStats compareMeshesBySampledDistance(const Mesh& original, const Mesh& simplified, int maxSamples);

/// @deprecated CSV 展示功能已移至 `apps/CliCsv.h`。
MANUMESH_API std::string statsHeaderCsv();
/// @deprecated CSV 展示功能已移至 `apps/CliCsv.h`。
MANUMESH_API std::string
statsRowCsv(const std::string& label, const MeshStats& stats, const DistanceStats* distance = nullptr);

} // namespace manumesh::simplification
