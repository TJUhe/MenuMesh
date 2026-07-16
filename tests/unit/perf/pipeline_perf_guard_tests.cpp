/**
 * @file tests/unit/perf/pipeline_perf_guard_tests.cpp
 * @brief Verifies pipeline perf guard tests behavior in the ManuMesh tests.
 * @ingroup manumesh_tests
 *
 * @details The fixture and assertions document observable contracts, numeric tolerances, determinism requirements, and previously fixed regressions.
 */

// Coarse wall-clock regression guards for the two pipeline entry points on a
// ~16k-face analytic sphere. These are not benchmarks: the limits are loose
// absolute ceilings whose only purpose is to fail fast when an accidental
// O(n^2) path (full-mesh rescan per collapse, quadratic neighborhood
// gathering, ...) slips into a hot loop. Exactly that class of bug existed
// before 2026-07: SimplificationRun::tryCollapse recomputed the input
// bounding-box diagonal (an O(V) scan) for every collapse attempt, which
// this guard would have caught (16k-face simplify took 15 s instead of 2 s).
//
// Machine baseline (2026-07, Windows x64, MinGW -O2 release, desktop CPU):
//   detectFeatureCurves (dihedral + normal tensor) ~0.18 s -> limit 2 s
//   simplify to ratio 0.2 (default options)        ~2.0 s  -> limit 8 s
// Both limits are >= 3x the measured time, so slower CI machines pass, while
// a quadratic regression (>10x at this size) cannot. The smooth-curvature
// channel is deliberately excluded here: it costs ~6 s on 16k faces, which
// would blow the fast-suite budget; its functional behavior is covered by
// feature_detection_analytic_tests.cpp on smaller fixtures.
#include "AnalyticFixtures.h"

#include "algorithms/feature_detection/FeatureDetector.h"
#include "algorithms/simplification/QEMSimplifier.h"

#include <gtest/gtest.h>

#include <chrono>

namespace {

namespace analytic = manumesh::test::analytic;
namespace feature = manumesh::feature;
namespace simplification = manumesh::simplification;

double elapsedSeconds(const std::chrono::steady_clock::time_point& start) {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}

} // namespace

TEST(PipelinePerfGuard, FeatureDetectionOnSixteenThousandFaceSphereStaysBounded) {
    const analytic::SphereFixture sphere = analytic::makeUvSphere(64, 128, 1.0);
    ASSERT_GT(static_cast<int>(sphere.mesh.faces.size()), 15000);

    feature::FeatureOptions options;
    options.featureAngleDeg = 40.0;
    options.useNormalTensorFeatures = true;
    options.useSmoothCurvatureFeatures = false;

    const auto start = std::chrono::steady_clock::now();
    const feature::FeatureAnalysis analysis = feature::detectFeatureCurves(sphere.mesh, options);
    const double seconds = elapsedSeconds(start);

    // A smooth closed sphere carries no hard features; consuming the result
    // also prevents the call from being optimized away.
    EXPECT_EQ(0, analysis.dihedralFeatureEdges);
    EXPECT_LT(seconds, 2.0) << "detectFeatureCurves took " << seconds << " s on ~16k faces";
}

TEST(PipelinePerfGuard, SimplifyOnSixteenThousandFaceSphereStaysBounded) {
    const analytic::SphereFixture sphere = analytic::makeUvSphere(64, 128, 1.0);

    simplification::SimplifyOptions options;
    options.targetRatio = 0.2;

    const auto start = std::chrono::steady_clock::now();
    simplification::QEMSimplifier simplifier(options);
    const manumesh::Mesh output = simplifier.simplify(sphere.mesh);
    const double seconds = elapsedSeconds(start);

    EXPECT_FALSE(output.empty());
    EXPECT_LT(seconds, 8.0) << "simplify took " << seconds << " s on ~16k faces";
}
