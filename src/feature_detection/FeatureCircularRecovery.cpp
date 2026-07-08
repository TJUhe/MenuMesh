#include "detail/FeatureCircularRecovery.h"

#include "detail/FeatureLoopBuilder.h"
#include "detail/PrimitiveFit.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <unordered_set>

namespace manumesh::feature::detector_detail {
namespace {

using primitive_fit_detail::applyPrimitiveFit;
using primitive_fit_detail::fitPrimitive;
using primitive_fit_detail::PrimitiveFit;

std::string vertexSetSignature(std::vector<int> ids) {
  std::sort(ids.begin(), ids.end());
  std::ostringstream out;
  for (int id : ids) {
    out << id << ';';
  }
  return out.str();
}

struct ThreePointCircle {
  bool valid = false;
  Vec3 center = Vec3::Zero();
  Vec3 normal = Vec3(0.0, 0.0, 1.0);
  double radius = 0.0;
};

ThreePointCircle fitCircleFromThree(const Mesh& mesh, int ia, int ib, int ic) {
  ThreePointCircle result;
  const Vec3& a = mesh.vertices[ia];
  const Vec3& b = mesh.vertices[ib];
  const Vec3& c = mesh.vertices[ic];
  const Vec3 ab = b - a;
  const Vec3 ac = c - a;
  result.normal = ab.cross(ac);
  const double n2 = result.normal.squaredNorm();
  if (n2 <= 1e-20) {
    return result;
  }
  const Vec3 term1 = ac.squaredNorm() * result.normal.cross(ab);
  const Vec3 term2 = ab.squaredNorm() * ac.cross(result.normal);
  result.center = a + (term1 + term2) / (2.0 * n2);
  result.radius = (result.center - a).norm();
  result.normal.normalize();
  result.valid = std::isfinite(result.radius) && result.radius > 1e-20 &&
                 result.center.allFinite();
  return result;
}

std::vector<int> sortAroundCircle(std::vector<int> ids, const Mesh& mesh,
                                  const Vec3& center, const Vec3& normal) {
  Vec3 axisX = mesh.vertices[ids.front()] - center;
  axisX -= normal * axisX.dot(normal);
  if (axisX.norm() <= 1e-20) {
    axisX = std::abs(normal.x()) < 0.9 ? Vec3(1.0, 0.0, 0.0) : Vec3(0.0, 1.0, 0.0);
    axisX -= normal * axisX.dot(normal);
  }
  axisX.normalize();
  const Vec3 axisY = normal.cross(axisX).normalized();
  std::sort(ids.begin(), ids.end(), [&](int lhs, int rhs) {
    const Vec3 dl = mesh.vertices[lhs] - center;
    const Vec3 dr = mesh.vertices[rhs] - center;
    const double al = std::atan2(dl.dot(axisY), dl.dot(axisX));
    const double ar = std::atan2(dr.dot(axisY), dr.dot(axisX));
    return al < ar;
  });
  return ids;
}

double angularCoverage(const std::vector<int>& ids, const Mesh& mesh,
                       const Vec3& center, const Vec3& normal) {
  Vec3 axisX = mesh.vertices[ids.front()] - center;
  axisX -= normal * axisX.dot(normal);
  if (axisX.norm() <= 1e-20) {
    return 0.0;
  }
  axisX.normalize();
  const Vec3 axisY = normal.cross(axisX).normalized();
  std::vector<double> angles;
  angles.reserve(ids.size());
  for (int id : ids) {
    const Vec3 d = mesh.vertices[id] - center;
    double angle = std::atan2(d.dot(axisY), d.dot(axisX));
    if (angle < 0.0) {
      angle += 2.0 * kPi;
    }
    angles.push_back(angle);
  }
  std::sort(angles.begin(), angles.end());
  double maxGap = 0.0;
  for (int i = 0; i < static_cast<int>(angles.size()); ++i) {
    const double a = angles[i];
    const double b = i + 1 < static_cast<int>(angles.size())
                         ? angles[i + 1]
                         : angles.front() + 2.0 * kPi;
    maxGap = std::max(maxGap, b - a);
  }
  return 2.0 * kPi - maxGap;
}

std::vector<int> collectTraceVertices(const std::vector<char>& traceVertex) {
  std::vector<int> candidates;
  for (int id = 0; id < static_cast<int>(traceVertex.size()); ++id) {
    if (traceVertex[id]) {
      candidates.push_back(id);
    }
  }
  return candidates;
}

} // namespace

void recoverCircularVertexClusters(const Mesh& mesh, const FeatureOptions& options,
                                   const std::vector<char>& traceVertex,
                                   const std::vector<std::vector<int>>& adjacency,
                                   FeatureAnalysis& analysis, int& loopId) {
  if (analysis.normalTensorFeatureEdges > 0) {
    return;
  }

  const bool alreadyHasCircularLoop =
      std::any_of(analysis.loops.begin(), analysis.loops.end(),
                  [](const FeatureLoop& loop) { return loop.circular; });
  if (alreadyHasCircularLoop) {
    return;
  }

  const std::vector<int> candidates = collectTraceVertices(traceVertex);
  if (static_cast<int>(candidates.size()) < options.minFeatureLoopVertices ||
      candidates.size() > 120) {
    return;
  }

  // Keep the fallback bounded on fragmented CAD/STL feature graphs. The
  // deterministic cap makes the worst case predictable while still covering
  // small exported holes, which are the only intended input for this repair.
  constexpr int kMaxCircularClusterTripletScans = 32768;
  std::unordered_set<std::string> seenClusters;
  int tripletScans = 0;
  for (int i = 0; i < static_cast<int>(candidates.size()) &&
                  tripletScans < kMaxCircularClusterTripletScans;
       ++i) {
    for (int j = i + 1; j < static_cast<int>(candidates.size()) &&
                        tripletScans < kMaxCircularClusterTripletScans;
         ++j) {
      for (int k = j + 1; k < static_cast<int>(candidates.size()) &&
                          tripletScans < kMaxCircularClusterTripletScans;
           ++k) {
        ++tripletScans;
        const ThreePointCircle circle =
            fitCircleFromThree(mesh, candidates[i], candidates[j], candidates[k]);
        if (!circle.valid) {
          continue;
        }
        const double allowed =
            std::max(3.0 * options.circleFitRelativeThreshold, 0.08) * circle.radius;
        std::vector<int> cluster;
        for (int id : candidates) {
          const Vec3 delta = mesh.vertices[id] - circle.center;
          const double plane = std::abs(delta.dot(circle.normal));
          const Vec3 inPlane = delta - circle.normal * delta.dot(circle.normal);
          const double radial = std::abs(inPlane.norm() - circle.radius);
          if (std::max(plane, radial) <= allowed) {
            cluster.push_back(id);
          }
        }
        if (static_cast<int>(cluster.size()) < options.minFeatureLoopVertices ||
            angularCoverage(cluster, mesh, circle.center, circle.normal) < 1.5 * kPi) {
          continue;
        }
        const std::string signature = vertexSetSignature(cluster);
        if (!seenClusters.insert(signature).second) {
          continue;
        }

        cluster =
            sortAroundCircle(std::move(cluster), mesh, circle.center, circle.normal);
        FeatureLoop loop;
        loop.id = loopId++;
        loop.vertices = std::move(cluster);
        loop.edgeCount = static_cast<int>(loop.vertices.size());
        loop.closed = true;
        const PrimitiveFit fit = fitPrimitive(mesh, loop, options);
        applyPrimitiveFit(fit, loop);
        if (!loop.circular) {
          --loopId;
          continue;
        }
        assignLoopToVertices(loop, mesh, adjacency, analysis);
        analysis.loops.push_back(std::move(loop));
      }
    }
  }
}

} // namespace manumesh::feature::detector_detail
