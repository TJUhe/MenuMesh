#pragma once

#include "line_quadrics_qem/algorithms/simplification/QEMSimplifier.h"

namespace lq::simplification {

void validateSimplifyOptions(const SimplifyOptions& options);
void validateSimplifierInput(const Mesh& input);

} // namespace lq::simplification
