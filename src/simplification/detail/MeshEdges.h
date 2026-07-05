#pragma once

#include "detail/SimplificationTypes.h"
#include "line_quadrics_qem/core/Mesh.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace lq {

struct EdgeInfo {
  std::vector<int> faces;
};

std::unordered_map<std::uint64_t, EdgeInfo> buildEdgeInfo(const Mesh& mesh);
std::vector<char> computeBoundaryVertices(const Mesh& mesh);

} // namespace lq
