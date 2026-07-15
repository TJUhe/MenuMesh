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

} // namespace manumesh::api
