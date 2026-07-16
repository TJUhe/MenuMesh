/**
 * @file tests/unit/simplification/qem_parameter_feature_tests.cpp
 * @brief Verifies qem parameter feature tests behavior in the ManuMesh tests.
 * @ingroup manumesh_tests
 *
 * @details The fixture and assertions document observable contracts, numeric tolerances, determinism requirements, and previously fixed regressions.
 */

#include "QemParameterTestSupport.h"
#include "TestSupport.h"
#include "algorithms/feature_detection/FeatureDetector.h"
#include "algorithms/simplification/Metrics.h"

#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <vector>

using manumesh::test::CaseLine;
using manumesh::test::circularFeatureOptions;
using manumesh::test::countCircularLoops;
using manumesh::test::expectBudget;
using manumesh::test::lineOptions;
using manumesh::test::loadCaseMesh;
using manumesh::test::maxCircularRelativeError;
using manumesh::test::protectedOptions;
using manumesh::test::readCaseLines;
using manumesh::test::SimplifiedMesh;
using manumesh::test::simplifyWithReport;
using manumesh::test::standardOptions;
using manumesh::test::qem_parameters::innerEllipseLoops;
TEST(ManuMeshParameters, FeatureProtectedCircularLoopsRemainDetectableAfterAggressiveSimplify) {
    const manumesh::Mesh input = loadCaseMesh("feature_fixtures/coaxial_hole_plate.obj");
    ASSERT_FALSE(input.empty());

    manumesh::simplification::SimplifyOptions options = protectedOptions(0.25);
    options.circleFitRelativeThreshold = 0.04;
    options.minFeatureLoopVertices = 12;
    const SimplifiedMesh result = simplifyWithReport(input, options);

    expectBudget(result, input, 0.25);
    // The fixture carries exactly four circular rims (outer plate rims at
    // r=2.0 and hole rims at r=0.6, at z=+-0.5); all of them must be picked up
    // as protected circular loops.
    EXPECT_EQ(4, result.report.circularFeatureLoops);
    EXPECT_GT(result.report.projectedFeaturePlacements, 0);

    // Hard-protection invariant: the 0.25 target drives every rim down to the
    // minCircularFeatureLoopVertices floor, but each surviving rim vertex must
    // still lie exactly on its original circle (collapse placements on a
    // circular loop are projected onto the fitted circle).
    const struct {
        double radius;
        double z;
    } rims[] = {{2.0, 0.5}, {2.0, -0.5}, {0.6, 0.5}, {0.6, -0.5}};
    constexpr double kOnCircleTolerance = 1e-6;
    for (const auto& rim : rims) {
        int verticesOnRim = 0;
        for (const manumesh::Vec3& p : result.mesh.vertices) {
            const double radialError = std::abs(std::hypot(p.x(), p.y()) - rim.radius);
            if (std::abs(p.z() - rim.z) <= kOnCircleTolerance && radialError <= kOnCircleTolerance) {
                ++verticesOnRim;
            }
        }
        EXPECT_GE(verticesOnRim, options.minCircularFeatureLoopVertices)
            << "rim r=" << rim.radius << " z=" << rim.z << " lost its protected vertices";
    }

    manumesh::feature::FeatureOptions outputFeatureOptions = circularFeatureOptions();
    outputFeatureOptions.circleFitRelativeThreshold = 0.16;
    outputFeatureOptions.minFeatureLoopVertices = options.minCircularFeatureLoopVertices;
    const manumesh::feature::FeatureAnalysis outputFeatures =
        manumesh::feature::detectFeatureCurves(result.mesh, outputFeatureOptions);

    // Re-detection on the floor-degenerate output is only guaranteed for the
    // hole rims. At the minCircularFeatureLoopVertices floor the top and
    // bottom rims of each cylinder keep independent rotational phases, so the
    // outer (r=2.0) wall becomes a zigzag of skewed triangles whose crease
    // dihedrals rival the rim crease; whether graph recovery closes the outer
    // rims then depends on the tessellation phase left behind by the
    // (deterministic but order-sensitive) collapse sequence, not on the
    // protection itself. The historical >=3 expectation was met only because
    // one spurious mixed loop sat 3% below the 0.16 threshold while the true
    // bottom outer rim already went undetected. The steep inner wall keeps the
    // r=0.6 hole rims robustly detectable, and the geometric checks above
    // pin down the protection guarantee for all four rims.
    EXPECT_GE(countCircularLoops(outputFeatures), 2);
    int detectableHoleRims = 0;
    for (const manumesh::feature::FeatureLoop& loop : outputFeatures.loops) {
        if (loop.circular && std::abs(loop.radius - 0.6) <= 0.05) {
            ++detectableHoleRims;
            EXPECT_LT(loop.rmsRadialError, 1e-9);
            EXPECT_LT(loop.rmsPlaneError, 1e-9);
        }
    }
    EXPECT_GE(detectableHoleRims, 2);
}

