#pragma once

#include "detail/DynamicTopology.h"
#include "detail/SpatialFaceIndex.h"
#include "line_quadrics_qem/core/Mesh.h"

#include <vector>

namespace lq {

struct MeshStateView {
  const std::vector<FaceState>& faces;
  const std::vector<VertexState>& vertices;
  const DynamicTopology& topology;
};

struct CollapseLegalityInput {
  CollapseEdge edge;
  Vec3 newPosition = Vec3::Zero();
  MeshStateView mesh;
  double areaEps = 0.0;
  double minTriangleQuality = 0.0;
  double minNormalDot = -1.0;
  double maxLocalError = 0.0;
  bool preventLocalIntersections = false;
  const SpatialFaceIndex* spatialIndex = nullptr;
};

CollapseRejectReason collapseRejectReason(const CollapseLegalityInput& input);

} // namespace lq
