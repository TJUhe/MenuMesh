/**
 * @file include/algorithms/simplification/PlainSimplifier.h
 * @brief Declares plain simplifier facilities for ManuMesh's simplification module.
 * @ingroup manumesh_simplification
 *
 * @details This file is part of the feature-aware edge-collapse pipeline. Quadric costs rank candidates; topology, geometry, feature, boundary, error, and optional texture policies decide whether a placement may mutate the mesh.
 */

#pragma once

#include "Export.h"
#include "algorithms/simplification/SimplificationTypes.h"
#include "core/PlainMesh.h"

namespace manumesh::simplification {

/// Simplifies a mesh through the Eigen-free C++ exchange type.
///
/// This entry point keeps host-facing C++ boundaries independent from Eigen.
/// The implementation converts to the internal Eigen-backed mesh, runs the same
/// simplifier as `simplifyMesh`, and converts the result back to `PlainMesh`.
/// @param[in] input Eigen-free triangle mesh exchange value.
/// @param[in] options Simplification target, costs, and hard policies.
/// @param[out] report Optional run diagnostics.
/// @return Simplified Eigen-free mesh.
/// @throws std::invalid_argument when input or options violate the C++ API contract.
MANUMESH_API PlainMesh
simplifyPlainMesh(const PlainMesh& input, const SimplifyOptions& options, SimplifyReport* report = nullptr);

} // namespace manumesh::simplification
