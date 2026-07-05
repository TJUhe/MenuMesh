#pragma once

#include "detail/DynamicTopology.h"
#include "detail/SpatialFaceIndex.h"
#include "line_quadrics_qem/core/Mesh.h"

#include <vector>

namespace lq {

CollapseRejectReason collapseRejectReason(int keep, int remove, const Vec3& newPosition,
                                          const std::vector<FaceState>& faces,
                                          const std::vector<VertexState>& vertices,
                                          const DynamicTopology& topology,
                                          double areaEps, double minTriangleQuality,
                                          double minNormalDot, double maxLocalError,
                                          bool preventLocalIntersections,
                                          const SpatialFaceIndex* spatialIndex);

} // namespace lq
