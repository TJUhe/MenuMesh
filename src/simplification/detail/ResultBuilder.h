#pragma once

#include "core/Mesh.h"
#include "detail/SimplificationTypes.h"

#include <vector>

namespace manumesh::simplification {

Mesh compactResult(const std::vector<VertexState>& vertices,
                   const std::vector<FaceState>& faces);

} // namespace manumesh::simplification
