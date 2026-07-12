#include "algorithms/feature_detection/FeatureComparison.h"

#include <cmath>
#include <gtest/gtest.h>
#include <limits>
#include <vector>

namespace {

using manumesh::Mesh;
using manumesh::Vec3;
using manumesh::feature::FeatureAnalysis;
using manumesh::feature::FeatureLoop;
using manumesh::feature::LoopMatch;
using manumesh::feature::LoopMatchOptions;
using manumesh::feature::LoopMatchReport;
using manumesh::feature::LoopMatchStatus;
using manumesh::feature::matchCircularLoops;

constexpr double kTestPi = 3.141592653589793238462643383279502884;

/// Appends `count` vertices on the circle (center, normal +Z, radius) to the
/// mesh and returns a circular FeatureLoop that references them.
FeatureLoop appendCircleLoop(Mesh& mesh, int id, const Vec3& center, double radius, int count) {
    FeatureLoop loop;
    loop.id = id;
    loop.closed = true;
    loop.circular = true;
    loop.primitive = manumesh::feature::FeaturePrimitiveType::Circle;
    loop.center = center;
    loop.normal = Vec3(0.0, 0.0, 1.0);
    loop.radius = radius;
    for (int i = 0; i < count; ++i) {
        const double angle = 2.0 * kTestPi * static_cast<double>(i) / static_cast<double>(count);
        loop.vertices.push_back(static_cast<int>(mesh.vertices.size()));
        mesh.vertices.push_back(center + Vec3(radius * std::cos(angle), radius * std::sin(angle), 0.0));
    }
    loop.edgeCount = count;
    return loop;
}

} // namespace

// Two identical circular loops must both come back as strong matches with
// zero errors and exact directional agreement, in original loop order.
TEST(FeatureComparison, IdenticalLoopsMatchStronglyWithZeroErrors) {
    Mesh original;
    Mesh simplified;
    FeatureAnalysis originalFeatures;
    FeatureAnalysis simplifiedFeatures;
    originalFeatures.loops.push_back(appendCircleLoop(original, 0, Vec3(0.0, 0.0, 0.0), 1.0, 12));
    originalFeatures.loops.push_back(appendCircleLoop(original, 1, Vec3(5.0, 0.0, 0.0), 0.5, 10));
    simplifiedFeatures.loops.push_back(appendCircleLoop(simplified, 0, Vec3(0.0, 0.0, 0.0), 1.0, 8));
    simplifiedFeatures.loops.push_back(appendCircleLoop(simplified, 1, Vec3(5.0, 0.0, 0.0), 0.5, 8));

    const LoopMatchReport report = matchCircularLoops(originalFeatures, simplifiedFeatures, simplified);

    EXPECT_EQ(2, report.originalCircularLoops);
    EXPECT_EQ(2, report.simplifiedCircularLoops);
    EXPECT_EQ(2, report.matchedLoops);
    EXPECT_EQ(0, report.missingLoops);
    ASSERT_EQ(2u, report.matches.size());
    for (int i = 0; i < 2; ++i) {
        const LoopMatch& match = report.matches[i];
        EXPECT_EQ(LoopMatchStatus::Matched, match.status);
        EXPECT_EQ(i, match.originalLoopId);
        EXPECT_EQ(i, match.simplifiedLoopIndex);
        EXPECT_EQ(8, match.simplifiedVertices);
        EXPECT_NEAR(0.0, match.centerError, 1e-12);
        EXPECT_NEAR(0.0, match.radiusError, 1e-12);
        EXPECT_NEAR(0.0, match.normalAngleDeg, 1e-9);
        EXPECT_NEAR(0.0, match.directional.radialMax, 1e-12);
        EXPECT_NEAR(0.0, match.directional.planeMax, 1e-12);
        EXPECT_EQ(8, match.directional.samples);
    }
}

