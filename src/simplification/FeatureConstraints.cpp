/**
 * @file src/simplification/FeatureConstraints.cpp
 * @brief Implements feature constraints facilities for ManuMesh's simplification module.
 * @ingroup manumesh_simplification
 *
 * @details Builds and enforces hard feature-curve and primitive constraints.
 * @algorithm Converts detected loops into segment indices and analytic circle/
 * ellipse fits, classifies endpoint ownership, rejects incompatible merges or
 * vertex-budget violations, and projects accepted placements to the protected
 * curve selected by policy.
 * @failuremodes Ambiguous multi-loop ownership is treated conservatively.
 */

#include "detail/FeatureConstraints.h"

#include "common/detail/GeometryPredicates.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <limits>
#include <numeric>

namespace manumesh::simplification {
namespace {

int curveSegmentCount(const FeatureCurveConstraint& curve) {
    return curve.closed ? static_cast<int>(curve.samples.size())
                        : std::max(0, static_cast<int>(curve.samples.size()) - 1);
}

Vec3 closestPointOnCurveSegment(const FeatureCurveConstraint& curve, int segment, const Vec3& position) {
    const Vec3& p0 = curve.samples[static_cast<std::size_t>(segment)];
    const Vec3& p1 = curve.samples[static_cast<std::size_t>(segment + 1) % curve.samples.size()];
    const Vec3 edge = p1 - p0;
    const double len2 = edge.squaredNorm();
    if (len2 > 1e-30) {
        const double t = std::clamp((position - p0).dot(edge) / len2, 0.0, 1.0);
        return p0 + t * edge;
    }
    return p0;
}

FeatureProtectionMode effectiveFeatureProtectionMode(const SimplifyOptions& options) {
    if (!options.preserveFeatureCurves) {
        return FeatureProtectionMode::None;
    }
    return options.featureProtectionMode;
}

bool isCircularPrimitive(FeatureCurveKind primitive) {
    return primitive == FeatureCurveKind::Circle || primitive == FeatureCurveKind::NearCircle;
}

bool isPrimitiveProtected(const VertexState& vertex, FeatureProtectionMode mode) {
    if (!vertex.isFeature) {
        return false;
    }
    switch (mode) {
    case FeatureProtectionMode::None:
        return false;
    case FeatureProtectionMode::CircularOnly:
        return vertex.circularFeature || isCircularPrimitive(vertex.featurePrimitive);
    case FeatureProtectionMode::PrimitiveCurves:
        return vertex.circularFeature || isCircularPrimitive(vertex.featurePrimitive) ||
               vertex.featurePrimitive == FeatureCurveKind::Ellipse;
    case FeatureProtectionMode::AllFeatureEdges:
        return vertex.isFeature;
    }
    return false;
}

bool isGenericFeature(const VertexState& vertex) {
    return vertex.isFeature && !isPrimitiveProtected(vertex, FeatureProtectionMode::PrimitiveCurves);
}

FeatureCollapseRejectKind featureCollapseRejectKind(const FeatureCollapseInput& input, const SimplifyOptions& options) {
    const FeatureProtectionMode mode = effectiveFeatureProtectionMode(options);
    if (mode == FeatureProtectionMode::None) {
        return FeatureCollapseRejectKind::None;
    }
    const int keep = input.edge.keep;
    const int remove = input.edge.remove;
    const std::vector<VertexState>& vertices = input.vertices;
    const std::vector<int>& activeLoopCounts = input.activeLoopCounts;
    const VertexState& a = vertices[keep];
    const VertexState& b = vertices[remove];
    if (!a.isFeature && !b.isFeature) {
        return FeatureCollapseRejectKind::None;
    }

    const bool aProtected = isPrimitiveProtected(a, mode);
    const bool bProtected = isPrimitiveProtected(b, mode);
    if (mode != FeatureProtectionMode::AllFeatureEdges && !aProtected && !bProtected) {
        return FeatureCollapseRejectKind::None;
    }
    if (mode != FeatureProtectionMode::AllFeatureEdges && aProtected != bProtected) {
        return FeatureCollapseRejectKind::Primitive;
    }

    const FeatureCollapseRejectKind rejectKind =
        mode == FeatureProtectionMode::AllFeatureEdges && (isGenericFeature(a) || isGenericFeature(b))
            ? FeatureCollapseRejectKind::Generic
            : FeatureCollapseRejectKind::Primitive;

    if (a.isFeature != b.isFeature) {
        return rejectKind;
    }
    if (a.featureLoopId < 0 || a.featureLoopId != b.featureLoopId) {
        return rejectKind;
    }
    if (a.featureJunction || b.featureJunction) {
        return rejectKind;
    }
    if (a.featureLoopId >= static_cast<int>(activeLoopCounts.size())) {
        return rejectKind;
    }
    const int minActiveLoopVertices = (a.circularFeature || b.circularFeature) ? options.minCircularFeatureLoopVertices
                                                                               : options.minFeatureLoopVertices;
    if (activeLoopCounts[a.featureLoopId] <= minActiveLoopVertices) {
        const bool hasCurveErrorBudget = options.maxFeatureCurveDeviationRatio > 0.0;
        const bool ellipseFeature =
            a.featurePrimitive == FeatureCurveKind::Ellipse || b.featurePrimitive == FeatureCurveKind::Ellipse;
        const int absoluteMinLoopVertices = (a.circularFeature || b.circularFeature || ellipseFeature) ? 4 : 3;
        if (!hasCurveErrorBudget || activeLoopCounts[a.featureLoopId] <= absoluteMinLoopVertices) {
            return rejectKind;
        }
    }
    return FeatureCollapseRejectKind::None;
}

bool projectFeaturePlacement(const FeatureProjectionInput& input, const SimplifyOptions& options, Vec3& position) {
    const FeatureProtectionMode mode = effectiveFeatureProtectionMode(options);
    if (mode == FeatureProtectionMode::None) {
        return false;
    }
    const int keep = input.edge.keep;
    const int remove = input.edge.remove;
    const std::vector<VertexState>& vertices = input.vertices;
    const std::vector<FeatureCurveConstraint>& curves = input.curves;
    const VertexState& a = vertices[keep];
    const VertexState& b = vertices[remove];
    if (!a.isFeature || !b.isFeature || a.featureLoopId != b.featureLoopId) {
        return false;
    }
    if (!isPrimitiveProtected(a, mode) && !isPrimitiveProtected(b, mode)) {
        return false;
    }
    if (a.circularFeature) {
        position = projectToCircle(position, a, primitiveFitOf(a, input.primitiveFits));
        return true;
    }
    if (b.circularFeature) {
        position = projectToCircle(position, b, primitiveFitOf(b, input.primitiveFits));
        return true;
    }
    if (mode == FeatureProtectionMode::PrimitiveCurves && a.featurePrimitive == FeatureCurveKind::Ellipse) {
        position = projectToEllipse(position, a, primitiveFitOf(a, input.primitiveFits));
        return true;
    }
    if (mode == FeatureProtectionMode::PrimitiveCurves && b.featurePrimitive == FeatureCurveKind::Ellipse) {
        position = projectToEllipse(position, b, primitiveFitOf(b, input.primitiveFits));
        return true;
    }
    if (mode == FeatureProtectionMode::AllFeatureEdges && a.featureLoopId >= 0 &&
        a.featureLoopId < static_cast<int>(curves.size()) && curves[a.featureLoopId].valid &&
        curves[a.featureLoopId].primitive == FeatureCurveKind::PolygonalLoop) {
        double bestDist2 = std::numeric_limits<double>::infinity();
        const Vec3 best = closestPointOnFeatureCurve(curves[a.featureLoopId], position, bestDist2);
        if (std::isfinite(bestDist2)) {
            position = best;
            return true;
        }
    }
    const Vec3 segment = b.p - a.p;
    const double segmentLen2 = segment.squaredNorm();
    if (segmentLen2 > 1e-30) {
        const double t = std::clamp((position - a.p).dot(segment) / segmentLen2, 0.0, 1.0);
        position = a.p + t * segment;
        return true;
    }
    Vec3 tangent = a.curveTangent;
    if (tangent.norm() <= 1e-20) {
        tangent = b.curveTangent;
    }
    if (tangent.norm() > 1e-20) {
        tangent.normalize();
        const Vec3 anchor = 0.5 * (a.p + b.p);
        position = anchor + tangent * (position - anchor).dot(tangent);
        return true;
    }
    return false;
}

} // namespace

Vec3 projectToCircle(const Vec3& p, const VertexState& feature, const FeaturePrimitiveFit& fit) {
    Vec3 normal = fit.circleNormal;
    if (normal.norm() <= 1e-20 || fit.circleRadius <= 1e-20) {
        return p;
    }
    normal.normalize();

    Vec3 radial = p - fit.circleCenter;
    radial -= normal * radial.dot(normal);
    if (radial.norm() <= 1e-20) {
        radial = feature.p - fit.circleCenter;
        radial -= normal * radial.dot(normal);
    }
    if (radial.norm() <= 1e-20) {
        return fit.circleCenter;
    }
    return fit.circleCenter + fit.circleRadius * radial.normalized();
}

Vec3 projectToEllipse(const Vec3& p, const VertexState& feature, const FeaturePrimitiveFit& fit) {
    Vec3 major = fit.ellipseMajorAxis;
    Vec3 minor = fit.ellipseMinorAxis;
    Vec3 normal = fit.ellipseNormal;
    if (major.norm() <= 1e-20 || minor.norm() <= 1e-20 || normal.norm() <= 1e-20 || fit.ellipseMajorRadius <= 1e-20 ||
        fit.ellipseMinorRadius <= 1e-20) {
        return p;
    }
    major.normalize();
    minor.normalize();
    normal.normalize();

    Vec3 delta = p - fit.ellipseCenter;
    delta -= normal * delta.dot(normal);
    if (delta.norm() <= 1e-20) {
        delta = feature.p - fit.ellipseCenter;
        delta -= normal * delta.dot(normal);
    }
    if (delta.norm() <= 1e-20) {
        return fit.ellipseCenter + fit.ellipseMajorRadius * major;
    }

    const double theta =
        std::atan2(delta.dot(minor) / fit.ellipseMinorRadius, delta.dot(major) / fit.ellipseMajorRadius);
    return fit.ellipseCenter + fit.ellipseMajorRadius * std::cos(theta) * major +
           fit.ellipseMinorRadius * std::sin(theta) * minor;
}

void refreshCircularTangent(VertexState& vertex, const FeaturePrimitiveFit& fit) {
    if (!vertex.circularFeature) {
        return;
    }
    Vec3 normal = fit.circleNormal;
    if (normal.norm() <= 1e-20) {
        return;
    }
    normal.normalize();
    Vec3 radial = vertex.p - fit.circleCenter;
    radial -= normal * radial.dot(normal);
    if (radial.norm() <= 1e-20) {
        return;
    }
    vertex.curveTangent = normal.cross(radial).normalized();
}

void refreshEllipseTangent(VertexState& vertex, const FeaturePrimitiveFit& fit) {
    if (vertex.featurePrimitive != FeatureCurveKind::Ellipse) {
        return;
    }
    Vec3 major = fit.ellipseMajorAxis;
    Vec3 minor = fit.ellipseMinorAxis;
    Vec3 normal = fit.ellipseNormal;
    if (major.norm() <= 1e-20 || minor.norm() <= 1e-20 || normal.norm() <= 1e-20 || fit.ellipseMajorRadius <= 1e-20 ||
        fit.ellipseMinorRadius <= 1e-20) {
        return;
    }
    major.normalize();
    minor.normalize();
    normal.normalize();
    Vec3 delta = vertex.p - fit.ellipseCenter;
    delta -= normal * delta.dot(normal);
    if (delta.norm() <= 1e-20) {
        return;
    }
    const double theta =
        std::atan2(delta.dot(minor) / fit.ellipseMinorRadius, delta.dot(major) / fit.ellipseMajorRadius);
    Vec3 tangent = -fit.ellipseMajorRadius * std::sin(theta) * major + fit.ellipseMinorRadius * std::cos(theta) * minor;
    if (tangent.norm() > 1e-20) {
        vertex.curveTangent = tangent.normalized();
    }
}

bool featureCurveBudgetAllows(
    const VertexState& a,
    const VertexState& b,
    const std::vector<FeatureCurveConstraint>& featureCurves,
    const std::vector<FeaturePrimitiveFit>& primitiveFits,
    const SimplifyOptions& options,
    double meshDiagonal,
    const Vec3& position
) {
    if (!options.preserveFeatureCurves || options.maxFeatureCurveDeviationRatio <= 0.0) {
        return true;
    }
    if (!a.isFeature || !b.isFeature || a.featureLoopId < 0 || a.featureLoopId != b.featureLoopId ||
        a.featureLoopId >= static_cast<int>(featureCurves.size())) {
        return true;
    }
    const FeatureCurveConstraint& curve = featureCurves[a.featureLoopId];
    if (!curve.valid) {
        return true;
    }
    const double maxDistance = options.maxFeatureCurveDeviationRatio * std::max(1e-12, meshDiagonal);
    if (a.circularFeature || b.circularFeature) {
        const VertexState& circleVertex = a.circularFeature ? a : b;
        const Vec3 projected = projectToCircle(position, circleVertex, primitiveFitOf(circleVertex, primitiveFits));
        return (position - projected).squaredNorm() <= maxDistance * maxDistance;
    }
    if (a.featurePrimitive == FeatureCurveKind::Ellipse || b.featurePrimitive == FeatureCurveKind::Ellipse) {
        const VertexState& ellipseVertex = a.featurePrimitive == FeatureCurveKind::Ellipse ? a : b;
        const Vec3 projected = projectToEllipse(position, ellipseVertex, primitiveFitOf(ellipseVertex, primitiveFits));
        return (position - projected).squaredNorm() <= maxDistance * maxDistance;
    }
    if (curve.primitive != FeatureCurveKind::PolygonalLoop) {
        return true;
    }

    double bestDist2 = std::numeric_limits<double>::infinity();
    closestPointOnFeatureCurve(curve, position, bestDist2);
    return std::isfinite(bestDist2) && bestDist2 <= maxDistance * maxDistance;
}

void buildPolylineSegmentIndex(FeatureCurveConstraint& curve) {
    curve.segmentIndex = PolylineSegmentIndex{};
    const int segmentCount = curveSegmentCount(curve);
    if (segmentCount < kPolylineIndexMinSegments) {
        return;
    }
    PolylineSegmentIndex& index = curve.segmentIndex;
    index.segmentOrder.resize(static_cast<std::size_t>(segmentCount));
    std::iota(index.segmentOrder.begin(), index.segmentOrder.end(), 0);

    std::vector<Vec3> centroids(static_cast<std::size_t>(segmentCount));
    for (int segment = 0; segment < segmentCount; ++segment) {
        const Vec3& p0 = curve.samples[static_cast<std::size_t>(segment)];
        const Vec3& p1 = curve.samples[static_cast<std::size_t>(segment + 1) % curve.samples.size()];
        centroids[static_cast<std::size_t>(segment)] = 0.5 * (p0 + p1);
    }

    constexpr int kLeafSegments = 8;
    index.nodes.reserve(static_cast<std::size_t>(2 * segmentCount / kLeafSegments + 2));
    // Deterministic median-split build: nth_element with an index tie-break,
    // split axis = largest centroid extent. Recursion depth is O(log L).
    const std::function<int(int, int)> buildNode = [&](int begin, int end) -> int {
        PolylineSegmentIndex::Node node;
        node.begin = begin;
        node.end = end;
        Vec3 lo = Vec3::Constant(std::numeric_limits<double>::infinity());
        Vec3 hi = Vec3::Constant(-std::numeric_limits<double>::infinity());
        for (int i = begin; i < end; ++i) {
            const int segment = index.segmentOrder[static_cast<std::size_t>(i)];
            const Vec3& p0 = curve.samples[static_cast<std::size_t>(segment)];
            const Vec3& p1 = curve.samples[static_cast<std::size_t>(segment + 1) % curve.samples.size()];
            lo = lo.cwiseMin(p0).cwiseMin(p1);
            hi = hi.cwiseMax(p0).cwiseMax(p1);
        }
        node.lo = lo;
        node.hi = hi;
        const int nodeId = static_cast<int>(index.nodes.size());
        index.nodes.push_back(node);
        if (end - begin > kLeafSegments) {
            Vec3 centroidLo = Vec3::Constant(std::numeric_limits<double>::infinity());
            Vec3 centroidHi = Vec3::Constant(-std::numeric_limits<double>::infinity());
            for (int i = begin; i < end; ++i) {
                const Vec3& c = centroids[static_cast<std::size_t>(index.segmentOrder[static_cast<std::size_t>(i)])];
                centroidLo = centroidLo.cwiseMin(c);
                centroidHi = centroidHi.cwiseMax(c);
            }
            const Vec3 extent = centroidHi - centroidLo;
            int axis = 0;
            if (extent.y() > extent[axis]) {
                axis = 1;
            }
            if (extent.z() > extent[axis]) {
                axis = 2;
            }
            const int mid = begin + (end - begin) / 2;
            std::nth_element(
                index.segmentOrder.begin() + begin,
                index.segmentOrder.begin() + mid,
                index.segmentOrder.begin() + end,
                [&](int lhs, int rhs) {
                    const double cl = centroids[static_cast<std::size_t>(lhs)][axis];
                    const double cr = centroids[static_cast<std::size_t>(rhs)][axis];
                    if (cl != cr) {
                        return cl < cr;
                    }
                    return lhs < rhs;
                }
            );
            const int left = buildNode(begin, mid);
            const int right = buildNode(mid, end);
            index.nodes[static_cast<std::size_t>(nodeId)].left = left;
            index.nodes[static_cast<std::size_t>(nodeId)].right = right;
        }
        return nodeId;
    };
    buildNode(0, segmentCount);
}

Vec3 closestPointOnFeatureCurve(const FeatureCurveConstraint& curve, const Vec3& position, double& outDistanceSquared) {
    outDistanceSquared = std::numeric_limits<double>::infinity();
    Vec3 best = position;
    const int segmentCount = curveSegmentCount(curve);
    if (segmentCount <= 0) {
        return best;
    }
    const PolylineSegmentIndex& index = curve.segmentIndex;
    if (!index.built()) {
        for (int segment = 0; segment < segmentCount; ++segment) {
            const Vec3 candidate = closestPointOnCurveSegment(curve, segment, position);
            const double dist2 = (position - candidate).squaredNorm();
            if (dist2 < outDistanceSquared) {
                outDistanceSquared = dist2;
                best = candidate;
            }
        }
        return best;
    }

    // Depth-first descent that visits the nearer child first and prunes with
    // the current best distance; the balanced median-split tree bounds the
    // stack by its depth (comfortably below 64 levels).
    std::array<int, 64> stack{};
    int stackSize = 0;
    stack[static_cast<std::size_t>(stackSize++)] = 0;
    while (stackSize > 0) {
        const PolylineSegmentIndex::Node& node =
            index.nodes[static_cast<std::size_t>(stack[static_cast<std::size_t>(--stackSize)])];
        if (manumesh::common::pointAabbDistanceSquared(position, node.lo, node.hi) >= outDistanceSquared) {
            continue;
        }
        if (node.leaf()) {
            for (int i = node.begin; i < node.end; ++i) {
                const int segment = index.segmentOrder[static_cast<std::size_t>(i)];
                const Vec3 candidate = closestPointOnCurveSegment(curve, segment, position);
                const double dist2 = (position - candidate).squaredNorm();
                if (dist2 < outDistanceSquared) {
                    outDistanceSquared = dist2;
                    best = candidate;
                }
            }
            continue;
        }
        const PolylineSegmentIndex::Node& leftNode = index.nodes[static_cast<std::size_t>(node.left)];
        const PolylineSegmentIndex::Node& rightNode = index.nodes[static_cast<std::size_t>(node.right)];
        const double leftDist = manumesh::common::pointAabbDistanceSquared(position, leftNode.lo, leftNode.hi);
        const double rightDist = manumesh::common::pointAabbDistanceSquared(position, rightNode.lo, rightNode.hi);
        if (stackSize + 2 <= static_cast<int>(stack.size())) {
            if (leftDist <= rightDist) {
                stack[static_cast<std::size_t>(stackSize++)] = node.right;
                stack[static_cast<std::size_t>(stackSize++)] = node.left;
            } else {
                stack[static_cast<std::size_t>(stackSize++)] = node.left;
                stack[static_cast<std::size_t>(stackSize++)] = node.right;
            }
        }
    }
    return best;
}

FeatureConstraintPolicy::FeatureConstraintPolicy(const SimplifyOptions& options)
    : options_(options) {}

FeatureCollapseRejectKind FeatureConstraintPolicy::collapseRejectKind(const FeatureCollapseInput& input) const {
    return featureCollapseRejectKind(input, options_);
}

bool FeatureConstraintPolicy::isHardProtectedVertex(int vertex, const std::vector<VertexState>& vertices) const {
    return vertex >= 0 && vertex < static_cast<int>(vertices.size()) &&
           isPrimitiveProtected(vertices[vertex], effectiveFeatureProtectionMode(options_));
}

bool FeatureConstraintPolicy::isHardProtectedCollapse(
    CollapseEdge edge, const std::vector<VertexState>& vertices
) const {
    const FeatureProtectionMode mode = effectiveFeatureProtectionMode(options_);
    if (mode == FeatureProtectionMode::None) {
        return false;
    }
    return isPrimitiveProtected(vertices[edge.keep], mode) && isPrimitiveProtected(vertices[edge.remove], mode) &&
           vertices[edge.keep].featureLoopId == vertices[edge.remove].featureLoopId;
}

bool FeatureConstraintPolicy::projectPlacement(const FeatureProjectionInput& input, Vec3& position) const {
    return projectFeaturePlacement(input, options_, position);
}

} // namespace manumesh::simplification
