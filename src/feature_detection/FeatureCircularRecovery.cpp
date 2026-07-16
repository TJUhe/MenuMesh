/**
 * @file src/feature_detection/FeatureCircularRecovery.cpp
 * @brief Implements feature circular recovery facilities for ManuMesh's feature-detection module.
 * @ingroup manumesh_feature_detection
 *
 * @details Recovers sparse circular vertex clusters missed by strict graph tracing.
 * @algorithm Groups compatible feature vertices in a fitted plane, orders them
 * angularly around a circle candidate, and validates radial/planar residuals
 * plus graph support before materializing a fallback loop.
 * @failuremodes This bounded CAD-oriented fallback rejects partial arcs and
 * clusters whose angular coverage or residuals do not support a closed circle.
 */

#include "detail/FeatureCircularRecovery.h"

#include "detail/FeatureGraph.h"
#include "detail/FeatureLoopBuilder.h"
#include "detail/PrimitiveFit.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <queue>
#include <utility>

namespace manumesh::feature::detector_detail {
namespace {

using primitive_fit_detail::applyPrimitiveFit;
using primitive_fit_detail::fitPrimitive;
using primitive_fit_detail::PrimitiveFit;

CycleSignature vertexSetSignature(const std::vector<int>& ids) {
    CycleSignature signature;
    signature.reserve(ids.size());
    for (int id : ids) {
        signature.push_back(static_cast<std::uint64_t>(id));
    }
    std::sort(signature.begin(), signature.end());
    return signature;
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
    result.valid = std::isfinite(result.radius) && result.radius > 1e-20 && result.center.allFinite();
    return result;
}

std::vector<int> sortAroundCircle(std::vector<int> ids, const Mesh& mesh, const Vec3& center, const Vec3& normal) {
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

double angularCoverage(const std::vector<int>& ids, const Mesh& mesh, const Vec3& center, const Vec3& normal) {
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
        const double b = i + 1 < static_cast<int>(angles.size()) ? angles[i + 1] : angles.front() + 2.0 * kPi;
        maxGap = std::max(maxGap, b - a);
    }
    return 2.0 * kPi - maxGap;
}

/// Maximum fraction of the full turn that may be spanned by consecutive
/// cluster vertices without a supporting feature edge between them. The
/// recovery exists to close small evidence gaps (arc segments dropped by
/// thresholding), not to invent circles, so most of the circle must already
/// be linked by trace-graph edges. Angles make the bound dimensionless and
/// invariant under uniform scaling; 0.25 matches the pre-existing 1.5*pi
/// angular-coverage requirement (at most a quarter turn missing).
constexpr double kMaxCircularRecoveryGapFraction = 0.25;

/// Evidence-connectivity gate for a recovered circle: walking the cluster in
/// circular order (the caller passes it already sorted around the fitted
/// circle), every consecutive pair should be a feature edge of the trace
/// graph; the angular extents of the unsupported pairs are summed and bounded
/// by kMaxCircularRecoveryGapFraction of the full turn. Purely geometric
/// concyclicity is not evidence: vertex sets whose members are not linked by
/// feature edges (e.g. the coplanar corner rows of a chamfered prism) must
/// not be stitched into a circle.
bool clusterSupportedByTraceEdges(
    const std::vector<int>& sortedCluster,
    const Mesh& mesh,
    const TraceGraph& trace,
    const Vec3& center,
    const Vec3& normal
) {
    Vec3 axisX = mesh.vertices[sortedCluster.front()] - center;
    axisX -= normal * axisX.dot(normal);
    if (axisX.norm() <= 1e-20) {
        return false;
    }
    axisX.normalize();
    const Vec3 axisY = normal.cross(axisX).normalized();
    const auto angleOf = [&](int id) {
        const Vec3 d = mesh.vertices[id] - center;
        return std::atan2(d.dot(axisY), d.dot(axisX));
    };
    double unsupportedAngle = 0.0;
    const int count = static_cast<int>(sortedCluster.size());
    for (int i = 0; i < count; ++i) {
        const int a = sortedCluster[i];
        const int b = sortedCluster[(i + 1) % count];
        if (traceGraphHasEdge(trace, a, b)) {
            continue;
        }
        double gap = angleOf(b) - angleOf(a);
        if (gap < 0.0) {
            gap += 2.0 * kPi;
        }
        unsupportedAngle += gap;
        if (unsupportedAngle > kMaxCircularRecoveryGapFraction * 2.0 * kPi) {
            return false;
        }
    }
    return true;
}

struct TraceComponent {
    std::vector<int> vertices;
    bool hasWeakEvidenceEdge = false;
    bool alreadyHasCircularLoop = false;
};

std::vector<TraceComponent> collectTraceComponents(const TraceGraph& trace, const FeatureAnalysis& analysis) {
    std::vector<TraceComponent> components;
    std::vector<char> visited(trace.adjacency.size(), 0);
    for (int seed = 0; seed < static_cast<int>(trace.adjacency.size()); ++seed) {
        if (visited[seed] || trace.adjacency[seed].empty()) {
            continue;
        }
        TraceComponent component;
        std::queue<int> queue;
        queue.push(seed);
        visited[seed] = 1;
        while (!queue.empty()) {
            const int v = queue.front();
            queue.pop();
            component.vertices.push_back(v);
            if (v >= 0 && v < static_cast<int>(analysis.vertices.size())) {
                component.alreadyHasCircularLoop = component.alreadyHasCircularLoop || analysis.vertices[v].circular;
            }
            for (int nb : trace.adjacency[v]) {
                if (!component.hasWeakEvidenceEdge) {
                    const TraceEdgeAttrs* attrs = traceEdgeAttrs(trace, v, nb);
                    component.hasWeakEvidenceEdge = attrs != nullptr && (attrs->normalTensor || attrs->smoothCurvature);
                }
                if (!visited[nb]) {
                    visited[nb] = 1;
                    queue.push(nb);
                }
            }
        }
        components.push_back(std::move(component));
    }
    return components;
}

} // namespace

void recoverCircularVertexClusters(
    const Mesh& mesh, const FeatureOptions& options, const TraceGraph& trace, FeatureAnalysis& analysis, int& loopId
) {
    CycleSignatureSet seenClusters;
    for (const TraceComponent& component : collectTraceComponents(trace, analysis)) {
        if (component.hasWeakEvidenceEdge || component.alreadyHasCircularLoop ||
            static_cast<int>(component.vertices.size()) < options.minFeatureLoopVertices ||
            component.vertices.size() > 120) {
            continue;
        }

        // Seed circles only from length-2 paths of the trace graph (a - b - c
        // with both edges present). The fallback repairs circles that already
        // carry partial edge evidence, and any evidenced arc of >= 3 vertices
        // contains such a path, so no legitimate circle is lost. This replaces
        // the earlier blind O(n^3) triplet scan, which both fabricated circles
        // through mutually unconnected (merely concyclic) vertices and burned
        // seconds on meshes as small as 58 vertices.
        constexpr int kMaxCircularClusterSeedScans = 32768;
        int seedScans = 0;
        std::vector<int> candidates = component.vertices;
        std::sort(candidates.begin(), candidates.end());
        for (int middle : candidates) {
            // Sorted neighbor enumeration keeps the recovery independent of
            // adjacency insertion order.
            std::vector<int> around = trace.adjacency[middle];
            std::sort(around.begin(), around.end());
            for (int i = 0; i < static_cast<int>(around.size()) && seedScans < kMaxCircularClusterSeedScans; ++i) {
                for (int j = i + 1; j < static_cast<int>(around.size()) && seedScans < kMaxCircularClusterSeedScans;
                     ++j) {
                    ++seedScans;
                    const ThreePointCircle circle = fitCircleFromThree(mesh, around[i], middle, around[j]);
                    if (!circle.valid) {
                        continue;
                    }
                    const double allowed = std::max(3.0 * options.circleFitRelativeThreshold, 0.08) * circle.radius;
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
                    cluster = sortAroundCircle(std::move(cluster), mesh, circle.center, circle.normal);
                    if (!clusterSupportedByTraceEdges(cluster, mesh, trace, circle.center, circle.normal)) {
                        continue;
                    }
                    if (!seenClusters.insert(vertexSetSignature(cluster)).second) {
                        continue;
                    }

                    FeatureLoop loop;
                    loop.id = loopId;
                    loop.vertices = std::move(cluster);
                    loop.edgeCount = static_cast<int>(loop.vertices.size());
                    loop.closed = true;
                    const PrimitiveFit fit = fitPrimitive(mesh, loop, options);
                    applyPrimitiveFit(fit, loop);
                    if (!loop.circular) {
                        continue;
                    }
                    ++loopId;
                    assignLoopToVertices(loop, mesh, trace.adjacency, analysis);
                    analysis.loops.push_back(std::move(loop));
                }
            }
        }
        // Reaching the cap means the exhaustive path scan was cut short (the
        // per-seed loops stop enumerating exactly at the cap).
        if (seedScans >= kMaxCircularClusterSeedScans) {
            ++analysis.circularRecoveryTruncated;
        }
    }
}

} // namespace manumesh::feature::detector_detail