// Radius drift inside the plausible band but outside the strong band must be
// classified weak_match, and the directional error must report the drift.
TEST(FeatureComparison, RadiusDriftInsidePlausibleBandIsWeakMatch) {
    Mesh original;
    Mesh simplified;
    FeatureAnalysis originalFeatures;
    FeatureAnalysis simplifiedFeatures;
    originalFeatures.loops.push_back(appendCircleLoop(original, 0, Vec3(0.0, 0.0, 0.0), 1.0, 12));
    // 15% radius drift: > matchedRadiusErrorRel (8%), < plausibleRadiusErrorRel (20%).
    simplifiedFeatures.loops.push_back(appendCircleLoop(simplified, 0, Vec3(0.0, 0.0, 0.0), 1.15, 12));

    LoopMatchOptions options;
    options.referenceDiagonal = 10.0;
    const LoopMatchReport report = matchCircularLoops(originalFeatures, simplifiedFeatures, simplified, options);

    EXPECT_EQ(1, report.matchedLoops);
    EXPECT_EQ(0, report.missingLoops);
    ASSERT_EQ(1u, report.matches.size());
    const LoopMatch& match = report.matches.front();
    EXPECT_EQ(LoopMatchStatus::WeakMatch, match.status);
    EXPECT_EQ("weak_match", manumesh::feature::toString(match.status));
    EXPECT_NEAR(0.15, match.radiusError, 1e-12);
    EXPECT_NEAR(0.15, match.directional.radialMax, 1e-12);
    EXPECT_EQ(0, static_cast<int>(std::lround(match.normalAngleDeg)));

    // The same drift with a tighter plausible band becomes missing.
    LoopMatchOptions strict = options;
    strict.plausibleRadiusErrorRel = 0.10;
    const LoopMatchReport strictReport = matchCircularLoops(originalFeatures, simplifiedFeatures, simplified, strict);
    ASSERT_EQ(1u, strictReport.matches.size());
    EXPECT_EQ(LoopMatchStatus::Missing, strictReport.matches.front().status);
}

// A vanished loop must be reported missing with zeroed errors while the
// surviving loop still matches; each simplified loop is consumed only once.
TEST(FeatureComparison, VanishedLoopIsMissingAndSimplifiedLoopsAreConsumedOnce) {
    Mesh original;
    Mesh simplified;
    FeatureAnalysis originalFeatures;
    FeatureAnalysis simplifiedFeatures;
    originalFeatures.loops.push_back(appendCircleLoop(original, 0, Vec3(0.0, 0.0, 0.0), 1.0, 12));
    originalFeatures.loops.push_back(appendCircleLoop(original, 1, Vec3(0.2, 0.0, 0.0), 1.0, 12));
    // Only one simplified loop survives; both originals would pick it first.
    simplifiedFeatures.loops.push_back(appendCircleLoop(simplified, 0, Vec3(0.0, 0.0, 0.0), 1.0, 8));

    LoopMatchOptions options;
    options.referenceDiagonal = 10.0;
    const LoopMatchReport report = matchCircularLoops(originalFeatures, simplifiedFeatures, simplified, options);

    EXPECT_EQ(2, report.originalCircularLoops);
    EXPECT_EQ(1, report.simplifiedCircularLoops);
    EXPECT_EQ(1, report.matchedLoops);
    EXPECT_EQ(1, report.missingLoops);
    ASSERT_EQ(2u, report.matches.size());
    EXPECT_EQ(LoopMatchStatus::Matched, report.matches[0].status);
    EXPECT_EQ(0, report.matches[0].simplifiedLoopIndex);

    const LoopMatch& missing = report.matches[1];
    EXPECT_EQ(LoopMatchStatus::Missing, missing.status);
    EXPECT_EQ(-1, missing.simplifiedLoopIndex);
    EXPECT_EQ(0, missing.simplifiedVertices);
    EXPECT_EQ(0.0, missing.centerError);
    EXPECT_EQ(0.0, missing.radiusError);
    EXPECT_EQ(0.0, missing.normalAngleDeg);
    EXPECT_EQ(0, missing.directional.samples);
}

