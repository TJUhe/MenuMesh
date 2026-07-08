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
MANUMESH_API PlainMesh simplifyPlainMesh(const PlainMesh& input,
                                         const SimplifyOptions& options,
                                         SimplifyReport* report = nullptr);

} // namespace manumesh::simplification
