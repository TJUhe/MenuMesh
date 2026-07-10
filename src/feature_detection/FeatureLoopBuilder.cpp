#include "detail/FeatureLoopBuilder.h"

#include "common/detail/MeshQueries.h"
#include "detail/FeatureGraph.h"
#include "detail/PrimitiveFit.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace manumesh::feature::detector_detail {
namespace {

using primitive_fit_detail::applyPrimitiveFit;
using primitive_fit_detail::cycleEdgesFollowCircle;
using primitive_fit_detail::fitPrimitive;
using primitive_fit_detail::PrimitiveFit;

Vec3 fallbackTangentFromNeighbors(int id, const std::vector<int>& neighbors, const Mesh& mesh) {
    Vec3 t = Vec3::Zero();
    for (int nb : neighbors) {
        Vec3 d = mesh.vertices[nb] - mesh.vertices[id];
        if (d.norm() > 1e-20) {
            t += d.normalized();
        }
    }
    if (t.norm() <= 1e-20 && neighbors.size() >= 2) {
        t = mesh.vertices[neighbors[1]] - mesh.vertices[neighbors[0]];
    }
    if (t.norm() <= 1e-20) {
        return Vec3(1.0, 0.0, 0.0);
    }
    return t.normalized();
}

bool cycleHasUniqueVertices(const std::vector<int>& vertices) {
    std::unordered_set<int> seen;
    seen.reserve(vertices.size());
    for (int id : vertices) {
        if (!seen.insert(id).second) {
            return false;
        }
    }
    return true;
}

} // namespace

std::string cycleSignature(const std::vector<int>& vertices) {
    std::vector<std::uint64_t> keys;
    keys.reserve(vertices.size());
    for (int i = 0; i < static_cast<int>(vertices.size()); ++i) {
        keys.push_back(manumesh::detail::meshEdgeKey(vertices[i], vertices[(i + 1) % vertices.size()]));
    }
    std::sort(keys.begin(), keys.end());
    std::ostringstream out;
    for (std::uint64_t key : keys) {
        out << key << ';';
    }
    return out.str();
}

void assignLoopToVertices(
    const FeatureLoop& loop, const Mesh& mesh, const std::vector<std::vector<int>>& adjacency, FeatureAnalysis& analysis
) {
    for (int id : loop.vertices) {
        VertexFeature& vf = analysis.vertices[id];
        if (id >= 0 && id < static_cast<int>(analysis.graph.vertices.size())) {
            FeatureGraphVertex& gv = analysis.graph.vertices[id];
            if (std::find(gv.loopIds.begin(), gv.loopIds.end(), loop.id) == gv.loopIds.end()) {
                gv.loopIds.push_back(loop.id);
            }
        }
        const bool alreadyFeature = vf.isFeature;
        if (!alreadyFeature) {
            vf.isFeature = true;
            vf.loopId = loop.id;
            vf.circular = loop.circular;
            vf.primitive = loop.primitive;
        }
        vf.junction = vf.junction || alreadyFeature || adjacency[id].size() != 2;
        if (loop.circular && (!alreadyFeature || !vf.circular)) {
            vf.loopId = loop.id;
            vf.circular = true;
            vf.primitive = loop.primitive;
            vf.circleCenter = loop.center;
            vf.circleNormal = loop.normal;
            vf.circleRadius = loop.radius;
            Vec3 radial = mesh.vertices[id] - loop.center;
            radial -= loop.normal * radial.dot(loop.normal);
            if (radial.norm() > 1e-20) {
                vf.tangent = loop.normal.cross(radial).normalized();
            } else {
                vf.tangent = fallbackTangentFromNeighbors(id, adjacency[id], mesh);
            }
        } else if (
            loop.primitive == FeaturePrimitiveType::Ellipse &&
            (!alreadyFeature || vf.primitive == FeaturePrimitiveType::Unknown || vf.tangent.norm() <= 1e-20)
        ) {
            vf.loopId = loop.id;
            vf.primitive = loop.primitive;
            vf.ellipseCenter = loop.center;
            vf.ellipseNormal = loop.normal;
            vf.ellipseMajorAxis = loop.majorAxis;
            vf.ellipseMinorAxis = loop.minorAxis;
            vf.ellipseMajorRadius = loop.majorRadius;
            vf.ellipseMinorRadius = loop.minorRadius;
            Vec3 major = loop.majorAxis;
            Vec3 minor = loop.minorAxis;
            if (major.norm() > 1e-20 && minor.norm() > 1e-20 && loop.majorRadius > 1e-20 && loop.minorRadius > 1e-20) {
                major.normalize();
                minor.normalize();
                const Vec3 delta = mesh.vertices[id] - loop.center;
                const double theta =
                    std::atan2(delta.dot(minor) / loop.minorRadius, delta.dot(major) / loop.majorRadius);
                Vec3 tangent = -loop.majorRadius * std::sin(theta) * major + loop.minorRadius * std::cos(theta) * minor;
                vf.tangent = tangent.norm() > 1e-20 ? tangent.normalized()
                                                    : fallbackTangentFromNeighbors(id, adjacency[id], mesh);
            } else {
                vf.tangent = fallbackTangentFromNeighbors(id, adjacency[id], mesh);
            }
        } else if (!alreadyFeature || vf.tangent.norm() <= 1e-20) {
            vf.primitive = loop.primitive;
            vf.tangent = fallbackTangentFromNeighbors(id, adjacency[id], mesh);
        }
    }
}

