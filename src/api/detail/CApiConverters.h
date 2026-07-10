#pragma once

#include "algorithms/simplification/Metrics.h"
#include "algorithms/simplification/SimplificationTypes.h"
#include "api/CApi.h"

#include <string>

namespace manumesh::api {

void initializeSimplifyOptions(ManuMeshSimplifyOptions& options);
void initializeSimplifyReport(ManuMeshSimplifyReport& report);
void initializeMeshStats(ManuMeshMeshStats& stats);

bool readSimplifyOptions(
    const ManuMeshSimplifyOptions& source, simplification::SimplifyOptions& target, std::string& error
);

bool validateSimplifyReportOutput(const ManuMeshSimplifyReport& target, std::string& error);
bool validateMeshStatsOutput(const ManuMeshMeshStats& target, std::string& error);
void fillSimplifyReport(const simplification::SimplifyReport& source, ManuMeshSimplifyReport& target);
void fillMeshStats(const simplification::MeshStats& source, ManuMeshMeshStats& target);

} // namespace manumesh::api
