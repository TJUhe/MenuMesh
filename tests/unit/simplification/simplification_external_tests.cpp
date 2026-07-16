/**
 * @file tests/unit/simplification/simplification_external_tests.cpp
 * @brief Verifies simplification external tests behavior in the ManuMesh tests.
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
TEST(ManuMesh, ExternalFinishedFlangeFixtureLoadsWithFeatures) {
    const manumesh::Mesh mesh = loadExternalStl("openfoam_flange.stl");
    ASSERT_FALSE(mesh.empty());
    EXPECT_GT(mesh.faces.size(), 1000u);

    manumesh::feature::FeatureOptions options = manumesh::test::circularFeatureOptions(0.08);
    options.featureAngleDeg = 25.0;
    const manumesh::feature::FeatureAnalysis analysis = manumesh::feature::detectFeatureCurves(mesh, options);
    EXPECT_GT(analysis.featureEdges, 0);
    EXPECT_GT(analysis.graph.edges.size(), 0u);
    EXPECT_GT(analysis.loops.size(), 0u);
}

TEST(ManuMesh, ExternalNasaIndustrialMeshesExposeRichFeatureTopology) {
    struct Case {
        std::string fileName;
        int minFaces = 0;
        int minFeatureEdges = 0;
        int minCircularLoops = 0;
    };

    const std::array<Case, 3> cases = {{
        {"nasa_antenna_azimuth_track.stl", 3000, 1000, 4},
        {"nasa_cubesat_middle.stl", 28000, 8000, 20},
        {"nasa_mars2020_wheel.stl", 45000, 10000, 4},
    }};

    for (const Case& testCase : cases) {
        SCOPED_TRACE(testCase.fileName);
        const manumesh::Mesh mesh = loadExternalStl(testCase.fileName);
        ASSERT_FALSE(mesh.empty());

        const manumesh::simplification::MeshStats stats = manumesh::simplification::computeMeshStats(mesh);
        EXPECT_GE(stats.faces, testCase.minFaces);
        EXPECT_GT(stats.edges, 0);
        EXPECT_GT(stats.area, 0.0);
        EXPECT_EQ(stats.nonManifoldEdges, 0);

        manumesh::feature::FeatureOptions featureOptions = circularFeatureOptions();
        featureOptions.featureAngleDeg = 30.0;
        featureOptions.circleFitRelativeThreshold = 0.05;
        const manumesh::feature::FeatureAnalysis features =
            manumesh::feature::detectFeatureCurves(mesh, featureOptions);
        EXPECT_GE(features.featureEdges, testCase.minFeatureEdges);
        EXPECT_GE(countCircularLoops(features), testCase.minCircularLoops);
        EXPECT_GT(features.dihedralFeatureEdges, 0);
    }
}

TEST(ManuMesh, ExternalDownloadedMeshesCompareIndustrialSimplificationModes) {
    struct Case {
        std::string fileName;
        bool expectsCircularProjection = false;
    };

    const std::array<Case, 3> cases = {{
        {"nasa_antenna_azimuth_track.stl", true},
        {"thingi10k/thingi10k_108336_projekt_muse_z_system.stl", false},
        {"thingi10k/thingi10k_318045_moko_mini_pulley.stl", true},
    }};

    constexpr double ratio = 0.45;
    for (const Case& testCase : cases) {
        SCOPED_TRACE(testCase.fileName);
        const manumesh::Mesh input = loadExternalStl(testCase.fileName);
        ASSERT_FALSE(input.empty());

        const SimplifiedMesh standard = simplifyWithReport(input, standardQemOptions(ratio));
        const SimplifiedMesh line = simplifyWithReport(input, paperLineQuadricsOptions(ratio));
        const SimplifiedMesh protectedResult = simplifyWithReport(input, protectedIndustrialFeatureOptions(ratio));

        expectBudgetedSimplification(standard, input, ratio);
        expectBudgetedSimplification(line, input, ratio);
        EXPECT_FALSE(protectedResult.mesh.empty());
        EXPECT_LT(protectedResult.report.finalFaces, protectedResult.report.initialFaces);
        EXPECT_EQ(protectedResult.report.finalFaces, static_cast<int>(protectedResult.mesh.faces.size()));

        EXPECT_EQ(0.0, standard.report.maxAppliedLineWeight);
        EXPECT_GE(line.report.maxAppliedLineWeight, line.report.minAppliedLineWeight);
        EXPECT_GT(protectedResult.report.featureLoops, 0);
        EXPECT_GT(protectedResult.report.featureVertices, 0);
        EXPECT_GT(protectedResult.report.featureRejectedCollapses, 0);
        EXPECT_GE(protectedResult.report.maxAppliedLineWeight, protectedResult.report.minAppliedLineWeight);

        if (testCase.expectsCircularProjection) {
            EXPECT_GT(protectedResult.report.circularFeatureLoops, 0);
            EXPECT_GT(protectedResult.report.projectedFeaturePlacements, 0);
        }
    }
}

TEST(ManuMesh, Public2014CastingModelKeepsClosedTopologyAfterLineSimplify) {
    const manumesh::Mesh input = loadExternalMesh("casting_aimshape_2014.stl");
    ASSERT_FALSE(input.empty());

    const manumesh::simplification::MeshStats inputStats = manumesh::simplification::computeMeshStats(input);
    ASSERT_EQ(inputStats.boundaryEdges, 0);
    ASSERT_EQ(inputStats.nonManifoldEdges, 0);

    manumesh::simplification::SimplifyOptions options = paperLineQuadricsOptions(0.25);
    manumesh::simplification::SimplifyReport report;
    manumesh::simplification::QEMSimplifier simplifier(options);
    const manumesh::Mesh output = simplifier.simplify(input, &report);
    const manumesh::simplification::MeshStats outputStats = manumesh::simplification::computeMeshStats(output);

    EXPECT_FALSE(output.empty());
    EXPECT_LE(report.finalFaces, static_cast<int>(std::llround(input.faces.size() * 0.25)) + 2);
    EXPECT_EQ(outputStats.boundaryEdges, 0);
    EXPECT_EQ(outputStats.nonManifoldEdges, 0);
}

TEST(ManuMesh, ExternalBinaryStlLoadKeepsGeometryUsable) {
    const manumesh::Mesh loaded = loadExternalStl("thingi10k/thingi10k_108336_projekt_muse_z_system.stl");
    ASSERT_FALSE(loaded.empty());

    const manumesh::simplification::MeshStats stats = manumesh::simplification::computeMeshStats(loaded);
    EXPECT_EQ(stats.faces, static_cast<int>(loaded.faces.size()));
    EXPECT_GT(stats.vertices, 0);
    EXPECT_GT(stats.edges, 0);
    EXPECT_GT(stats.area, 0.0);
    EXPECT_EQ(stats.nonManifoldEdges, 0);
}

TEST(ManuMesh, MeshDistanceIsZeroForIdenticalMeshAndFiniteAfterSimplify) {
    const manumesh::Mesh input = loadExternalStl("thingi10k/thingi10k_108336_projekt_muse_z_system.stl");
    ASSERT_FALSE(input.empty());

    const manumesh::simplification::DistanceStats identical =
        manumesh::simplification::compareMeshesBySampledDistance(input, input, 32);
    EXPECT_NEAR(identical.meanOriginalToSimplified, 0.0, 1e-12);
    EXPECT_NEAR(identical.maxOriginalToSimplified, 0.0, 1e-12);
    EXPECT_NEAR(identical.meanSimplifiedToOriginal, 0.0, 1e-12);
    EXPECT_NEAR(identical.maxSimplifiedToOriginal, 0.0, 1e-12);

    const SimplifiedMesh simplified = simplifyWithReport(input, paperLineQuadricsOptions(0.35));
    const manumesh::simplification::DistanceStats distance =
        manumesh::simplification::compareMeshesBySampledDistance(input, simplified.mesh, 32);
    EXPECT_GE(distance.meanOriginalToSimplified, 0.0);
    EXPECT_GE(distance.maxOriginalToSimplified, distance.meanOriginalToSimplified);
    EXPECT_GE(distance.meanSimplifiedToOriginal, 0.0);
    EXPECT_GE(distance.maxSimplifiedToOriginal, distance.meanSimplifiedToOriginal);
}
