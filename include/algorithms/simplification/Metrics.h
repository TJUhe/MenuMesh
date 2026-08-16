/**
 * @file include/algorithms/simplification/Metrics.h
 * @brief 保留旧版简化统计接口并转发到 analysis 模块。
 * @ingroup manumesh_simplification
 *
 * @details 新代码应直接使用 algorithms/analysis/MeshAnalysis.h；这里的别名和函数仅用于源码迁移。
 */

#pragma once

#include "algorithms/analysis/MeshAnalysis.h"

#include <string>

namespace manumesh {
namespace simplification {

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

} // namespace simplification
} // namespace manumesh
