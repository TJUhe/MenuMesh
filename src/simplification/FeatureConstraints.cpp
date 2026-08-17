/**
 * @file src/simplification/FeatureConstraints.cpp
 * @brief 执行硬特征曲线判定和解析曲线投影。
 * @ingroup manumesh_simplification
 *
 * @details 构建并执行硬性特征曲线约束和解析几何基元约束。
 * @algorithm 将检测到的环转换为线段索引，并拟合解析圆/椭圆；判断端点归属，拒绝不兼容的合并或违反顶点预算的操作，并将接受的放置点投影到策略选定的受保护曲线。
 * @failuremodes 对多环归属不明确的情况采取保守处理。
 */

#include "detail/FeatureConstraints.h"
#include "core/MathUtils.h"

#include "common/detail/GeometryPredicates.h"
#include "detail/SimplificationPolicies.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <limits>
#include <numeric>

namespace manumesh {
namespace simplification {
namespace {

int curveSegmentCount(const FeatureCurveConstraint& curve) {
    if (!curve.segments.empty()) {
        return static_cast<int>(curve.segments.size());
    }
    return curve.closed ? static_cast<int>(curve.samples.size())
                        : std::max(0, static_cast<int>(curve.samples.size()) - 1);
}

std::array<Vec3, 2> curveSegmentEndpoints(const FeatureCurveConstraint& curve, int segment) {
    if (!curve.segments.empty()) {
        return curve.segments[static_cast<std::size_t>(segment)];
    }
    return {{
        curve.samples[static_cast<std::size_t>(segment)],
        curve.samples[static_cast<std::size_t>(segment + 1) % curve.samples.size()],
    }};
}

Vec3 closestPointOnCurveSegment(const FeatureCurveConstraint& curve, int segment, const Vec3& position) {
    const std::array<Vec3, 2> endpoints = curveSegmentEndpoints(curve, segment);
    const Vec3& p0 = endpoints[0];
    const Vec3& p1 = endpoints[1];
    const Vec3 edge = p1 - p0;
    const double len2 = edge.squaredNorm();
    if (len2 > 1e-30) {
        const double t = manumesh::clampValue((position - p0).dot(edge) / len2, 0.0, 1.0);
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

bool currentConstraintRoleBlocksCollapse(int vertex, const FeatureConstraintGraph& constraints) {
    if (vertex < 0 || vertex >= static_cast<int>(constraints.vertices.size())) {
        return false;
    }
    const FeatureConstraintVertex& state = constraints.vertices[static_cast<std::size_t>(vertex)];
    return state.junction || state.shared || state.ambiguousJunction;
}

FeatureCollapseRejectKind featureCollapseRejectKind(
    const FeatureCollapseInput& input, const SimplifyOptions& options, int minFeatureLoopVertices
) {
    const FeatureProtectionMode mode = effectiveFeatureProtectionMode(options);
    if (mode == FeatureProtectionMode::None) {
        return FeatureCollapseRejectKind::None;
    }
    const int keep = input.edge.keep;
    const int remove = input.edge.remove;
    const std::vector<VertexState>& vertices = input.vertices;
    const std::vector<int>& activeLoopCounts = input.activeLoopCounts;
    if (keep < 0 || remove < 0 || keep == remove || keep >= static_cast<int>(vertices.size()) ||
        remove >= static_cast<int>(vertices.size())) {
        return FeatureCollapseRejectKind::Generic;
    }
    const VertexState& a = vertices[keep];
    const VertexState& b = vertices[remove];
    const bool currentRoleBlocksKeep = currentConstraintRoleBlocksCollapse(keep, input.constraints);
    const bool currentRoleBlocksRemove = currentConstraintRoleBlocksCollapse(remove, input.constraints);

    if (mode == FeatureProtectionMode::AllFeatureEdges) {
        const bool aGraphFeature = input.constraints.hasProtectedIncidentEdge(keep);
        const bool bGraphFeature = input.constraints.hasProtectedIncidentEdge(remove);
        if (!aGraphFeature && !bGraphFeature && !a.isFeature && !b.isFeature) {
            return FeatureCollapseRejectKind::None;
        }

        const FeatureCollapseRejectKind rejectKind = isGenericFeature(a) || isGenericFeature(b)
                                                         ? FeatureCollapseRejectKind::Generic
                                                         : FeatureCollapseRejectKind::Primitive;
        if (!aGraphFeature || !bGraphFeature || !input.constraints.isProtectedPathEdge(keep, remove)) {
            return rejectKind;
        }
        if (a.featureJunction || b.featureJunction || currentRoleBlocksKeep || currentRoleBlocksRemove) {
            return rejectKind;
        }
        if (input.constraints.isOnlyProtectedEdgeInComponent(keep, remove)) {
            return rejectKind;
        }

        const FeatureConstraintEdge* edge = input.constraints.findEdge(keep, remove);
        if (edge == nullptr || edge->loopIds.empty()) {
            // Untraced evidence remains protected, but cannot be shortened without a curve budget.
            return rejectKind;
        }
        const int minActiveLoopVertices =
            (a.circularFeature || b.circularFeature) ? options.minCircularFeatureLoopVertices : minFeatureLoopVertices;
        const bool hasCurveErrorBudget = options.maxFeatureCurveDeviationRatio > 0.0;
        const bool ellipseFeature =
            a.featurePrimitive == FeatureCurveKind::Ellipse || b.featurePrimitive == FeatureCurveKind::Ellipse;
        const int absoluteMinLoopVertices = (a.circularFeature || b.circularFeature || ellipseFeature) ? 4 : 3;
        const auto belowBudget = [&](int activeVertices) {
            return activeVertices <= 0 || (activeVertices <= minActiveLoopVertices &&
                                           (!hasCurveErrorBudget || activeVertices <= absoluteMinLoopVertices));
        };
        // Synthetic recovery edges can split one detector loop into several
        // real protected path components. Preserve the old component-local
        // floor as well as the per-loop floor below.
        if (belowBudget(input.constraints.protectedComponentVertexCount(keep, remove))) {
            return rejectKind;
        }
        // A protected component can contain several loops joined at shared
        // vertices. A large sibling loop must not hide that this contraction
        // would take a smaller loop below its own budget.
        for (int loopId : edge->loopIds) {
            if (loopId < 0 || loopId >= static_cast<int>(activeLoopCounts.size())) {
                return rejectKind;
            }
            const int activeLoopVertices = activeLoopCounts[static_cast<std::size_t>(loopId)];
            if (belowBudget(activeLoopVertices)) {
                return rejectKind;
            }
        }
        return FeatureCollapseRejectKind::None;
    }

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

    const FeatureCollapseRejectKind rejectKind = FeatureCollapseRejectKind::Primitive;

    if (a.isFeature != b.isFeature) {
        return rejectKind;
    }
    if (a.featureLoopId < 0 || a.featureLoopId != b.featureLoopId) {
        return rejectKind;
    }
    if (a.featureJunction || b.featureJunction || currentRoleBlocksKeep || currentRoleBlocksRemove) {
        return rejectKind;
    }
    if (a.featureLoopId >= static_cast<int>(activeLoopCounts.size())) {
        return rejectKind;
    }
    const int minActiveLoopVertices =
        (a.circularFeature || b.circularFeature) ? options.minCircularFeatureLoopVertices : minFeatureLoopVertices;
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
    if (keep < 0 || remove < 0 || keep == remove || keep >= static_cast<int>(vertices.size()) ||
        remove >= static_cast<int>(vertices.size())) {
        return false;
    }
    const VertexState& a = vertices[keep];
    const VertexState& b = vertices[remove];
    if (mode == FeatureProtectionMode::AllFeatureEdges) {
        if (!input.constraints.isProtectedPathEdge(keep, remove)) {
            return false;
        }
    } else {
        if (!a.isFeature || !b.isFeature || a.featureLoopId != b.featureLoopId) {
            return false;
        }
        if (!isPrimitiveProtected(a, mode) && !isPrimitiveProtected(b, mode)) {
            return false;
        }
    }
    if (a.circularFeature || isCircularPrimitive(a.featurePrimitive)) {
        position = projectToCircle(position, a, primitiveFitOf(a, input.primitiveFits));
        return true;
    }
    if (b.circularFeature || isCircularPrimitive(b.featurePrimitive)) {
        position = projectToCircle(position, b, primitiveFitOf(b, input.primitiveFits));
        return true;
    }
    if (a.featurePrimitive == FeatureCurveKind::Ellipse) {
        position = projectToEllipse(position, a, primitiveFitOf(a, input.primitiveFits));
        return true;
    }
    if (b.featurePrimitive == FeatureCurveKind::Ellipse) {
        position = projectToEllipse(position, b, primitiveFitOf(b, input.primitiveFits));
        return true;
    }
    if (mode == FeatureProtectionMode::AllFeatureEdges) {
        double bestDist2 = std::numeric_limits<double>::infinity();
        Vec3 best = position;
        const FeatureConstraintEdge* edge = input.constraints.findEdge(keep, remove);
        if (edge != nullptr) {
            for (int loopId : edge->loopIds) {
                if (loopId < 0 || loopId >= static_cast<int>(curves.size()) || !curves[loopId].valid ||
                    curves[loopId].primitive != FeatureCurveKind::PolygonalLoop) {
                    continue;
                }
                double distanceSquared = std::numeric_limits<double>::infinity();
                const Vec3 candidate = closestPointOnFeatureCurve(curves[loopId], position, distanceSquared);
                if (distanceSquared < bestDist2) {
                    bestDist2 = distanceSquared;
                    best = candidate;
                }
            }
        }
        if (std::isfinite(bestDist2)) {
            position = best;
            return true;
        }
    }
    const Vec3 segment = b.p - a.p;
    const double segmentLen2 = segment.squaredNorm();
    if (segmentLen2 > 1e-30) {
        const double t = manumesh::clampValue((position - a.p).dot(segment) / segmentLen2, 0.0, 1.0);
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
    normal.normalize();
    major -= normal * major.dot(normal);
    if (major.norm() <= 1e-20) {
        return p;
    }
    major.normalize();
    const Vec3 originalMinor = minor;
    minor -= normal * minor.dot(normal);
    minor -= major * minor.dot(major);
    if (minor.norm() <= 1e-20) {
        minor = normal.cross(major);
    }
    if (minor.dot(originalMinor) < 0.0) {
        minor = -minor;
    }
    minor.normalize();

    Vec3 delta = p - fit.ellipseCenter;
    delta -= normal * delta.dot(normal);
    if (delta.norm() <= 1e-20 && fit.ellipseMajorRadius == fit.ellipseMinorRadius) {
        delta = feature.p - fit.ellipseCenter;
        delta -= normal * delta.dot(normal);
    }
    if (delta.norm() <= 1e-20) {
        return fit.ellipseMajorRadius <= fit.ellipseMinorRadius ? fit.ellipseCenter + fit.ellipseMajorRadius * major
                                                                : fit.ellipseCenter + fit.ellipseMinorRadius * minor;
    }

    const double x = delta.dot(major);
    const double y = delta.dot(minor);
    const double absX = std::abs(x);
    const double absY = std::abs(y);
    const double majorRadius = fit.ellipseMajorRadius;
    const double minorRadius = fit.ellipseMinorRadius;
    const auto distanceSquaredAt = [&](double theta) {
        const long double dx = static_cast<long double>(majorRadius) * std::cos(theta) - absX;
        const long double dy = static_cast<long double>(minorRadius) * std::sin(theta) - absY;
        return dx * dx + dy * dy;
    };

    // The radial parameter angle is not the Euclidean closest point for a
    // non-circular ellipse. Locate the global minimum in the first quadrant,
    // then refine its local bracket with a deterministic golden-section search.
    constexpr int kSamples = 32;
    const double halfPi = 0.5 * std::acos(-1.0);
    const double step = halfPi / static_cast<double>(kSamples);
    int bestIndex = 0;
    long double bestDistance = distanceSquaredAt(0.0);
    for (int sample = 1; sample <= kSamples; ++sample) {
        const long double distance = distanceSquaredAt(step * static_cast<double>(sample));
        if (distance < bestDistance) {
            bestDistance = distance;
            bestIndex = sample;
        }
    }
    double left = step * static_cast<double>(std::max(0, bestIndex - 1));
    double right = step * static_cast<double>(std::min(kSamples, bestIndex + 1));
    constexpr double kGolden = 0.6180339887498948482;
    double c = right - kGolden * (right - left);
    double d = left + kGolden * (right - left);
    long double fc = distanceSquaredAt(c);
    long double fd = distanceSquaredAt(d);
    for (int iteration = 0; iteration < 56; ++iteration) {
        if (fc <= fd) {
            right = d;
            d = c;
            fd = fc;
            c = right - kGolden * (right - left);
            fc = distanceSquaredAt(c);
        } else {
            left = c;
            c = d;
            fc = fd;
            d = left + kGolden * (right - left);
            fd = distanceSquaredAt(d);
        }
    }
    double theta = 0.5 * (left + right);
    // Function values become indistinguishable when the angular error reaches
    // roughly sqrt(machine epsilon). Analytic Newton refinement restores the
    // first-order closest-point condition without changing the global basin
    // selected by the bounded search above.
    for (int iteration = 0; iteration < 8; ++iteration) {
        const long double angle = theta;
        const long double cosine = std::cos(angle);
        const long double sine = std::sin(angle);
        const long double dx = static_cast<long double>(majorRadius) * cosine - absX;
        const long double dy = static_cast<long double>(minorRadius) * sine - absY;
        const long double dxFirst = -static_cast<long double>(majorRadius) * sine;
        const long double dyFirst = static_cast<long double>(minorRadius) * cosine;
        const long double first = 2.0L * (dx * dxFirst + dy * dyFirst);
        const long double second = 2.0L * (dxFirst * dxFirst - dx * static_cast<long double>(majorRadius) * cosine +
                                           dyFirst * dyFirst - dy * static_cast<long double>(minorRadius) * sine);
        if (!std::isfinite(first) || !std::isfinite(second) || second <= 1e-30L) {
            break;
        }
        const double refined =
            manumesh::clampValue(static_cast<double>(static_cast<long double>(theta) - first / second), 0.0, halfPi);
        if (refined == theta) {
            break;
        }
        theta = refined;
    }
    const double xSign = x < 0.0 ? -1.0 : 1.0;
    const double ySign = y < 0.0 ? -1.0 : 1.0;
    return fit.ellipseCenter + fit.ellipseMajorRadius * std::cos(theta) * xSign * major +
           fit.ellipseMinorRadius * std::sin(theta) * ySign * minor;
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
    const Vec3& position,
    const FeatureConstraintGraph* constraints,
    CollapseEdge edge
) {
    if (!options.preserveFeatureCurves || options.maxFeatureCurveDeviationRatio <= 0.0) {
        return true;
    }
    if (!a.isFeature || !b.isFeature) {
        return true;
    }

    std::vector<int> loopIds;
    bool invalidLoopId = false;
    const auto appendLoopId = [&](int loopId) {
        if (loopId < 0) {
            invalidLoopId = true;
        } else if (std::find(loopIds.begin(), loopIds.end(), loopId) == loopIds.end()) {
            loopIds.push_back(loopId);
        }
    };
    bool graphOwnershipAvailable = false;
    if (constraints != nullptr && edge.keep >= 0 && edge.remove >= 0) {
        if (edge.keep == edge.remove && edge.keep < static_cast<int>(constraints->vertices.size())) {
            // Quality refinement moves one existing vertex, so all loops
            // currently attached to that graph vertex must share its budget.
            for (int loopId : constraints->vertices[static_cast<std::size_t>(edge.keep)].loopIds) {
                appendLoopId(loopId);
            }
            graphOwnershipAvailable = !loopIds.empty();
        } else if (edge.keep != edge.remove) {
            const FeatureConstraintEdge* constraintEdge = constraints->findEdge(edge.keep, edge.remove);
            if (constraintEdge != nullptr) {
                for (int loopId : constraintEdge->loopIds) {
                    appendLoopId(loopId);
                }
                graphOwnershipAvailable = !loopIds.empty();
            }
        }
    }
    if (invalidLoopId) {
        return false;
    }
    // When current graph ownership is unavailable, retain the original
    // single-loop behavior for compatibility with untraced or legacy data.
    if (!graphOwnershipAvailable) {
        if (a.featureLoopId < 0 || a.featureLoopId != b.featureLoopId) {
            return true;
        }
        appendLoopId(a.featureLoopId);
    }

    std::sort(loopIds.begin(), loopIds.end());

    const double maxDistance = options.maxFeatureCurveDeviationRatio * std::max(1e-12, meshDiagonal);
    const double maxDistanceSquared = maxDistance * maxDistance;
    for (int loopId : loopIds) {
        if (loopId < 0 || loopId >= static_cast<int>(featureCurves.size())) {
            return false;
        }
        const FeatureCurveConstraint& curve = featureCurves[static_cast<std::size_t>(loopId)];
        if (!curve.valid) {
            continue;
        }
        if (curve.primitive == FeatureCurveKind::Circle || curve.primitive == FeatureCurveKind::NearCircle) {
            const VertexState& circleVertex = a.circularFeature || isCircularPrimitive(a.featurePrimitive) ? a : b;
            const FeaturePrimitiveFit& fit =
                curve.primitiveFitId >= 0 && curve.primitiveFitId < static_cast<int>(primitiveFits.size())
                    ? primitiveFits[static_cast<std::size_t>(curve.primitiveFitId)]
                    : primitiveFitOf(circleVertex, primitiveFits);
            const Vec3 projected = projectToCircle(position, circleVertex, fit);
            if ((position - projected).squaredNorm() > maxDistanceSquared) {
                return false;
            }
            continue;
        }
        if (curve.primitive == FeatureCurveKind::Ellipse) {
            const VertexState& ellipseVertex = a.featurePrimitive == FeatureCurveKind::Ellipse ? a : b;
            const FeaturePrimitiveFit& fit =
                curve.primitiveFitId >= 0 && curve.primitiveFitId < static_cast<int>(primitiveFits.size())
                    ? primitiveFits[static_cast<std::size_t>(curve.primitiveFitId)]
                    : primitiveFitOf(ellipseVertex, primitiveFits);
            const Vec3 projected = projectToEllipse(position, ellipseVertex, fit);
            if ((position - projected).squaredNorm() > maxDistanceSquared) {
                return false;
            }
            continue;
        }
        if (curve.primitive != FeatureCurveKind::PolygonalLoop) {
            continue;
        }
        double bestDist2 = std::numeric_limits<double>::infinity();
        closestPointOnFeatureCurve(curve, position, bestDist2);
        if (!std::isfinite(bestDist2) || bestDist2 > maxDistanceSquared) {
            return false;
        }
    }
    return true;
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
        const std::array<Vec3, 2> endpoints = curveSegmentEndpoints(curve, segment);
        const Vec3& p0 = endpoints[0];
        const Vec3& p1 = endpoints[1];
        centroids[static_cast<std::size_t>(segment)] = 0.5 * (p0 + p1);
    }

    constexpr int kLeafSegments = 8;
    index.nodes.reserve(static_cast<std::size_t>(2 * segmentCount / kLeafSegments + 2));
    // 确定性的中位数划分构建：nth_element 使用索引作为平局打破规则，划分轴取质心范围最大的坐标轴。递归深度为 O(log L)。
    const std::function<int(int, int)> buildNode = [&](int begin, int end) -> int {
        PolylineSegmentIndex::Node node;
        node.begin = begin;
        node.end = end;
        Vec3 lo = Vec3::Constant(std::numeric_limits<double>::infinity());
        Vec3 hi = Vec3::Constant(-std::numeric_limits<double>::infinity());
        for (int i = begin; i < end; ++i) {
            const int segment = index.segmentOrder[static_cast<std::size_t>(i)];
            const std::array<Vec3, 2> endpoints = curveSegmentEndpoints(curve, segment);
            const Vec3& p0 = endpoints[0];
            const Vec3& p1 = endpoints[1];
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

    // 深度优先下降时优先访问较近的子节点，并用当前最佳距离进行剪枝；平衡的中位数划分树将栈深限制在树深以内（远低于 64 层）。
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
    : options_(options),
      minFeatureLoopVertices_(featureOptionsFromSimplifyOptions(options, 5).minFeatureLoopVertices) {}

FeatureCollapseRejectKind FeatureConstraintPolicy::collapseRejectKind(const FeatureCollapseInput& input) const {
    return featureCollapseRejectKind(input, options_, minFeatureLoopVertices_);
}

bool FeatureConstraintPolicy::isHardProtectedVertex(
    int vertex, const std::vector<VertexState>& vertices, const FeatureConstraintGraph& constraints
) const {
    if (vertex < 0 || vertex >= static_cast<int>(vertices.size())) {
        return false;
    }
    const FeatureProtectionMode mode = effectiveFeatureProtectionMode(options_);
    return mode == FeatureProtectionMode::AllFeatureEdges ? constraints.hasProtectedIncidentEdge(vertex)
                                                          : isPrimitiveProtected(vertices[vertex], mode);
}

bool FeatureConstraintPolicy::isHardProtectedCollapse(
    CollapseEdge edge, const std::vector<VertexState>& vertices, const FeatureConstraintGraph& constraints
) const {
    if (edge.keep < 0 || edge.remove < 0 || edge.keep == edge.remove ||
        edge.keep >= static_cast<int>(vertices.size()) || edge.remove >= static_cast<int>(vertices.size())) {
        return false;
    }
    const FeatureProtectionMode mode = effectiveFeatureProtectionMode(options_);
    if (mode == FeatureProtectionMode::None) {
        return false;
    }
    if (mode == FeatureProtectionMode::AllFeatureEdges) {
        return constraints.isProtectedPathEdge(edge.keep, edge.remove);
    }
    return isPrimitiveProtected(vertices[edge.keep], mode) && isPrimitiveProtected(vertices[edge.remove], mode) &&
           vertices[edge.keep].featureLoopId == vertices[edge.remove].featureLoopId;
}

bool FeatureConstraintPolicy::projectPlacement(const FeatureProjectionInput& input, Vec3& position) const {
    return projectFeaturePlacement(input, options_, position);
}

} // namespace simplification
} // namespace manumesh