FeatureLoop makeLoopFromStats(std::vector<int> vertices, int loopId, const TraceLoopStats& stats) {
    FeatureLoop loop;
    loop.id = loopId;
    loop.vertices = std::move(vertices);
    loop.edgeCount = stats.edgeCount;
    loop.closed = stats.closed;
    loop.mostlyBoundary =
        loop.edgeCount > 0 && stats.boundaryEdges >= static_cast<int>(0.6 * static_cast<double>(loop.edgeCount));
    loop.convexEdges = stats.convexEdges;
    loop.concaveEdges = stats.concaveEdges;
    loop.unknownSignedEdges = stats.unknownSignedEdges;
    return loop;
}

void addTracedLoop(
    const Mesh& mesh,
    const FeatureOptions& options,
    const std::vector<std::vector<int>>& adjacency,
    std::vector<int> vertices,
    const TraceLoopStats& stats,
    FeatureAnalysis& analysis,
    int& loopId
) {
    if (vertices.empty() || stats.edgeCount <= 0) {
        return;
    }

    FeatureLoop loop = makeLoopFromStats(std::move(vertices), loopId++, stats);
    if (loop.closed && static_cast<int>(loop.vertices.size()) >= options.minFeatureLoopVertices) {
        applyPrimitiveFit(fitPrimitive(mesh, loop, options), loop);
    }

    assignLoopToVertices(loop, mesh, adjacency, analysis);
    analysis.loops.push_back(std::move(loop));
}

bool addRecoveredCycle(
    RecoveredCycleKind kind,
    std::vector<int> vertices,
    std::unordered_set<std::string>& seenCycles,
    const Mesh& mesh,
    const FeatureOptions& options,
    const TraceGraph& trace,
    FeatureAnalysis& analysis,
    int& loopId
) {
    if (static_cast<int>(vertices.size()) < options.minFeatureLoopVertices || !cycleHasUniqueVertices(vertices)) {
        return false;
    }
    const std::string signature = cycleSignature(vertices);
    if (!seenCycles.insert(signature).second) {
        return false;
    }

    TraceLoopStats stats;
    stats.edgeCount = static_cast<int>(vertices.size());
    stats.closed = true;
    for (int i = 0; i < static_cast<int>(vertices.size()); ++i) {
        const int a = vertices[i];
        const int b = vertices[(i + 1) % vertices.size()];
        if (traceEdgeBoundary(trace, a, b)) {
            ++stats.boundaryEdges;
        }
        if (traceEdgeDihedral(trace, a, b)) {
            ++stats.dihedralEdges;
        }
        if (traceEdgeNormalTensor(trace, a, b)) {
            ++stats.normalTensorEdges;
        }
        if (traceEdgeNonManifold(trace, a, b)) {
            ++stats.nonManifoldEdges;
        }
        if (traceEdgeCleanupBridge(trace, a, b)) {
            ++stats.cleanupBridgeEdges;
        }
        const int sign = traceEdgeSign(trace, a, b);
        if (sign > 0)
            ++stats.convexEdges;
        if (sign < 0)
            ++stats.concaveEdges;
        if (sign == 0 && !traceEdgeBoundary(trace, a, b)) {
            ++stats.unknownSignedEdges;
        }
    }

    FeatureLoop loop = makeLoopFromStats(std::move(vertices), loopId, stats);
    const PrimitiveFit fit = fitPrimitive(mesh, loop, options);
    applyPrimitiveFit(fit, loop);
    if (kind == RecoveredCycleKind::Circular &&
        (!loop.circular || !cycleEdgesFollowCircle(loop.vertices, fit, mesh, options))) {
        return false;
    }
    if (kind == RecoveredCycleKind::Polygonal &&
        (!fit.valid || loop.primitive != FeaturePrimitiveType::PolygonalLoop)) {
        return false;
    }

    ++loopId;
    assignLoopToVertices(loop, mesh, trace.adjacency, analysis);
    analysis.loops.push_back(std::move(loop));
    return true;
}

} // namespace manumesh::feature::detector_detail
