#pragma once

#include "detail/SimplificationTypes.h"
#include "manumesh/core/Mesh.h"

#include <vector>

namespace manumesh::simplification {

Mesh compactResult(const std::vector<VertexState>& vertices,
                   const std::vector<FaceState>& faces);

} // namespace manumesh::simplification
