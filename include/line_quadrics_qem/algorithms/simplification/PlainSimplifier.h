#pragma once

#include "line_quadrics_qem/Export.h"
#include "line_quadrics_qem/algorithms/simplification/SimplificationTypes.h"
#include "line_quadrics_qem/core/PlainMesh.h"

namespace lq {

/// Simplifies a mesh through the Eigen-free C++ exchange type.
///
/// This entry point keeps host-facing C++ boundaries independent from Eigen.
/// The implementation converts to the internal Eigen-backed mesh, runs the same
/// simplifier as `simplifyMesh`, and converts the result back to `PlainMesh`.
LQ_API PlainMesh simplifyPlainMesh(const PlainMesh& input,
                                   const SimplifyOptions& options,
                                   SimplifyReport* report = nullptr);

} // namespace lq
