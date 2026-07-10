#include "detail/FeatureConstraints.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace manumesh::simplification {
namespace {

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
        position = projectToCircle(position, a);
        return true;
    }
    if (b.circularFeature) {
        position = projectToCircle(position, b);
        return true;
    }
    if (mode == FeatureProtectionMode::PrimitiveCurves && a.featurePrimitive == FeatureCurveKind::Ellipse) {
        position = projectToEllipse(position, a);
        return true;
    }
    if (mode == FeatureProtectionMode::PrimitiveCurves && b.featurePrimitive == FeatureCurveKind::Ellipse) {
        position = projectToEllipse(position, b);
        return true;
    }
    if (mode == FeatureProtectionMode::AllFeatureEdges && a.featureLoopId >= 0 &&
        a.featureLoopId < static_cast<int>(curves.size()) && curves[a.featureLoopId].valid &&
        curves[a.featureLoopId].primitive == FeatureCurveKind::PolygonalLoop) {
        const FeatureCurveConstraint& curve = curves[a.featureLoopId];
        double bestDist2 = std::numeric_limits<double>::infinity();
        Vec3 best = position;
        const int segmentCount = curve.closed ? static_cast<int>(curve.samples.size())
                                              : std::max(0, static_cast<int>(curve.samples.size()) - 1);
        for (int i = 0; i < segmentCount; ++i) {
            const Vec3& p0 = curve.samples[i];
            const Vec3& p1 = curve.samples[(i + 1) % curve.samples.size()];
            const Vec3 edge = p1 - p0;
            const double len2 = edge.squaredNorm();
            Vec3 candidate = p0;
            if (len2 > 1e-30) {
                const double t = std::clamp((position - p0).dot(edge) / len2, 0.0, 1.0);
                candidate = p0 + t * edge;
            }
            const double dist2 = (position - candidate).squaredNorm();
            if (dist2 < bestDist2) {
                bestDist2 = dist2;
                best = candidate;
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

Vec3 projectToCircle(const Vec3& p, const VertexState& feature) {
    Vec3 normal = feature.circleNormal;
    if (normal.norm() <= 1e-20 || feature.circleRadius <= 1e-20) {
        return p;
    }
    normal.normalize();

    Vec3 radial = p - feature.circleCenter;
    radial -= normal * radial.dot(normal);
    if (radial.norm() <= 1e-20) {
        radial = feature.p - feature.circleCenter;
        radial -= normal * radial.dot(normal);
    }
    if (radial.norm() <= 1e-20) {
        return feature.circleCenter;
    }
    return feature.circleCenter + feature.circleRadius * radial.normalized();
}

Vec3 projectToEllipse(const Vec3& p, const VertexState& feature) {
    Vec3 major = feature.ellipseMajorAxis;
    Vec3 minor = feature.ellipseMinorAxis;
    Vec3 normal = feature.ellipseNormal;
    if (major.norm() <= 1e-20 || minor.norm() <= 1e-20 || normal.norm() <= 1e-20 ||
        feature.ellipseMajorRadius <= 1e-20 || feature.ellipseMinorRadius <= 1e-20) {
        return p;
    }
    major.normalize();
    minor.normalize();
    normal.normalize();

    Vec3 delta = p - feature.ellipseCenter;
    delta -= normal * delta.dot(normal);
    if (delta.norm() <= 1e-20) {
        delta = feature.p - feature.ellipseCenter;
        delta -= normal * delta.dot(normal);
    }
    if (delta.norm() <= 1e-20) {
        return feature.ellipseCenter + feature.ellipseMajorRadius * major;
    }

    const double theta =
        std::atan2(delta.dot(minor) / feature.ellipseMinorRadius, delta.dot(major) / feature.ellipseMajorRadius);
    return feature.ellipseCenter + feature.ellipseMajorRadius * std::cos(theta) * major +
           feature.ellipseMinorRadius * std::sin(theta) * minor;
}

void refreshCircularTangent(VertexState& vertex) {
    if (!vertex.circularFeature) {
        return;
    }
    Vec3 normal = vertex.circleNormal;
    if (normal.norm() <= 1e-20) {
        return;
    }
    normal.normalize();
    Vec3 radial = vertex.p - vertex.circleCenter;
    radial -= normal * radial.dot(normal);
    if (radial.norm() <= 1e-20) {
        return;
    }
    vertex.curveTangent = normal.cross(radial).normalized();
}

void refreshEllipseTangent(VertexState& vertex) {
    if (vertex.featurePrimitive != FeatureCurveKind::Ellipse) {
        return;
    }
    Vec3 major = vertex.ellipseMajorAxis;
    Vec3 minor = vertex.ellipseMinorAxis;
    Vec3 normal = vertex.ellipseNormal;
    if (major.norm() <= 1e-20 || minor.norm() <= 1e-20 || normal.norm() <= 1e-20 ||
        vertex.ellipseMajorRadius <= 1e-20 || vertex.ellipseMinorRadius <= 1e-20) {
        return;
    }
    major.normalize();
    minor.normalize();
    normal.normalize();
    Vec3 delta = vertex.p - vertex.ellipseCenter;
    delta -= normal * delta.dot(normal);
    if (delta.norm() <= 1e-20) {
        return;
    }
    const double theta =
        std::atan2(delta.dot(minor) / vertex.ellipseMinorRadius, delta.dot(major) / vertex.ellipseMajorRadius);
    Vec3 tangent =
        -vertex.ellipseMajorRadius * std::sin(theta) * major + vertex.ellipseMinorRadius * std::cos(theta) * minor;
    if (tangent.norm() > 1e-20) {
        vertex.curveTangent = tangent.normalized();
    }
}

bool projectBoundaryPlacement(const BoundaryProjectionInput& input, Vec3& position) {
    if (!input.decision.boundaryEdge) {
        return false;
    }

    const int keep = input.edge.keep;
    const int remove = input.edge.remove;
    const std::vector<VertexState>& vertices = input.vertices;
    const Vec3& a = vertices[keep].p;
    const Vec3& b = vertices[remove].p;
    const Vec3 edge = b - a;
    const double len2 = edge.squaredNorm();
    if (len2 <= 1e-30) {
        position = 0.5 * (a + b);
        return true;
    }
    const double t = std::clamp((position - a).dot(edge) / len2, 0.0, 1.0);
    position = a + t * edge;
    return true;
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
