/**
 * @file include/algorithms/simplification/Metrics.h
 * @brief Declares metrics facilities for ManuMesh's simplification module.
 * @ingroup manumesh_simplification
 *
 * @details This file is part of the feature-aware edge-collapse pipeline. Quadric costs rank candidates; topology, geometry, feature, boundary, error, and optional texture policies decide whether a placement may mutate the mesh.
 */

#pragma once

#include "algorithms/analysis/MeshAnalysis.h"

#include <string>

namespace manumesh::simplification {

using MeshStats = manumesh::analysis::MeshStats;
using DistanceStats = manumesh::analysis::DistanceStats;

/// @deprecated Use manumesh::analysis::computeMeshStats.
MANUMESH_API MeshStats computeMeshStats(const Mesh& mesh);
/// @deprecated Use
/// manumesh::analysis::compareMeshesBySampledDistance.
MANUMESH_API DistanceStats compareMeshesBySampledDistance(const Mesh& original, const Mesh& simplified, int maxSamples);

/// @deprecated CSV presentation moved to `apps/CliCsv.h`.
MANUMESH_API std::string statsHeaderCsv();
/// @deprecated CSV presentation moved to `apps/CliCsv.h`.
MANUMESH_API std::string
statsRowCsv(const std::string& label, const MeshStats& stats, const DistanceStats* distance = nullptr);

} // namespace manumesh::simplification
