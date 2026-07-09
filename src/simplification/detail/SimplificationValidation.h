#pragma once

#include "algorithms/simplification/SimplificationTypes.h"
#include "core/Mesh.h"

namespace manumesh::simplification {

void validateSimplifyOptions(const SimplifyOptions& options);
void validateSimplifierInput(const Mesh& input);

} // namespace manumesh::simplification
