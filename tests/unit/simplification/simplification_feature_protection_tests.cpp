#include "SimplificationTestSupport.h"
#include "algorithms/feature_detection/FeatureDetector.h"
#include "algorithms/simplification/Metrics.h"
#include "algorithms/simplification/PlainSimplifier.h"
#include "algorithms/simplification/QEMSimplifier.h"
#include "core/MeshGenerators.h"
#include "core/MeshTopology.h"
#include "core/PlainMesh.h"

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

    EXPECT_GT(features.featureEdges, 0);
    EXPECT_GT(features.dihedralFeatureEdges, 0);
    EXPECT_FALSE(features.loops.empty());
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
