#pragma once

#include "detail/SimplificationTypes.h"
#include "line_quadrics_qem/core/Mesh.h"

#include <vector>

namespace lq::simplification {

Mesh compactResult(const std::vector<VertexState>& vertices,
                   const std::vector<FaceState>& faces);

} // namespace lq::simplification
