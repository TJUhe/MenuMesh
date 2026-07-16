/**
 * @file tests/unit/feature_detection/feature_detection_primitive_fit_tests.cpp
 * @brief Verifies feature detection primitive fit tests behavior in the ManuMesh tests.
 * @ingroup manumesh_tests
 *
 * @details The fixture and assertions document observable contracts, numeric tolerances, determinism requirements, and previously fixed regressions.
 */

#include "FeatureDetectionTestSupport.h"

#include "feature_detection/detail/PrimitiveFit.h"

#include <Eigen/Dense>
#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <vector>

namespace manumesh::test::feature_detection {
namespace {

namespace primitive_fit = feature::primitive_fit_detail;

constexpr double kTau = 6.283185307179586;

struct LoopFixture {
    Mesh mesh;
    FeatureLoop loop;
};

LoopFixture makePlanarLoopFixture(const std::vector<double>& x, const std::vector<double>& y) {
    LoopFixture fixture;
    fixture.mesh.vertices.reserve(x.size());
    for (std::size_t i = 0; i < x.size(); ++i) {
        fixture.mesh.vertices.emplace_back(x[i], y[i], 0.0);
        fixture.loop.vertices.push_back(static_cast<int>(i));
    }
    fixture.loop.id = 0;
    fixture.loop.edgeCount = static_cast<int>(x.size());
    fixture.loop.closed = true;
    return fixture;
}

/// The legacy Kaasa normal-equation circle fit, kept here as the comparison
/// baseline the Taubin replacement must beat on partial arcs.
double kasaRadius(const std::vector<double>& x, const std::vector<double>& y) {
    double meanX = 0.0;
    double meanY = 0.0;
    for (std::size_t i = 0; i < x.size(); ++i) {
        meanX += x[i];
        meanY += y[i];
    }
    meanX /= static_cast<double>(x.size());
    meanY /= static_cast<double>(y.size());

    Eigen::Matrix3d ata = Eigen::Matrix3d::Zero();
    Eigen::Vector3d atb = Eigen::Vector3d::Zero();
    for (std::size_t i = 0; i < x.size(); ++i) {
        const double px = x[i] - meanX;
        const double py = y[i] - meanY;
        const Eigen::Vector3d row(2.0 * px, 2.0 * py, 1.0);
        ata += row * row.transpose();
        atb += row * (px * px + py * py);
    }
    const Eigen::Vector3d solution = ata.ldlt().solve(atb);
    return std::sqrt(std::max(0.0, solution.z() + solution.x() * solution.x() + solution.y() * solution.y()));
}

/// Deterministic geometric (Gauss-Newton) circle fit used as the gold
/// standard: the perturbed arc's true best-fit radius differs from the
/// nominal one, so both algebraic fits are judged against this optimum.
double geometricRadius(const std::vector<double>& x, const std::vector<double>& y, double cx, double cy, double r) {
    for (int iteration = 0; iteration < 50; ++iteration) {
        Eigen::Matrix3d jtj = Eigen::Matrix3d::Zero();
        Eigen::Vector3d jtr = Eigen::Vector3d::Zero();
        for (std::size_t i = 0; i < x.size(); ++i) {
            const double dx = x[i] - cx;
            const double dy = y[i] - cy;
            const double dist = std::sqrt(dx * dx + dy * dy);
            if (dist <= 1e-12) {
                continue;
            }
            const Eigen::Vector3d row(-dx / dist, -dy / dist, -1.0);
            jtj += row * row.transpose();
            jtr += row * (dist - r);
        }
        const Eigen::Vector3d step = jtj.ldlt().solve(-jtr);
        cx += step(0);
        cy += step(1);
        r += step(2);
        if (step.norm() < 1e-14) {
            break;
        }
    }
    return r;
}

/// The legacy second-moment ellipse axis estimate (exact only for uniform
/// parameter-angle sampling), kept as the comparison baseline.
void momentAxes(const std::vector<double>& x, const std::vector<double>& y, double& major, double& minor) {
    double xx = 0.0;
    double yy = 0.0;
    for (std::size_t i = 0; i < x.size(); ++i) {
        xx += x[i] * x[i];
        yy += y[i] * y[i];
    }
    const double invN = 1.0 / static_cast<double>(x.size());
    major = std::sqrt(2.0 * xx * invN);
    minor = std::sqrt(2.0 * yy * invN);
}

} // namespace

TEST(FeatureDetectionPrimitiveFit, TaubinMatchesExactCircleUnderNonUniformSampling) {
    const double radius = 0.75;
    const int count = 40;
    std::vector<double> x;
    std::vector<double> y;
    for (int i = 0; i < count; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(count);
        // Strongly non-uniform but full coverage: dense near angle 0.
        const double angle = kTau * std::pow(t, 2.0);
        x.push_back(0.3 + radius * std::cos(angle));
        y.push_back(-0.2 + radius * std::sin(angle));
    }
    const LoopFixture fixture = makePlanarLoopFixture(x, y);
    const primitive_fit::PrimitiveFit fit = primitive_fit::fitPrimitive(fixture.mesh, fixture.loop, FeatureOptions{});

    ASSERT_TRUE(fit.valid);
    EXPECT_NEAR(fit.radius, radius, 1e-10);
    EXPECT_NEAR(fit.center.x(), 0.3, 1e-10);
    EXPECT_NEAR(fit.center.y(), -0.2, 1e-10);
    EXPECT_LT(fit.rmsRadialError, 1e-10);
}

TEST(FeatureDetectionPrimitiveFit, TaubinBeatsKasaOnNoisyPartialArc) {
    const double radius = 1.0;
    const int count = 48;
    std::vector<double> x;
    std::vector<double> y;
    for (int i = 0; i < count; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(count - 1);
        const double angle = 0.2 + 1.75 * t; // roughly a 100-degree arc
        // Deterministic high-frequency perturbation standing in for noise.
        const double noisy = radius + 0.02 * std::sin(37.0 * angle + 0.7) + 0.015 * std::cos(23.0 * angle);
        x.push_back(noisy * std::cos(angle));
        y.push_back(noisy * std::sin(angle));
    }
    const LoopFixture fixture = makePlanarLoopFixture(x, y);
    const primitive_fit::PrimitiveFit fit = primitive_fit::fitPrimitive(fixture.mesh, fixture.loop, FeatureOptions{});

    ASSERT_TRUE(fit.valid);
    // On a perturbed partial arc the true (geometric) best-fit radius is the
    // reference; the deterministic perturbation shifts it away from the
    // nominal radius.
    const double reference = geometricRadius(x, y, fit.center.x(), fit.center.y(), fit.radius);
    const double taubinError = std::abs(fit.radius - reference);
    const double kasaError = std::abs(kasaRadius(x, y) - reference);
    // Kaasa's (r + R)^2 weighting biases the radius on short arcs; Taubin's
    // first-order normalization must land far closer to the geometric optimum.
    EXPECT_LT(taubinError, 0.05 * kasaError);
    EXPECT_LT(taubinError, 1e-3 * radius);
    EXPECT_GT(kasaError, 5e-3 * radius);
}

TEST(FeatureDetectionPrimitiveFit, HalirFlusserRecoversEllipseUnderNonUniformSampling) {
    const double major = 0.8;
    const double minor = 0.45;
    const int count = 48;
    std::vector<double> x;
    std::vector<double> y;
    for (int i = 0; i < count; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(count);
        // Monotone but non-uniform parameter angle (derivative stays positive).
        const double angle = kTau * t + 0.45 * std::sin(kTau * t);
        x.push_back(major * std::cos(angle));
        y.push_back(minor * std::sin(angle));
    }
    const LoopFixture fixture = makePlanarLoopFixture(x, y);
    const primitive_fit::PrimitiveFit fit = primitive_fit::fitPrimitive(fixture.mesh, fixture.loop, FeatureOptions{});

    ASSERT_TRUE(fit.valid);
    EXPECT_NEAR(fit.majorRadius, major, 1e-9);
    EXPECT_NEAR(fit.minorRadius, minor, 1e-9);
    EXPECT_NEAR(fit.axisRatio, minor / major, 1e-9);
    EXPECT_LT(fit.rmsEllipseError, 1e-9);
    EXPECT_GT(std::abs(fit.majorAxis.x()), 0.999);
    EXPECT_GT(std::abs(fit.minorAxis.y()), 0.999);
    EXPECT_EQ(FeaturePrimitiveType::Ellipse, fit.primitive);

    // The retired moment estimator is measurably biased on the same data.
    double momentMajor = 0.0;
    double momentMinor = 0.0;
    momentAxes(x, y, momentMajor, momentMinor);
    const double momentError = std::max(std::abs(momentMajor - major), std::abs(momentMinor - minor));
    EXPECT_GT(momentError, 1e-3);
}

TEST(FeatureDetectionPrimitiveFit, HalirFlusserRecoversRotatedEllipseAxes) {
    const double major = 1.2;
    const double minor = 0.5;
    const double rotation = 0.5235987755982988; // 30 degrees
    const int count = 64;
    std::vector<double> x;
    std::vector<double> y;
    const double cosR = std::cos(rotation);
    const double sinR = std::sin(rotation);
    for (int i = 0; i < count; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(count);
        const double angle = kTau * t + 0.3 * std::sin(kTau * t);
        const double px = major * std::cos(angle);
        const double py = minor * std::sin(angle);
        x.push_back(cosR * px - sinR * py + 2.0);
        y.push_back(sinR * px + cosR * py - 1.0);
    }
    const LoopFixture fixture = makePlanarLoopFixture(x, y);
    const primitive_fit::PrimitiveFit fit = primitive_fit::fitPrimitive(fixture.mesh, fixture.loop, FeatureOptions{});

    ASSERT_TRUE(fit.valid);
    EXPECT_NEAR(fit.majorRadius, major, 1e-9);
    EXPECT_NEAR(fit.minorRadius, minor, 1e-9);
    EXPECT_NEAR(fit.center.x(), 2.0, 1e-9);
    EXPECT_NEAR(fit.center.y(), -1.0, 1e-9);
    const double axisAlignment = std::abs(fit.majorAxis.x() * cosR + fit.majorAxis.y() * sinR);
    EXPECT_NEAR(axisAlignment, 1.0, 1e-9);
    EXPECT_LT(fit.rmsEllipseError, 1e-9);
    EXPECT_EQ(FeaturePrimitiveType::Ellipse, fit.primitive);
}

} // namespace manumesh::test::feature_detection
