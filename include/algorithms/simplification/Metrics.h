#pragma once

// DEPRECATED forwarding header.
//
// The generic mesh statistics moved to the cross-algorithm analysis module:
// use "algorithms/analysis/MeshAnalysis.h" and manumesh::analysis instead.
// The CSV helpers (statsHeaderCsv/statsRowCsv) were presentation-layer code
// and moved into the manumesh CLI (apps/manumesh/CliCsv.h). Deprecated
// compatibility wrappers remain here for one migration cycle.
//
// New code should include MeshAnalysis.h directly. These aliases and wrappers
// are source-compatibility aids for the pre-1.0 C++ SDK; applications must
// still rebuild when changing ManuMesh C++ SDK versions. The stable binary
// boundary is the C ABI in api/CApi.h.
#include "algorithms/analysis/MeshAnalysis.h"

#include <string>

namespace manumesh::simplification {

using MeshStats = manumesh::analysis::MeshStats;
using DistanceStats = manumesh::analysis::DistanceStats;

/// Deprecated compatibility wrapper; use manumesh::analysis::computeMeshStats.
MANUMESH_API MeshStats computeMeshStats(const Mesh& mesh);
/// Deprecated compatibility wrapper; use
/// manumesh::analysis::compareMeshesBySampledDistance.
MANUMESH_API DistanceStats
compareMeshesBySampledDistance(const Mesh& original, const Mesh& simplified, int maxSamples);

/// Deprecated compatibility wrapper for the historical CSV presentation API.
MANUMESH_API std::string statsHeaderCsv();
/// Deprecated compatibility wrapper for the historical CSV presentation API.
MANUMESH_API std::string
statsRowCsv(const std::string& label, const MeshStats& stats, const DistanceStats* distance = nullptr);

} // namespace manumesh::simplification
