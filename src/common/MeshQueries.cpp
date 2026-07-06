#include "common/detail/MeshQueries.h"

#include <algorithm>
#include <unordered_set>

namespace lq::detail {

std::uint64_t meshEdgeKey(int a, int b) {
  if (a > b) std::swap(a, b);
  return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(a)) << 32u) |
         static_cast<std::uint32_t>(b);
}

std::pair<int, int> unpackMeshEdgeKey(std::uint64_t key) {
  return {static_cast<int>(key >> 32u), static_cast<int>(key & 0xffffffffu)};
}

std::array<int, 3> sortedFaceKey(std::array<int, 3> ids) {
  std::sort(ids.begin(), ids.end());
  return ids;
}

std::size_t FaceKeyHash::operator()(const std::array<int, 3>& ids) const {
  return static_cast<std::size_t>(ids[0]) * 73856093u ^
         static_cast<std::size_t>(ids[1]) * 19349663u ^
         static_cast<std::size_t>(ids[2]) * 83492791u;
}

MeshEdgeInfoMap buildMeshEdgeInfo(const Mesh& mesh) {
  MeshEdgeInfoMap edges;
  edges.reserve(mesh.faces.size() * 3);
  for (int fi = 0; fi < static_cast<int>(mesh.faces.size()); ++fi) {
    const Face& face = mesh.faces[fi];
    for (int e = 0; e < 3; ++e) {
      edges[meshEdgeKey(face.v[e], face.v[(e + 1) % 3])].faces.push_back(fi);
    }
  }
  return edges;
}

std::vector<Vec3> computeFaceNormals(const Mesh& mesh) {
  std::vector<Vec3> normals(mesh.faces.size(), Vec3::Zero());
  for (int fi = 0; fi < static_cast<int>(mesh.faces.size()); ++fi) {
    const Face& f = mesh.faces[fi];
    normals[fi] = triangleNormal(mesh.vertices[f.v[0]], mesh.vertices[f.v[1]],
                                 mesh.vertices[f.v[2]]);
  }
  return normals;
}

Vec3 faceCentroid(const Mesh& mesh, const Face& face) {
  return (mesh.vertices[face.v[0]] + mesh.vertices[face.v[1]] +
          mesh.vertices[face.v[2]]) /
         3.0;
}

std::vector<std::vector<int>> buildVertexNeighbors(const Mesh& mesh) {
  std::vector<std::unordered_set<int>> neighborSets(mesh.vertices.size());
  for (const Face& face : mesh.faces) {
    for (int i = 0; i < 3; ++i) {
      const int a = face.v[i];
      const int b = face.v[(i + 1) % 3];
      if (a >= 0 && b >= 0 && a < static_cast<int>(neighborSets.size()) &&
          b < static_cast<int>(neighborSets.size()) && a != b) {
        neighborSets[a].insert(b);
        neighborSets[b].insert(a);
      }
    }
  }

  std::vector<std::vector<int>> neighbors(mesh.vertices.size());
  for (int i = 0; i < static_cast<int>(neighborSets.size()); ++i) {
    neighbors[i].assign(neighborSets[i].begin(), neighborSets[i].end());
  }
  return neighbors;
}

std::vector<char> computeBoundaryVertices(const Mesh& mesh) {
  std::vector<char> boundary(mesh.vertices.size(), 0);
  const MeshEdgeInfoMap edgeInfo = buildMeshEdgeInfo(mesh);
  for (const auto& [key, info] : edgeInfo) {
    if (info.faces.size() == 1) {
      const auto [a, b] = unpackMeshEdgeKey(key);
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

} // namespace lq::detail
