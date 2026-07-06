#pragma once

#include "line_quadrics_qem/core/Mesh.h"
#include "line_quadrics_qem/features/FeatureDetection.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

namespace lq {

/// Mutable vertex record used only during one simplification run.
///
/// The fields are kept flat because the hot path reads them from several
/// modules. Conceptually they form three groups: geometry/QEM state, feature
/// ownership, and queue invalidation.
struct VertexState {
  // Geometry and QEM state.
  Vec3 p = Vec3::Zero();
  Mat4 q = Mat4::Zero();
  bool active = true;

  // Feature ownership and primitive-fit data copied from FeatureAnalysis.
  bool isFeature = false;
  bool isBoundary = false;
  bool circularFeature = false;
  bool featureJunction = false;
  FeaturePrimitiveType featurePrimitive = FeaturePrimitiveType::Unknown;
  int featureLoopId = -1;
  Vec3 curveTangent = Vec3::Zero();
  Vec3 circleCenter = Vec3::Zero();
  Vec3 circleNormal = Vec3(0.0, 0.0, 1.0);
  double circleRadius = 0.0;
  Vec3 ellipseCenter = Vec3::Zero();
  Vec3 ellipseNormal = Vec3(0.0, 0.0, 1.0);
  Vec3 ellipseMajorAxis = Vec3(1.0, 0.0, 0.0);
  Vec3 ellipseMinorAxis = Vec3(0.0, 1.0, 0.0);
  double ellipseMajorRadius = 0.0;
  double ellipseMinorRadius = 0.0;

  // Incremented after collapse so queued candidates can detect stale endpoints.
  int version = 0;
};

/// Mutable triangle record used by the active topology during simplification.
struct FaceState {
  std::array<int, 3> v{};
  bool active = true;
};

/// Directed edge-collapse choice: keep one endpoint and remove the other.
struct CollapseEdge {
  int keep = -1;
  int remove = -1;
};

/// Priority-queue entry. The comparison is reversed for std::priority_queue so
/// the lowest-cost candidate is popped first.
struct Candidate {
  double cost = 0.0;
  int a = -1;
  int b = -1;
  int versionA = 0;
  int versionB = 0;

  bool operator<(const Candidate& other) const { return cost > other.cost; }
};

/// Candidate collapse placement and its evaluated quadric cost.
struct SolveResult {
  Vec3 position = Vec3::Zero();
  double cost = 0.0;
  bool usedFallback = false;
};

enum class FeatureCollapseRejectKind {
  None,
  Primitive,
  Generic,
};

enum class CollapseRejectReason {
  None,
  Topology,
  NormalFlip,
  TriangleQuality,
  SelfIntersection,
  LocalError,
};

struct BoundaryCollapseDecision {
  bool allowed = true;
  bool boundaryEdge = false;
};

struct FeatureCurveConstraint {
  bool valid = false;
  bool closed = false;
  FeaturePrimitiveType primitive = FeaturePrimitiveType::Unknown;
  std::vector<Vec3> samples;
};

struct CellCoord {
  int x = 0;
  int y = 0;
  int z = 0;

  bool operator==(const CellCoord& other) const {
    return x == other.x && y == other.y && z == other.z;
  }
};

struct CellCoordHash {
  std::size_t operator()(const CellCoord& cell) const {
    std::size_t seed = 1469598103934665603ull;
    auto mix = [&](int value) {
      seed ^= static_cast<std::size_t>(static_cast<std::uint32_t>(value));
      seed *= 1099511628211ull;
    };
    mix(cell.x);
    mix(cell.y);
    mix(cell.z);
    return seed;
  }
};

inline std::uint64_t edgeKey(int a, int b) {
  if (a > b) std::swap(a, b);
  return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(a)) << 32u) |
         static_cast<std::uint32_t>(b);
}

inline std::array<int, 3> faceKey(std::array<int, 3> ids) {
  std::sort(ids.begin(), ids.end());
  return ids;
}

struct FaceKeyHash {
  std::size_t operator()(const std::array<int, 3>& ids) const {
    return static_cast<std::size_t>(ids[0]) * 73856093u ^
           static_cast<std::size_t>(ids[1]) * 19349663u ^
           static_cast<std::size_t>(ids[2]) * 83492791u;
  }
};

inline std::pair<int, int> unpackEdgeKey(std::uint64_t key) {
  return {static_cast<int>(key >> 32u), static_cast<int>(key & 0xffffffffu)};
}

} // namespace lq
