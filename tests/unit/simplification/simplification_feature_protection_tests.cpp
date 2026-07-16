/**
 * @file tests/unit/simplification/simplification_feature_protection_tests.cpp
 * @brief Verifies simplification feature protection tests behavior in the ManuMesh tests.
 * @ingroup manumesh_tests
 *
 * @details The fixture and assertions document observable contracts, numeric tolerances, determinism requirements, and previously fixed regressions.
 */

#include "SimplificationTestSupport.h"
#include "algorithms/feature_detection/FeatureDetector.h"
#include "algorithms/simplification/Metrics.h"
#include "algorithms/simplification/PlainSimplifier.h"
#include "algorithms/simplification/QEMSimplifier.h"
#include "core/MeshGenerators.h"
#include "core/MeshTopology.h"
#include "core/PlainMesh.h"
#include "simplification/detail/SimplificationPolicies.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

using manumesh::test::countCircularLoops;
using manumesh::test::loadExternalMesh;
using manumesh::test::loadExternalStl;
using manumesh::test::SimplifiedMesh;
using manumesh::test::simplifyWithReport;
using namespace manumesh::test::simplification;

namespace simplification = manumesh::simplification;

TEST(ManuMesh, SimplificationMapsSmoothCurvatureAndWeakSpurOptions) {
    simplification::SimplifyOptions options;
    options.useSmoothCurvatureFeatures = true;
    options.smoothCurvatureFeatureThreshold = 0.021;
    options.smoothCurvatureMinEdgeAlignment = 0.61;
    options.smoothCurvatureMinTangentConsistency = 0.73;
    options.smoothCurvatureBaseNeighborhoodRings = 3;
    options.smoothCurvatureScaleCount = 4;
    options.smoothCurvatureMinPersistentScales = 3;
    options.smoothCurvatureRobustFitIterations = 1;
    options.featureGraphMinWeakSpurStrength = 0.42;

    const manumesh::feature::FeatureOptions mapped = simplification::featureOptionsFromSimplifyOptions(options);

    EXPECT_TRUE(mapped.useSmoothCurvatureFeatures);
    EXPECT_DOUBLE_EQ(options.smoothCurvatureFeatureThreshold, mapped.smoothCurvatureFeatureThreshold);
    EXPECT_DOUBLE_EQ(options.smoothCurvatureMinEdgeAlignment, mapped.smoothCurvatureMinEdgeAlignment);
    EXPECT_DOUBLE_EQ(options.smoothCurvatureMinTangentConsistency, mapped.smoothCurvatureMinTangentConsistency);
    EXPECT_EQ(options.smoothCurvatureBaseNeighborhoodRings, mapped.smoothCurvatureBaseNeighborhoodRings);
    EXPECT_EQ(options.smoothCurvatureScaleCount, mapped.smoothCurvatureScaleCount);
    EXPECT_EQ(options.smoothCurvatureMinPersistentScales, mapped.smoothCurvatureMinPersistentScales);
    EXPECT_EQ(options.smoothCurvatureRobustFitIterations, mapped.smoothCurvatureRobustFitIterations);
    EXPECT_DOUBLE_EQ(options.featureGraphMinWeakSpurStrength, mapped.featureGraphMinWeakSpurStrength);
}

TEST(ManuMesh, SimplificationDetectsSmoothCurvatureFeaturesThroughPrimaryEntryPoint) {
    const manumesh::Mesh input = manumesh::generateBumpGrid(24, 2.0);
    simplification::SimplifyOptions options = standardQemOptions(0.50);
    options.preserveFeatureCurves = true;
    options.featureProtectionMode = simplification::FeatureProtectionMode::AllFeatureEdges;
    options.useNormalTensorFeatures = false;
    options.featureAngleDeg = 180.0;
    options.loopTraceAngleDeg = 180.0;
    options.useSmoothCurvatureFeatures = true;
    options.smoothCurvatureFeatureThreshold = 0.008;
    options.smoothCurvatureMinEdgeAlignment = 0.45;
    options.smoothCurvatureMinTangentConsistency = 0.55;
    options.smoothCurvatureBaseNeighborhoodRings = 2;
    options.smoothCurvatureScaleCount = 3;
    options.smoothCurvatureMinPersistentScales = 2;
    options.smoothCurvatureRobustFitIterations = 2;

    const SimplifiedMesh result = simplifyWithReport(input, options);

    expectBudgetedSimplification(result, input, options.targetRatio);
    EXPECT_GT(result.report.smoothCurvatureScoredVertices, 0);
    EXPECT_GT(result.report.smoothCurvatureFeatureEdges, 0);
    EXPECT_GT(result.report.maxSmoothCurvaturePersistentScore, options.smoothCurvatureFeatureThreshold);
    EXPECT_GT(result.report.meanSmoothCurvatureLocalScale, 0.0);
    EXPECT_GT(result.report.meanSmoothCurvaturePersistence, 0.0);
    EXPECT_GT(result.report.featureVertices, countBoundaryVertices(input));
    EXPECT_GT(result.report.featureRejectedCollapses, 0);
}

