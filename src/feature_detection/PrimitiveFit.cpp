#include "detail/PrimitiveFit.h"

#include <Eigen/Eigenvalues>
#include <algorithm>
#include <cmath>

namespace manumesh::feature::primitive_fit_detail {
namespace {

struct LoopFitFrame {
    bool valid = false;
    Vec3 mean = Vec3::Zero();
    Vec3 normal = Vec3(0.0, 0.0, 1.0);
    Vec3 majorAxis = Vec3(1.0, 0.0, 0.0);
    Vec3 minorAxis = Vec3(0.0, 1.0, 0.0);
};

struct PlaneCircleFit {
    bool valid = false;
    Vec3 center = Vec3::Zero();
    double radius = 0.0;
    double majorRadius = 0.0;
    double minorRadius = 0.0;
};

LoopFitFrame fitLoopFrame(const Mesh& mesh, const FeatureLoop& loop) {
    LoopFitFrame frame;
    Vec3 mean = Vec3::Zero();
    for (int id : loop.vertices) {
        mean += mesh.vertices[id];
    }
    mean /= static_cast<double>(loop.vertices.size());

    Eigen::Matrix3d covariance = Eigen::Matrix3d::Zero();
    for (int id : loop.vertices) {
        const Vec3 d = mesh.vertices[id] - mean;
        covariance += d * d.transpose();
    }

    Eigen::SelfAdjointEigenSolver<Eigen::Matrix3d> eig(covariance);
    if (eig.info() != Eigen::Success) {
        return frame;
    }

    Vec3 normal = eig.eigenvectors().col(0).normalized();
    Vec3 u = eig.eigenvectors().col(2).normalized();
    Vec3 v = normal.cross(u).normalized();
    if (u.norm() <= 1e-20 || v.norm() <= 1e-20) {
        return frame;
    }

    frame.valid = true;
    frame.mean = mean;
    frame.normal = normal;
    frame.majorAxis = u;
    frame.minorAxis = v;
    return frame;
}

PlaneCircleFit solvePlaneCircle(const Mesh& mesh, const FeatureLoop& loop, const LoopFitFrame& frame) {
    PlaneCircleFit circle;
    Eigen::Matrix3d ata = Eigen::Matrix3d::Zero();
    Eigen::Vector3d atb = Eigen::Vector3d::Zero();
    double majorSq = 0.0;
    double minorSq = 0.0;
    for (int id : loop.vertices) {
        const Vec3 d = mesh.vertices[id] - frame.mean;
        const double x = d.dot(frame.majorAxis);
        const double y = d.dot(frame.minorAxis);
        majorSq += x * x;
        minorSq += y * y;
        Eigen::Vector3d row(2.0 * x, 2.0 * y, 1.0);
        ata += row * row.transpose();
        atb += row * (x * x + y * y);
    }

    Eigen::LDLT<Eigen::Matrix3d> ldlt(ata);
    if (ldlt.info() != Eigen::Success) {
        return circle;
    }

    const Eigen::Vector3d solution = ldlt.solve(atb);
    if (!solution.allFinite()) {
        return circle;
    }

    const double cx = solution.x();
    const double cy = solution.y();
    const double radiusSq = solution.z() + cx * cx + cy * cy;
    if (radiusSq <= 1e-24 || !std::isfinite(radiusSq)) {
        return circle;
    }

    const double invN = 1.0 / static_cast<double>(loop.vertices.size());
    const double majorRadius = std::sqrt(std::max(0.0, 2.0 * majorSq * invN));
    const double minorRadius = std::sqrt(std::max(0.0, 2.0 * minorSq * invN));
    if (majorRadius <= 1e-20 || minorRadius <= 1e-20) {
        return circle;
    }

    circle.valid = true;
    circle.center = frame.mean + cx * frame.majorAxis + cy * frame.minorAxis;
    circle.radius = std::sqrt(radiusSq);
    circle.majorRadius = majorRadius;
    circle.minorRadius = minorRadius;
    return circle;
}

void measurePrimitiveFitErrors(
    const Mesh& mesh, const FeatureLoop& loop, const LoopFitFrame& frame, PrimitiveFit& fit
) {
    const double invN = 1.0 / static_cast<double>(loop.vertices.size());
    double radialSq = 0.0;
    double planeSq = 0.0;
    double ellipseSq = 0.0;
    double radialMax = 0.0;
    double planeMax = 0.0;
    double ellipseMax = 0.0;
    for (int id : loop.vertices) {
        const Vec3 d = mesh.vertices[id] - fit.center;
        const double plane = d.dot(frame.normal);
        const Vec3 inPlane = d - plane * frame.normal;
        const double radial = inPlane.norm() - fit.radius;
        radialSq += radial * radial;
        planeSq += plane * plane;
        radialMax = std::max(radialMax, std::abs(radial));
        planeMax = std::max(planeMax, std::abs(plane));

        const Vec3 de = mesh.vertices[id] - frame.mean;
        const double ex = de.dot(frame.majorAxis);
        const double ey = de.dot(frame.minorAxis);
        const double ellipse =
            (std::sqrt(
                 (ex * ex) / (fit.majorRadius * fit.majorRadius) + (ey * ey) / (fit.minorRadius * fit.minorRadius)
             ) -
             1.0) *
            std::sqrt(fit.majorRadius * fit.minorRadius);
        ellipseSq += ellipse * ellipse;
        ellipseMax = std::max(ellipseMax, std::abs(ellipse));
    }

    fit.rmsRadialError = std::sqrt(radialSq * invN);
    fit.maxRadialError = radialMax;
    fit.rmsEllipseError = std::sqrt(ellipseSq * invN);
    fit.maxEllipseError = ellipseMax;
    fit.rmsPlaneError = std::sqrt(planeSq * invN);
    fit.maxPlaneError = planeMax;
}

void classifyPrimitiveFit(const FeatureOptions& options, PrimitiveFit& fit) {
    const double relRms = (fit.rmsRadialError + fit.rmsPlaneError) / std::max(1e-12, fit.radius);
    const double relMax = std::max(fit.maxRadialError, fit.maxPlaneError) / std::max(1e-12, fit.radius);
    const bool circleFit = relRms <= options.circleFitRelativeThreshold &&
                           relMax <= std::max(3.0 * options.circleFitRelativeThreshold, 0.08);

    const double ellipseScale = std::max(1e-12, std::sqrt(fit.majorRadius * fit.minorRadius));
    const double ellipseRelRms = (fit.rmsEllipseError + fit.rmsPlaneError) / ellipseScale;
    const double ellipseRelMax = std::max(fit.maxEllipseError, fit.maxPlaneError) / ellipseScale;
    const bool ellipseFit = ellipseRelRms <= options.ellipseFitRelativeThreshold &&
                            ellipseRelMax <= std::max(3.0 * options.ellipseFitRelativeThreshold, 0.08);

    if (circleFit) {
        const double axisError = std::abs(1.0 - fit.axisRatio);
        if (axisError <= std::min(0.01, 0.25 * options.nearCircleAxisRatioTolerance)) {
            fit.primitive = FeaturePrimitiveType::Circle;
        } else if (axisError <= options.nearCircleAxisRatioTolerance) {
            fit.primitive = FeaturePrimitiveType::NearCircle;
        } else {
            fit.primitive = FeaturePrimitiveType::Ellipse;
        }
    } else if (ellipseFit) {
        fit.primitive = std::abs(1.0 - fit.axisRatio) <= options.nearCircleAxisRatioTolerance
                            ? FeaturePrimitiveType::NearCircle
                            : FeaturePrimitiveType::Ellipse;
    } else {
        fit.primitive = FeaturePrimitiveType::PolygonalLoop;
    }
}

} // namespace

PrimitiveFit fitPrimitive(const Mesh& mesh, const FeatureLoop& loop, const FeatureOptions& options) {
    PrimitiveFit fit;
    if (loop.vertices.size() < 5) {
        return fit;
    }

    const LoopFitFrame frame = fitLoopFrame(mesh, loop);
    if (!frame.valid) {
        return fit;
    }
    const PlaneCircleFit circle = solvePlaneCircle(mesh, loop, frame);
    if (!circle.valid) {
        return fit;
    }

    fit.valid = true;
    fit.center = circle.center;
    fit.normal = frame.normal;
    fit.majorAxis = frame.majorAxis;
    fit.minorAxis = frame.minorAxis;
    fit.radius = circle.radius;
    fit.majorRadius = circle.majorRadius;
    fit.minorRadius = circle.minorRadius;
    fit.axisRatio = circle.minorRadius / std::max(1e-12, circle.majorRadius);
    measurePrimitiveFitErrors(mesh, loop, frame, fit);
    classifyPrimitiveFit(options, fit);
    return fit;
}

void applyPrimitiveFit(const PrimitiveFit& fit, FeatureLoop& loop) {
    if (!fit.valid) {
        return;
    }
    loop.primitive = fit.primitive;
    loop.circular = fit.primitive == FeaturePrimitiveType::Circle || fit.primitive == FeaturePrimitiveType::NearCircle;
    loop.center = fit.center;
    loop.normal = fit.normal;
    loop.majorAxis = fit.majorAxis;
    loop.minorAxis = fit.minorAxis;
    loop.radius = fit.radius;
    loop.majorRadius = fit.majorRadius;
    loop.minorRadius = fit.minorRadius;
    loop.axisRatio = fit.axisRatio;
    loop.rmsRadialError = fit.rmsRadialError;
    loop.maxRadialError = fit.maxRadialError;
    loop.rmsEllipseError = fit.rmsEllipseError;
    loop.maxEllipseError = fit.maxEllipseError;
    loop.rmsPlaneError = fit.rmsPlaneError;
    loop.maxPlaneError = fit.maxPlaneError;
}

bool cycleEdgesFollowCircle(
    const std::vector<int>& vertices, const PrimitiveFit& fit, const Mesh& mesh, const FeatureOptions& options
) {
    if (!fit.valid ||
        (fit.primitive != FeaturePrimitiveType::Circle && fit.primitive != FeaturePrimitiveType::NearCircle) ||
        fit.radius <= 1e-20 || fit.normal.norm() <= 1e-20) {
        return false;
    }

    Vec3 normal = fit.normal.normalized();
    const double allowed = std::max(3.0 * options.circleFitRelativeThreshold, 0.08) * fit.radius;
    for (int i = 0; i < static_cast<int>(vertices.size()); ++i) {
        const Vec3 midpoint = 0.5 * (mesh.vertices[vertices[i]] + mesh.vertices[vertices[(i + 1) % vertices.size()]]);
        const Vec3 delta = midpoint - fit.center;
        const double plane = std::abs(delta.dot(normal));
        const Vec3 inPlane = delta - normal * delta.dot(normal);
        const double radial = std::abs(inPlane.norm() - fit.radius);
        if (std::max(radial, plane) > allowed) {
            return false;
        }
    }
    return true;
}

DirectionalCurveError measureLoopAgainstCircle(
    const Mesh& mesh, const FeatureLoop& loop, const Vec3& center, const Vec3& normalIn, double radius
) {
    DirectionalCurveError error;
    Vec3 normal = normalIn;
    if (normal.norm() <= 1e-20 || radius <= 1e-20) {
        return error;
    }
    normal.normalize();

    for (int id : loop.vertices) {
        if (id < 0 || id >= static_cast<int>(mesh.vertices.size())) {
            continue;
        }
        const Vec3 d = mesh.vertices[id] - center;
        const double plane = d.dot(normal);
        const Vec3 inPlane = d - plane * normal;
        const double radial = inPlane.norm() - radius;
        error.radialRms += radial * radial;
        error.planeRms += plane * plane;
        error.radialMax = std::max(error.radialMax, std::abs(radial));
        error.planeMax = std::max(error.planeMax, std::abs(plane));
        ++error.samples;
    }
    if (error.samples > 0) {
        const double inv = 1.0 / static_cast<double>(error.samples);
        error.radialRms = std::sqrt(error.radialRms * inv);
        error.planeRms = std::sqrt(error.planeRms * inv);
    }
    return error;
}

} // namespace manumesh::feature::primitive_fit_detail
