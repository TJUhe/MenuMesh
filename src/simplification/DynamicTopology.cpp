#include "simplification/DynamicTopology.h"

#include <algorithm>
#include <cstdint>

namespace lq {

bool containsVertex(const FaceState& face, int vertex) {
  return face.v[0] == vertex || face.v[1] == vertex || face.v[2] == vertex;
}

DynamicTopology::DynamicTopology(const std::vector<FaceState>& faces, int vertexCount) {
  vertexFaces.resize(vertexCount);
  for (int fi = 0; fi < static_cast<int>(faces.size()); ++fi) {
    if (faces[fi].active) {
      addFace(fi, faces[fi]);
    }
  }
}

void DynamicTopology::addFace(int faceId, const FaceState& face) {
  for (int id : face.v) {
    if (id >= 0 && id < static_cast<int>(vertexFaces.size())) {
      vertexFaces[id].insert(faceId);
    }
  }
  facesByKey[faceKey(face.v)].insert(faceId);
}

void DynamicTopology::removeFace(int faceId, const FaceState& face) {
  for (int id : face.v) {
    if (id >= 0 && id < static_cast<int>(vertexFaces.size())) {
      vertexFaces[id].erase(faceId);
    }
  }
  const auto key = faceKey(face.v);
  auto it = facesByKey.find(key);
  if (it != facesByKey.end()) {
    it->second.erase(faceId);
    if (it->second.empty()) {
      facesByKey.erase(it);
    }
  }
}

bool DynamicTopology::hasDuplicateFace(int faceId, const FaceState& face) const {
  const auto it = facesByKey.find(faceKey(face.v));
  if (it == facesByKey.end()) {
    return false;
  }
  for (int id : it->second) {
    if (id != faceId) {
      return true;
    }
  }
  return false;
}

std::vector<std::pair<int, int>>
collectActiveEdges(const std::vector<FaceState>& faces) {
  std::unordered_set<std::uint64_t> seen;
  std::vector<std::pair<int, int>> edges;
  for (const FaceState& face : faces) {
    if (!face.active) {
      continue;
    }
    for (int e = 0; e < 3; ++e) {
      int a = face.v[e];
      int b = face.v[(e + 1) % 3];
      if (a == b) {
        continue;
      }
      const std::uint64_t key = edgeKey(a, b);
      if (seen.insert(key).second) {
        if (a > b) std::swap(a, b);
        edges.emplace_back(a, b);
      }
    }
  }
  return edges;
}

bool areAdjacent(int a, int b, const std::vector<FaceState>& faces,
                 const DynamicTopology& topology) {
  if (a < 0 || b < 0 || a >= static_cast<int>(topology.vertexFaces.size()) ||
      b >= static_cast<int>(topology.vertexFaces.size())) {
    return false;
  }
  const auto& aFaces = topology.vertexFaces[a];
  const auto& bFaces = topology.vertexFaces[b];
  const auto& smaller = aFaces.size() <= bFaces.size() ? aFaces : bFaces;
  const auto& larger = aFaces.size() <= bFaces.size() ? bFaces : aFaces;
  for (int faceId : smaller) {
    if (larger.find(faceId) != larger.end() && faces[faceId].active) {
      return true;
    }
  }
  return false;
}

int activeIncidentFaceCountForEdge(int a, int b, const std::vector<FaceState>& faces,
                                   const DynamicTopology& topology) {
  if (a < 0 || b < 0 || a >= static_cast<int>(topology.vertexFaces.size()) ||
      b >= static_cast<int>(topology.vertexFaces.size())) {
    return 0;
  }
  const auto& aFaces = topology.vertexFaces[a];
  const auto& bFaces = topology.vertexFaces[b];
  const auto& smaller = aFaces.size() <= bFaces.size() ? aFaces : bFaces;
  const auto& larger = aFaces.size() <= bFaces.size() ? bFaces : aFaces;
  int count = 0;
  for (int faceId : smaller) {
    if (larger.find(faceId) != larger.end() && faces[faceId].active) {
      ++count;
    }
  }
  return count;
}

BoundaryCollapseDecision
boundaryCollapseDecision(int keep, int remove, const std::vector<FaceState>& faces,
                         const std::vector<VertexState>& vertices,
                         const DynamicTopology& topology,
                         const SimplifyOptions& options) {
  if (!options.preserveBoundary) {
    return {};
  }

  const bool keepBoundary = vertices[keep].isBoundary;
  const bool removeBoundary = vertices[remove].isBoundary;
  if (!keepBoundary && !removeBoundary) {
    return {};
  }
  if (keepBoundary != removeBoundary) {
    return {false, false};
  }

  const bool isCurrentBoundaryEdge =
      activeIncidentFaceCountForEdge(keep, remove, faces, topology) == 1;
  return {isCurrentBoundaryEdge, isCurrentBoundaryEdge};
}

std::vector<int> activeNeighborsOf(int v, const std::vector<FaceState>& faces,
                                   const std::vector<VertexState>& vertices,
                                   const DynamicTopology& topology) {
  std::unordered_set<int> seen;
  if (v < 0 || v >= static_cast<int>(topology.vertexFaces.size())) {
    return {};
  }
  for (int faceId : topology.vertexFaces[v]) {
    const FaceState& face = faces[faceId];
    if (!face.active) {
      continue;
    }
    for (int id : face.v) {
      if (id != v && vertices[id].active) {
        seen.insert(id);
      }
    }
  }
  return std::vector<int>(seen.begin(), seen.end());
}

std::unordered_set<int> activeLinkOf(int vertex, const std::vector<FaceState>& faces,
                                     const std::vector<VertexState>& vertices,
                                     const DynamicTopology& topology,
                                     int excludedVertex) {
  std::unordered_set<int> link;
  if (vertex < 0 || vertex >= static_cast<int>(topology.vertexFaces.size())) {
    return link;
  }
  for (int faceId : topology.vertexFaces[vertex]) {
    const FaceState& face = faces[faceId];
    if (!face.active) {
      continue;
    }
    for (int id : face.v) {
      if (id != vertex && id != excludedVertex && vertices[id].active) {
        link.insert(id);
      }
    }
  }
  return link;
}

bool collapseWouldPreserveLinkCondition(int keep, int remove,
                                        const std::vector<FaceState>& faces,
                                        const std::vector<VertexState>& vertices,
                                        const DynamicTopology& topology) {
  std::unordered_set<int> edgeLink;
  int incidentFaceCount = 0;
  const auto& keepFaces = topology.vertexFaces[keep];
  const auto& removeFaces = topology.vertexFaces[remove];
  const auto& smaller =
      keepFaces.size() <= removeFaces.size() ? keepFaces : removeFaces;
  const auto& larger = keepFaces.size() <= removeFaces.size() ? removeFaces : keepFaces;
  for (int faceId : smaller) {
    if (larger.find(faceId) == larger.end()) {
      continue;
    }
    const FaceState& face = faces[faceId];
    if (!face.active) {
      continue;
    }
    ++incidentFaceCount;
    for (int id : face.v) {
      if (id != keep && id != remove && vertices[id].active) {
        edgeLink.insert(id);
      }
    }
  }

  if (incidentFaceCount <= 0 || incidentFaceCount > 2 ||
      edgeLink.size() != static_cast<std::size_t>(incidentFaceCount)) {
    return false;
  }

  const std::unordered_set<int> keepLink =
      activeLinkOf(keep, faces, vertices, topology, remove);
  const std::unordered_set<int> removeLink =
      activeLinkOf(remove, faces, vertices, topology, keep);
  std::unordered_set<int> intersection;
  for (int id : keepLink) {
    if (removeLink.find(id) != removeLink.end()) {
      intersection.insert(id);
    }
  }

  if (intersection.size() != edgeLink.size()) {
    return false;
  }
  for (int id : intersection) {
    if (edgeLink.find(id) == edgeLink.end()) {
      return false;
    }
  }
  return true;
}

} // namespace lq
