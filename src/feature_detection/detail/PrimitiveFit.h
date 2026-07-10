#pragma once

#include "algorithms/feature_detection/FeatureTypes.h"

#include <vector>

namespace manumesh::feature::primitive_fit_detail {

struct PrimitiveFit {
    bool valid = false;
    FeaturePrimitiveType primitive = FeaturePrimitiveType::Unknown;
    Vec3 center = Vec3::Zero();
    Vec3 normal = Vec3(0.0, 0.0, 1.0);
    Vec3 majorAxis = Vec3(1.0, 0.0, 0.0);
    Vec3 minorAxis = Vec3(0.0, 1.0, 0.0);
    double radius = 0.0;
    double majorRadius = 0.0;
    double minorRadius = 0.0;
    double axisRatio = 0.0;
    double rmsRadialError = 0.0;
    double maxRadialError = 0.0;
    double rmsEllipseError = 0.0;
    double maxEllipseError = 0.0;
    double rmsPlaneError = 0.0;
    double maxPlaneError = 0.0;
};

PrimitiveFit fitPrimitive(const Mesh& mesh, const FeatureLoop& loop, const FeatureOptions& options);
void applyPrimitiveFit(const PrimitiveFit& fit, FeatureLoop& loop);
bool cycleEdgesFollowCircle(
    const std::vector<int>& vertices, const PrimitiveFit& fit, const Mesh& mesh, const FeatureOptions& options
);
DirectionalCurveError measureLoopAgainstCircle(
    const Mesh& mesh, const FeatureLoop& loop, const Vec3& center, const Vec3& normalIn, double radius
);

} // namespace manumesh::feature::primitive_fit_detail