TEST(ManuMeshParameters, EllipsePrimitiveUsesPrimitiveFeatureProjection) {
    const manumesh::Mesh input = loadCaseMesh("feature_fixtures/elliptical_hole_plate.obj");
    ASSERT_FALSE(input.empty());

    manumesh::feature::FeatureOptions featureOptions = circularFeatureOptions();
    featureOptions.ellipseFitRelativeThreshold = 0.03;
    const manumesh::feature::FeatureAnalysis features = manumesh::feature::detectFeatureCurves(input, featureOptions);
    ASSERT_GE(innerEllipseLoops(features).size(), 2u);

    manumesh::simplification::SimplifyOptions options = protectedOptions(0.85);
    options.ellipseFitRelativeThreshold = 0.03;
    options.minFeatureLoopVertices = 8;
    const SimplifiedMesh result = simplifyWithReport(input, options);

    expectBudget(result, input, 0.85);
    EXPECT_GT(result.report.featureLoops, 0);
    EXPECT_GT(result.report.projectedFeaturePlacements, 0);
    EXPECT_GT(result.report.featureRejectedCollapses, 0);

    manumesh::feature::FeatureOptions outputFeatureOptions = circularFeatureOptions();
    outputFeatureOptions.ellipseFitRelativeThreshold = 0.12;
    outputFeatureOptions.minFeatureLoopVertices = 6;
    const manumesh::feature::FeatureAnalysis outputFeatures =
        manumesh::feature::detectFeatureCurves(result.mesh, outputFeatureOptions);
    const std::vector<manumesh::feature::FeatureLoop> outputEllipses = innerEllipseLoops(outputFeatures);

    ASSERT_GE(outputEllipses.size(), 2u);
    for (const manumesh::feature::FeatureLoop& loop : outputEllipses) {
        EXPECT_NEAR(loop.axisRatio, 0.45 / 0.8, 0.10);
        EXPECT_LT(loop.rmsEllipseError, 0.08);
        EXPECT_LT(loop.rmsPlaneError, 0.05);
    }
}

TEST(ManuMeshParameters, EllipsePrimitiveIsProtectedByPrimitiveMode) {
    const manumesh::Mesh input = loadCaseMesh("feature_fixtures/elliptical_hole_plate.obj");
    ASSERT_FALSE(input.empty());

    manumesh::simplification::SimplifyOptions options = protectedOptions(0.85);
    options.ellipseFitRelativeThreshold = 0.03;
    const SimplifiedMesh result = simplifyWithReport(input, options);

    expectBudget(result, input, 0.85);
    EXPECT_GT(result.report.featureRejectedCollapses, 0);
    EXPECT_GT(result.report.projectedFeaturePlacements, 0);

    manumesh::feature::FeatureOptions outputFeatureOptions = circularFeatureOptions();
    outputFeatureOptions.ellipseFitRelativeThreshold = 0.12;
    outputFeatureOptions.minFeatureLoopVertices = 6;
    const manumesh::feature::FeatureAnalysis outputFeatures =
        manumesh::feature::detectFeatureCurves(result.mesh, outputFeatureOptions);

    EXPECT_GE(innerEllipseLoops(outputFeatures).size(), 2u);
}
