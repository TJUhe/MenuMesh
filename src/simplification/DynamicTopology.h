#pragma once

#include "line_quadrics_qem/simplification/QEMSimplifier.h"
#include "simplification/SimplificationTypes.h"

#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace lq {

bool containsVertex(const FaceState& face, int vertex);

struct DynamicTopology {
  std::vector<std::unordered_set<int>> vertexFaces;
  std::unordered_map<std::array<int, 3>, std::unordered_set<int>, FaceKeyHash>
      facesByKey;

  DynamicTopology(const std::vector<FaceState>& faces, int vertexCount);

  void addFace(int faceId, const FaceState& face);
  void removeFace(int faceId, const FaceState& face);
  bool hasDuplicateFace(int faceId, const FaceState& face) const;
};

std::vector<std::pair<int, int>>
collectActiveEdges(const std::vector<FaceState>& faces);

bool areAdjacent(int a, int b, const std::vector<FaceState>& faces,
                 const DynamicTopology& topology);

int activeIncidentFaceCountForEdge(int a, int b, const std::vector<FaceState>& faces,
                                   const DynamicTopology& topology);

BoundaryCollapseDecision
boundaryCollapseDecision(int keep, int remove, const std::vector<FaceState>& faces,
                         const std::vector<VertexState>& vertices,
                         const DynamicTopology& topology,
                         const SimplifyOptions& options);

std::vector<int> activeNeighborsOf(int v, const std::vector<FaceState>& faces,
                                   const std::vector<VertexState>& vertices,
                                   const DynamicTopology& topology);

std::unordered_set<int> activeLinkOf(int vertex, const std::vector<FaceState>& faces,
                                     const std::vector<VertexState>& vertices,
                                     const DynamicTopology& topology,
                                     int excludedVertex);

bool collapseWouldPreserveLinkCondition(int keep, int remove,
                                        const std::vector<FaceState>& faces,
                                        const std::vector<VertexState>& vertices,
                                        const DynamicTopology& topology);

} // namespace lq
