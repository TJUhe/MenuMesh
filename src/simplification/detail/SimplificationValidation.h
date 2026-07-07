#pragma once

#include "manumesh/algorithms/simplification/QEMSimplifier.h"

namespace manumesh::simplification {

void validateSimplifyOptions(const SimplifyOptions& options);
void validateSimplifierInput(const Mesh& input);

} // namespace manumesh::simplification