TEST(ManuMesh, StrictPolygonalFeatureProtectionRejectsChordPlacement) {
    const manumesh::Mesh input = makePolygonalFeatureChordMesh();

    manumesh::simplification::SimplifyOptions options = standardQemOptions(0.25);
    options.targetFaces = 1;
    options.preserveFeatureCurves = true;
    options.featureProtectionMode = manumesh::simplification::FeatureProtectionMode::AllFeatureEdges;
    options.useNormalTensorFeatures = false;
    options.featureAngleDeg = 179.0;
    options.circleFitRelativeThreshold = 0.0;
    options.ellipseFitRelativeThreshold = 0.0;
    options.minFeatureLoopVertices = 3;
    options.maxFeatureCurveDeviationRatio = 1e-9;
    options.maxNormalDeviationDeg = 180.0;
    options.minTriangleQuality = 0.0;
    const SimplifiedMesh result = simplifyWithReport(input, options);

    EXPECT_FALSE(result.mesh.empty());
    EXPECT_GT(result.report.featureLoops, 0);
    EXPECT_GT(result.report.featureRejectedCollapses, 0);
    EXPECT_GT(result.report.genericFeatureRejectedCollapses, 0);
    EXPECT_EQ(manumesh::simplification::SimplifyTerminationReason::RejectionLimit, result.report.terminationReason);
    EXPECT_EQ(
        result.report.rejectedCollapses,
        result.report.topologyRejectedCollapses + result.report.normalFlipRejectedCollapses +
            result.report.qualityRejectedCollapses + result.report.boundaryRejectedCollapses +
            result.report.selfIntersectionRejectedCollapses + result.report.curveBudgetRejectedCollapses +
            result.report.errorRejectedCollapses + result.report.featureRejectedCollapses
    );
}

TEST(ManuMesh, PrimitiveModeKeepsPolygonalFeaturesSoftByDefault) {
    const manumesh::Mesh input = manumesh::generateCubeGrid(4, 1.0);

    manumesh::simplification::SimplifyOptions options = standardQemOptions(0.35);
    options.preserveFeatureCurves = true;
    options.featureProtectionMode = manumesh::simplification::FeatureProtectionMode::PrimitiveCurves;
    options.useNormalTensorFeatures = false;
    options.featureAngleDeg = 25.0;
    options.minFeatureLoopVertices = 4;
    options.maxNormalDeviationDeg = 180.0;
    const SimplifiedMesh result = simplifyWithReport(input, options);

    EXPECT_FALSE(result.mesh.empty());
    EXPECT_GT(result.report.featureLoops, 0);
    EXPECT_GT(result.report.featureVertices, 0);
    EXPECT_EQ(0, result.report.genericFeatureRejectedCollapses);
}

TEST(ManuMesh, ReportsFeatureLoopsOnCylinderCreases) {
    const manumesh::Mesh input = manumesh::generateCylinderGrid(32, 4, 1.0, 2.0);
    manumesh::feature::FeatureOptions options;
    options.featureAngleDeg = 30.0;

    const manumesh::feature::FeatureAnalysis features = manumesh::feature::detectFeatureCurves(input, options);

    // The only dihedral angles above 30 degrees are at the two cap rims,
    // where wall and cap faces meet at 90 degrees: 32 radial segments per rim
    // gives exactly 2 * 32 = 64 dihedral feature edges. Wall edges bend by at
    // most 360/32 = 11.25 degrees and cap-fan edges are coplanar, so neither
    // contributes; the closed cylinder also has no boundary edges.
    EXPECT_EQ(64, features.featureEdges);
    EXPECT_EQ(64, features.dihedralFeatureEdges);
    EXPECT_EQ(0, features.boundaryFeatureEdges);
    // Each rim traces into one closed circular loop of the 32 rim vertices at
    // the exact analytic rim circle (radius 1.0, z = +/- height/2 = +/- 1.0).
    ASSERT_EQ(2u, features.loops.size());
    for (const manumesh::feature::FeatureLoop& loop : features.loops) {
        EXPECT_TRUE(loop.closed);
        EXPECT_TRUE(loop.circular);
        EXPECT_EQ(32u, loop.vertices.size());
        EXPECT_NEAR(1.0, loop.radius, 1e-9);
        EXPECT_NEAR(1.0, std::abs(loop.center.z()), 1e-9);
    }
}

TEST(ManuMesh, MeasuresCircularFeatureLoopAgainstDetectedCircle) {
    const manumesh::Mesh input = manumesh::generateCylinderGrid(32, 4, 1.0, 2.0);
    const manumesh::feature::FeatureAnalysis features =
        manumesh::feature::detectFeatureCurves(input, circularFeatureOptions());
    ASSERT_GT(countCircularLoops(features), 0);

    const auto loopIt =
        std::find_if(features.loops.begin(), features.loops.end(), [](const manumesh::feature::FeatureLoop& loop) {
            return loop.circular;
        });
    ASSERT_NE(loopIt, features.loops.end());

    const manumesh::feature::DirectionalCurveError error =
        manumesh::feature::measureLoopAgainstCircle(input, *loopIt, loopIt->center, loopIt->normal, loopIt->radius);

    EXPECT_EQ(error.samples, static_cast<int>(loopIt->vertices.size()));
    EXPECT_NEAR(error.radialRms, loopIt->rmsRadialError, 1e-10);
    EXPECT_NEAR(error.planeRms, loopIt->rmsPlaneError, 1e-10);
    EXPECT_LT(error.radialMax, 1e-10);
    EXPECT_LT(error.planeMax, 1e-10);
}
