#include "detail/MeshEdges.h"

namespace lq {

std::unordered_map<std::uint64_t, EdgeInfo> buildEdgeInfo(const Mesh& mesh) {
  std::unordered_map<std::uint64_t, EdgeInfo> edges;
  edges.reserve(mesh.faces.size() * 3);
  for (int fi = 0; fi < static_cast<int>(mesh.faces.size()); ++fi) {
    const Face& face = mesh.faces[fi];
    for (int e = 0; e < 3; ++e) {
      edges[edgeKey(face.v[e], face.v[(e + 1) % 3])].faces.push_back(fi);
    }
  }
  return edges;
}

std::vector<char> computeBoundaryVertices(const Mesh& mesh) {
  std::vector<char> boundary(mesh.vertices.size(), 0);
  const auto edgeInfo = buildEdgeInfo(mesh);
  for (const auto& [key, info] : edgeInfo) {
    if (info.faces.size() == 1) {
      const auto [a, b] = unpackEdgeKey(key);
      if (a >= 0 && a < static_cast<int>(boundary.size())) {
        boundary[a] = 1;
      }
      if (b >= 0 && b < static_cast<int>(boundary.size())) {
        boundary[b] = 1;
      }
    }
  }
  return boundary;
}

} // namespace lq
