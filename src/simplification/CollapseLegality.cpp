#include "simplification/CollapseLegality.h"

#include "simplification/GeometryPredicates.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>

namespace lq {

CollapseRejectReason collapseRejectReason(int keep, int remove, const Vec3& newPosition,
                                          const std::vector<FaceState>& faces,
                                          const std::vector<VertexState>& vertices,
                                          const DynamicTopology& topology,
                                          double areaEps, double minTriangleQuality,
                                          double minNormalDot, double maxLocalError,
                                          bool preventLocalIntersections,
                                          const SpatialFaceIndex* spatialIndex) {
  if (!collapseWouldPreserveLinkCondition(keep, remove, faces, vertices, topology)) {
    return CollapseRejectReason::Topology;
  }

  std::unordered_set<int> touchedFaces = topology.vertexFaces[keep];
  touchedFaces.insert(topology.vertexFaces[remove].begin(),
                      topology.vertexFaces[remove].end());
  struct NewTriangle {
    int faceId = -1;
    std::array<int, 3> ids{};
    std::array<Vec3, 3> p{};
  };
  std::vector<NewTriangle> newTriangles;
  std::vector<Vec3> localReferencePoints;
  const bool measureLocalError = maxLocalError > 0.0;
  if (measureLocalError) {
    localReferencePoints.push_back(vertices[keep].p);
    localReferencePoints.push_back(vertices[remove].p);
  }
  for (int faceId : touchedFaces) {
    const FaceState& face = faces[faceId];
    if (!face.active) {
      continue;
    }
    for (int id : face.v) {
      if (id != keep && id != remove && vertices[id].active) {
        localReferencePoints.push_back(vertices[id].p);
      }
    }
    bool touches = false;
    std::array<int, 3> mapped = face.v;
    for (int& id : mapped) {
      if (id == keep || id == remove) {
        touches = true;
      }
      if (id == remove) {
        id = keep;
      }
    }
    const bool containsBoth =
        (face.v[0] == keep || face.v[1] == keep || face.v[2] == keep) &&
        (face.v[0] == remove || face.v[1] == remove || face.v[2] == remove);
    if (!touches || containsBoth) {
      continue;
    }
    if (mapped[0] == mapped[1] || mapped[1] == mapped[2] || mapped[0] == mapped[2]) {
      return CollapseRejectReason::Topology;
    }
    const Vec3 oldNormal = triangleNormal(vertices[face.v[0]].p, vertices[face.v[1]].p,
                                          vertices[face.v[2]].p);
    Vec3 a = vertices[mapped[0]].p;
    Vec3 b = vertices[mapped[1]].p;
    Vec3 c = vertices[mapped[2]].p;
    if (mapped[0] == keep) a = newPosition;
    if (mapped[1] == keep) b = newPosition;
    if (mapped[2] == keep) c = newPosition;
    const double area = triangleArea(a, b, c);
    if (area <= areaEps) {
      return CollapseRejectReason::Topology;
    }
    if (minTriangleQuality > 0.0 &&
        triangleQualityLocal(a, b, c) < minTriangleQuality) {
      return CollapseRejectReason::TriangleQuality;
    }
    if (minNormalDot > -1.0 && oldNormal.norm() > 1e-20) {
      const Vec3 newNormal = triangleNormal(a, b, c);
      if (newNormal.norm() <= 1e-20 || oldNormal.dot(newNormal) < minNormalDot) {
        return CollapseRejectReason::NormalFlip;
      }
    }
    if (preventLocalIntersections || measureLocalError) {
      newTriangles.push_back(NewTriangle{faceId, mapped, {a, b, c}});
    }
  }

  if (measureLocalError && !localReferencePoints.empty()) {
    if (newTriangles.empty()) {
      return CollapseRejectReason::LocalError;
    }
    const double maxError2 = maxLocalError * maxLocalError;
    for (const Vec3& point : localReferencePoints) {
      double best = std::numeric_limits<double>::infinity();
      for (const NewTriangle& tri : newTriangles) {
        best = std::min(best, pointTriangleDistanceSquaredLocal(point, tri.p[0],
                                                                tri.p[1], tri.p[2]));
      }
      if (!std::isfinite(best) || best > maxError2) {
        return CollapseRejectReason::LocalError;
      }
    }
  }

  if (preventLocalIntersections) {
    const double eps = std::sqrt(std::max(areaEps, 1e-30));
    auto sharesVertex = [](const std::array<int, 3>& a, const std::array<int, 3>& b) {
      for (int lhs : a) {
        for (int rhs : b) {
          if (lhs == rhs) {
            return true;
          }
        }
      }
      return false;
    };
    for (const NewTriangle& tri : newTriangles) {
      const auto [triLo, triHi] = triangleAabb(tri.p, eps);
      const std::vector<int> candidateFaces =
          spatialIndex ? spatialIndex->query(triLo, triHi) : std::vector<int>();
      const bool useSpatialCandidates = spatialIndex && spatialIndex->enabled();
      const int candidateCount = useSpatialCandidates
                                     ? static_cast<int>(candidateFaces.size())
                                     : static_cast<int>(faces.size());
      for (int candidateIndex = 0; candidateIndex < candidateCount; ++candidateIndex) {
        const int faceId =
            useSpatialCandidates ? candidateFaces[candidateIndex] : candidateIndex;
        if (!faces[faceId].active || touchedFaces.find(faceId) != touchedFaces.end()) {
          continue;
        }
        const FaceState& face = faces[faceId];
        if (sharesVertex(tri.ids, face.v)) {
          continue;
        }
        const std::array<Vec3, 3> other = {vertices[face.v[0]].p, vertices[face.v[1]].p,
                                           vertices[face.v[2]].p};
        if (trianglesIntersect(tri.p, other, eps)) {
          return CollapseRejectReason::SelfIntersection;
        }
      }
    }
  }
  return CollapseRejectReason::None;
}

} // namespace lq