// A lower combined score must not hide a slightly higher-scoring candidate
// that satisfies all three plausible thresholds.
TEST(FeatureComparison, ChoosesBestPlausibleCandidateAfterThresholdFiltering) {
    Mesh original;
    Mesh simplified;
    FeatureAnalysis originalFeatures;
    FeatureAnalysis simplifiedFeatures;
    originalFeatures.loops.push_back(appendCircleLoop(original, 0, Vec3(0.0, 0.0, 0.0), 1.0, 12));

    // With referenceDiagonal=10, candidate 0 scores 0.081 but exceeds the
    // plausible center limit (0.8). Candidate 1 scores 0.09 and is plausible.
    simplifiedFeatures.loops.push_back(appendCircleLoop(simplified, 0, Vec3(0.81, 0.0, 0.0), 1.0, 8));
    simplifiedFeatures.loops.push_back(appendCircleLoop(simplified, 1, Vec3(0.7, 0.0, 0.0), 1.02, 8));

    LoopMatchOptions options;
    options.referenceDiagonal = 10.0;
    const LoopMatchReport report = matchCircularLoops(originalFeatures, simplifiedFeatures, simplified, options);

    EXPECT_EQ(1, report.matchedLoops);
    EXPECT_EQ(0, report.missingLoops);
    ASSERT_EQ(1u, report.matches.size());
    const LoopMatch& match = report.matches.front();
    EXPECT_EQ(LoopMatchStatus::WeakMatch, match.status);
    EXPECT_EQ(1, match.simplifiedLoopIndex);
    EXPECT_NEAR(0.7, match.centerError, 1e-12);
    EXPECT_NEAR(0.02, match.radiusError, 1e-12);
}

TEST(FeatureComparison, RejectsInvalidLoopMatchOptionsBeforeMatching) {
    auto expectInvalid = [](const LoopMatchOptions& options) {
        const manumesh::Status status = manumesh::feature::validateLoopMatchOptions(options);
        EXPECT_FALSE(status.ok());
        EXPECT_EQ(manumesh::StatusCode::InvalidArgument, status.code());
    };

    LoopMatchOptions options;
    options.plausibleCenterErrorRatio = std::numeric_limits<double>::quiet_NaN();
    expectInvalid(options);

    options = LoopMatchOptions{};
    options.matchedRadiusErrorRel = -0.01;
    expectInvalid(options);

    options = LoopMatchOptions{};
    options.plausibleNormalAngleDeg = 181.0;
    expectInvalid(options);

    options = LoopMatchOptions{};
    options.referenceDiagonal = std::numeric_limits<double>::infinity();
    expectInvalid(options);

    options = LoopMatchOptions{};
    options.matchedCenterErrorRatio = options.plausibleCenterErrorRatio + 0.01;
    expectInvalid(options);

    FeatureAnalysis analysis;
    Mesh mesh;
    EXPECT_THROW(matchCircularLoops(analysis, analysis, mesh, options), std::invalid_argument);
}

// Non-circular loops must be ignored on both sides.
TEST(FeatureComparison, NonCircularLoopsAreIgnored) {
    Mesh original;
    Mesh simplified;
    FeatureAnalysis originalFeatures;
    FeatureAnalysis simplifiedFeatures;
    FeatureLoop polygonal = appendCircleLoop(original, 0, Vec3(0.0, 0.0, 0.0), 1.0, 6);
    polygonal.circular = false;
    polygonal.primitive = manumesh::feature::FeaturePrimitiveType::PolygonalLoop;
    originalFeatures.loops.push_back(polygonal);
    simplifiedFeatures.loops.push_back(appendCircleLoop(simplified, 0, Vec3(0.0, 0.0, 0.0), 1.0, 6));

    const LoopMatchReport report = matchCircularLoops(originalFeatures, simplifiedFeatures, simplified);

    EXPECT_EQ(0, report.originalCircularLoops);
    EXPECT_EQ(1, report.simplifiedCircularLoops);
    EXPECT_TRUE(report.matches.empty());
    EXPECT_EQ(0, report.matchedLoops);
    EXPECT_EQ(0, report.missingLoops);
}
