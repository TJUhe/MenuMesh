/**
 * @file src/api/detail/CApiConverters.h
 * @brief 声明 ManuMesh 的C ABI 模块的C API 转换器功能。
 * @ingroup manumesh_c_api
 *
 * @details 声明 C ABI 结构体与内部 C++ 结果之间的有界转换辅助函数。
 */

#pragma once

#include "algorithms/analysis/MeshAnalysis.h"
#include "algorithms/simplification/SimplificationTypes.h"
#include "api/CApi.h"

#include <cstddef>
#include <string>

namespace manumesh::api {

ManuMeshStatus initializeSimplifyOptions(ManuMeshSimplifyOptions* options, std::size_t structCapacity);
ManuMeshStatus initializeSimplifyReport(ManuMeshSimplifyReport* report, std::size_t structCapacity);
ManuMeshStatus initializeMeshStats(ManuMeshMeshStats* stats, std::size_t structCapacity);

bool readSimplifyOptions(
    const ManuMeshSimplifyOptions& source, simplification::SimplifyOptions& target, std::string& error
);

bool validateSimplifyReportOutput(const ManuMeshSimplifyReport* target, std::size_t structCapacity, std::string& error);
bool validateMeshStatsOutput(const ManuMeshMeshStats* target, std::size_t structCapacity, std::string& error);
ManuMeshStatus fillSimplifyReport(
    const simplification::SimplifyReport& source, ManuMeshSimplifyReport* target, std::size_t structCapacity
);
ManuMeshStatus fillMeshStats(const analysis::MeshStats& source, ManuMeshMeshStats* target, std::size_t structCapacity);

} // manumesh::api 命名空间
