#include "detail/PrimitiveFit.h"

#include <Eigen/Eigenvalues>
#include <Eigen/LU>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

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
    Vec3 ellipseCenter = Vec3::Zero();
    Vec3 ellipseMajorAxis = Vec3(1.0, 0.0, 0.0);
    Vec3 ellipseMinorAxis = Vec3(0.0, 1.0, 0.0);
    double majorRadius = 0.0;
    double minorRadius = 0.0;
};

/// In-plane loop coordinates, centered on the loop centroid and scaled by the
/// RMS radius so both algebraic fits below operate on well-conditioned data
/// (the ellipse design matrix carries fourth-order monomials).
struct PlanePoints {
    bool valid = false;
    std::vector<double> x;
    std::vector<double> y;
    double scale = 0.0;
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

PlanePoints projectLoopToPlane(const Mesh& mesh, const FeatureLoop& loop, const LoopFitFrame& frame) {
    PlanePoints points;
    points.x.reserve(loop.vertices.size());
    points.y.reserve(loop.vertices.size());
    double radiusSq = 0.0;
    for (int id : loop.vertices) {
        const Vec3 d = mesh.vertices[id] - frame.mean;
        const double x = d.dot(frame.majorAxis);
        const double y = d.dot(frame.minorAxis);
        if (!std::isfinite(x) || !std::isfinite(y)) {
            return points;
        }
        points.x.push_back(x);
        points.y.push_back(y);
        radiusSq += x * x + y * y;
    }
    const double meanRadiusSq = radiusSq / static_cast<double>(loop.vertices.size());
    if (!(meanRadiusSq > 1e-24)) {
        return points;
    }
    points.scale = std::sqrt(meanRadiusSq);
    const double invScale = 1.0 / points.scale;
    for (std::size_t i = 0; i < points.x.size(); ++i) {
        points.x[i] *= invScale;
        points.y[i] *= invScale;
    }
    points.valid = true;
    return points;
}

struct PlaneCircle {
    bool valid = false;
    double cx = 0.0;
    double cy = 0.0;
    double radius = 0.0;
};

/// Kaasa algebraic fit, kept as the deterministic fallback when the Taubin
/// Newton step degenerates. Exact on full uniform loops; biased on short arcs.
PlaneCircle solveKasaCircle(const PlanePoints& points) {
    PlaneCircle circle;
    Eigen::Matrix3d ata = Eigen::Matrix3d::Zero();
    Eigen::Vector3d atb = Eigen::Vector3d::Zero();
    for (std::size_t i = 0; i < points.x.size(); ++i) {
        const double x = points.x[i];
        const double y = points.y[i];
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
    const double radiusSq = solution.z() + solution.x() * solution.x() + solution.y() * solution.y();
    if (radiusSq <= 1e-24 || !std::isfinite(radiusSq)) {
        return circle;
    }
    circle.valid = true;
    circle.cx = solution.x();
    circle.cy = solution.y();
    circle.radius = std::sqrt(radiusSq);
    return circle;
}

/// Taubin algebraic circle fit (Taubin 1991; Newton form after Chernov,
/// "Circular and Linear Regression", 2010). One order less essential bias than
/// Kaasa (O(sigma^4) vs O(sigma^2)), so partial arcs and non-uniform sampling
/// no longer collapse the radius estimate. The characteristic cubic is solved
/// by a guarded Newton iteration from eta = 0; any degeneracy falls back to
/// Kaasa so behavior on pathological input is unchanged.
PlaneCircle solveTaubinCircle(const PlanePoints& points) {
    PlaneCircle circle;
    const std::size_t n = points.x.size();
    if (n < 3) {
        return circle;
    }
    // The plane coordinates are centroid-centered by construction, so the
    // first-order moments vanish and the classic centered Taubin moments apply.
    const double invN = 1.0 / static_cast<double>(n);
    double Mxx = 0.0;
    double Myy = 0.0;
    double Mxy = 0.0;
    double Mxz = 0.0;
    double Myz = 0.0;
    double Mzz = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double x = points.x[i];
        const double y = points.y[i];
        const double z = x * x + y * y;
        Mxx += x * x;
        Myy += y * y;
        Mxy += x * y;
        Mxz += x * z;
        Myz += y * z;
        Mzz += z * z;
    }
    Mxx *= invN;
    Myy *= invN;
    Mxy *= invN;
    Mxz *= invN;
    Myz *= invN;
    Mzz *= invN;

    const double Mz = Mxx + Myy;
    const double covXy = Mxx * Myy - Mxy * Mxy;
    const double varZ = Mzz - Mz * Mz;
    const double a3 = 4.0 * Mz;
    const double a2 = -3.0 * Mz * Mz - Mzz;
    const double a1 = varZ * Mz + 4.0 * covXy * Mz - Mxz * Mxz - Myz * Myz;
    const double a0 = Mxz * (Mxz * Myy - Myz * Mxy) + Myz * (Myz * Mxx - Mxz * Mxy) - varZ * covXy;
    const double a22 = a2 + a2;
    const double a33 = a3 + a3 + a3;

    double eta = 0.0;
    double value = a0;
    for (int iteration = 0; iteration < 32; ++iteration) {
        const double derivative = a1 + eta * (a22 + eta * a33);
        if (!std::isfinite(derivative) || std::abs(derivative) <= 1e-30) {
            return circle;
        }
        const double etaNew = eta - value / derivative;
        if (!std::isfinite(etaNew) || etaNew == eta) {
            break;
        }
        const double valueNew = a0 + etaNew * (a1 + etaNew * (a2 + etaNew * a3));
        if (std::abs(valueNew) >= std::abs(value)) {
            break;
        }
        eta = etaNew;
        value = valueNew;
    }

    const double det = eta * eta - eta * Mz + covXy;
    // rcond-style degeneracy guard: the 2x2 center system determinant must not
    // vanish relative to the (unit RMS) coordinate scale.
    if (!std::isfinite(det) || std::abs(det) <= 1e-12) {
        return circle;
    }
    const double cx = (Mxz * (Myy - eta) - Myz * Mxy) / det / 2.0;
    const double cy = (Myz * (Mxx - eta) - Mxz * Mxy) / det / 2.0;
    const double radiusSq = cx * cx + cy * cy + Mz;
    if (!std::isfinite(cx) || !std::isfinite(cy) || !(radiusSq > 1e-24)) {
        return circle;
    }
    circle.valid = true;
    circle.cx = cx;
    circle.cy = cy;
    circle.radius = std::sqrt(radiusSq);
    return circle;
}

struct PlaneEllipse {
    bool valid = false;
    double cx = 0.0;
    double cy = 0.0;
    double majorRadius = 0.0;
    double minorRadius = 0.0;
    // Unit direction of the major axis in the (majorAxis, minorAxis) frame.
    double majorDirX = 1.0;
    double majorDirY = 0.0;
};

/// Halir-Flusser numerically stable direct least-squares ellipse fit
/// (Fitzgibbon constraint 4ac - b^2 = 1, solved on the 3x3 reduced system).
/// Unlike the previous second-moment axis estimate, this is exact for any
/// vertex distribution on an exact ellipse and always returns a true ellipse.
PlaneEllipse solveHalirFlusserEllipse(const PlanePoints& points) {
    PlaneEllipse ellipse;
    const std::size_t n = points.x.size();
    if (n < 5) {
        return ellipse;
    }

    Eigen::Matrix3d s1 = Eigen::Matrix3d::Zero();
    Eigen::Matrix3d s2 = Eigen::Matrix3d::Zero();
    Eigen::Matrix3d s3 = Eigen::Matrix3d::Zero();
    for (std::size_t i = 0; i < n; ++i) {
        const double x = points.x[i];
        const double y = points.y[i];
        const Eigen::Vector3d d1(x * x, x * y, y * y);
        const Eigen::Vector3d d2(x, y, 1.0);
        s1 += d1 * d1.transpose();
        s2 += d1 * d2.transpose();
        s3 += d2 * d2.transpose();
    }

    const Eigen::FullPivLU<Eigen::Matrix3d> s3Lu(s3);
    // Degeneracy rejection: collinear or repeated points make S3 singular.
    if (!s3Lu.isInvertible() || s3Lu.rcond() < 1e-12) {
        return ellipse;
    }
    const Eigen::Matrix3d t = -s3Lu.solve(s2.transpose());
    const Eigen::Matrix3d m = s1 + s2 * t;
    Eigen::Matrix3d reduced;
    reduced.row(0) = m.row(2) / 2.0;
    reduced.row(1) = -m.row(1);
    reduced.row(2) = m.row(0) / 2.0;

    const Eigen::EigenSolver<Eigen::Matrix3d> eig(reduced);
    if (eig.info() != Eigen::Success) {
        return ellipse;
    }

    Eigen::Vector3d a1 = Eigen::Vector3d::Zero();
    bool found = false;
    for (int i = 0; i < 3; ++i) {
        if (std::abs(eig.eigenvalues()(i).imag()) > 1e-9 * (1.0 + std::abs(eig.eigenvalues()(i).real()))) {
            continue;
        }
        const Eigen::Vector3d candidate = eig.eigenvectors().col(i).real();
        const double constraint = 4.0 * candidate(0) * candidate(2) - candidate(1) * candidate(1);
        if (constraint > 0.0 && candidate.allFinite()) {
            a1 = candidate;
            found = true;
            break;
        }
    }
    if (!found) {
        return ellipse;
    }
    const Eigen::Vector3d a2 = t * a1;

    const double a = a1(0);
    const double b = a1(1);
    const double c = a1(2);
    const double d = a2(0);
    const double e = a2(1);
    const double f = a2(2);

    const double den = b * b - 4.0 * a * c;
    if (!(den < -1e-18)) {
        return ellipse;
    }
    const double cx = (2.0 * c * d - b * e) / den;
    const double cy = (2.0 * a * e - b * d) / den;
    // Value of the conic at its center; the ellipse equation becomes
    // (p - c)^T Q (p - c) = -fc with Q = [[a, b/2], [b/2, c]].
    const double fc = a * cx * cx + b * cx * cy + c * cy * cy + d * cx + e * cy + f;
    Eigen::Matrix2d q;
    q << a, 0.5 * b, 0.5 * b, c;
    const Eigen::SelfAdjointEigenSolver<Eigen::Matrix2d> qEig(q);
    if (qEig.info() != Eigen::Success) {
        return ellipse;
    }
    const double lambda0 = qEig.eigenvalues()(0);
    const double lambda1 = qEig.eigenvalues()(1);
    if (std::abs(lambda0) <= 1e-18 || std::abs(lambda1) <= 1e-18) {
        return ellipse;
    }
    const double ratio0 = -fc / lambda0;
    const double ratio1 = -fc / lambda1;
    if (!(ratio0 > 0.0) || !(ratio1 > 0.0)) {
        return ellipse;
    }
    const double semi0 = std::sqrt(ratio0);
    const double semi1 = std::sqrt(ratio1);
    const int majorIndex = semi0 >= semi1 ? 0 : 1;
    const Eigen::Vector2d majorDir = qEig.eigenvectors().col(majorIndex);
    if (!std::isfinite(semi0) || !std::isfinite(semi1) || !majorDir.allFinite()) {
        return ellipse;
    }

    ellipse.valid = true;
    ellipse.cx = cx;
    ellipse.cy = cy;
    ellipse.majorRadius = std::max(semi0, semi1);
    ellipse.minorRadius = std::min(semi0, semi1);
    ellipse.majorDirX = majorDir(0);
    ellipse.majorDirY = majorDir(1);
    return ellipse;
}

PlaneCircleFit solvePlaneCircle(const Mesh& mesh, const FeatureLoop& loop, const LoopFitFrame& frame) {
    PlaneCircleFit circle;
    const PlanePoints points = projectLoopToPlane(mesh, loop, frame);
    if (!points.valid) {
        return circle;
    }

    // Circle: Taubin first, Kaasa as the fallback rung.
    PlaneCircle planeCircle = solveTaubinCircle(points);
    if (!planeCircle.valid) {
        planeCircle = solveKasaCircle(points);
    }
    if (!planeCircle.valid) {
        return circle;
    }

    const double scale = points.scale;
    circle.center = frame.mean + (planeCircle.cx * frame.majorAxis + planeCircle.cy * frame.minorAxis) * scale;
    circle.radius = planeCircle.radius * scale;
    if (!(circle.radius > 1e-20) || !std::isfinite(circle.radius)) {
        return circle;
    }

    const PlaneEllipse ellipse = solveHalirFlusserEllipse(points);
    if (ellipse.valid) {
        circle.ellipseCenter = frame.mean + (ellipse.cx * frame.majorAxis + ellipse.cy * frame.minorAxis) * scale;
        Vec3 majorAxis = ellipse.majorDirX * frame.majorAxis + ellipse.majorDirY * frame.minorAxis;
        if (majorAxis.norm() <= 1e-20) {
            return circle;
        }
        majorAxis.normalize();
        // Deterministic orientation: align with the PCA frame so repeated runs
        // and near-circular fits report a stable axis sign.
        if (majorAxis.dot(frame.majorAxis) < 0.0 ||
            (majorAxis.dot(frame.majorAxis) == 0.0 && majorAxis.dot(frame.minorAxis) < 0.0)) {
            majorAxis = -majorAxis;
        }
        Vec3 minorAxis = frame.normal.cross(majorAxis);
        if (minorAxis.norm() <= 1e-20) {
            return circle;
        }
        minorAxis.normalize();
        circle.ellipseMajorAxis = majorAxis;
        circle.ellipseMinorAxis = minorAxis;
        circle.majorRadius = ellipse.majorRadius * scale;
        circle.minorRadius = ellipse.minorRadius * scale;
    } else {
        // Fallback rung: previous second-moment estimate around the PCA axes.
        double majorSq = 0.0;
        double minorSq = 0.0;
        for (std::size_t i = 0; i < points.x.size(); ++i) {
            majorSq += points.x[i] * points.x[i];
            minorSq += points.y[i] * points.y[i];
        }
        const double invN = 1.0 / static_cast<double>(points.x.size());
        circle.ellipseCenter = frame.mean;
        circle.ellipseMajorAxis = frame.majorAxis;
        circle.ellipseMinorAxis = frame.minorAxis;
        circle.majorRadius = std::sqrt(std::max(0.0, 2.0 * majorSq * invN)) * scale;
        circle.minorRadius = std::sqrt(std::max(0.0, 2.0 * minorSq * invN)) * scale;
    }
    if (circle.majorRadius <= 1e-20 || circle.minorRadius <= 1e-20) {
        return circle;
    }

    circle.valid = true;
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

        const Vec3 de = mesh.vertices[id] - fit.ellipseCenter;
        const double ex = de.dot(fit.majorAxis);
        const double ey = de.dot(fit.minorAxis);
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
    if (fit.primitive == FeaturePrimitiveType::Ellipse) {
        // Report the directly fitted ellipse center for elliptical loops; it
        // matches the circle center on symmetric loops and is strictly better
        // on asymmetric sampling.
        fit.center = fit.ellipseCenter;
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
    fit.ellipseCenter = circle.ellipseCenter;
    fit.normal = frame.normal;
    fit.majorAxis = circle.ellipseMajorAxis;
    fit.minorAxis = circle.ellipseMinorAxis;
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
