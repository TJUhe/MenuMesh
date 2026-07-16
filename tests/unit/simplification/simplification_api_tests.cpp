/**
 * @file tests/unit/simplification/simplification_api_tests.cpp
 * @brief Verifies simplification api tests behavior in the ManuMesh tests.
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
TEST(ManuMesh, WeightModesRoundTripAndRejectUnknownValues) {
    EXPECT_EQ(manumesh::simplification::WeightMode::Uniform, manumesh::simplification::parseWeightMode("uniform"));
    EXPECT_EQ(manumesh::simplification::WeightMode::Dihedral, manumesh::simplification::parseWeightMode("dihedral"));
    EXPECT_EQ(
        manumesh::simplification::WeightMode::NormalTensor, manumesh::simplification::parseWeightMode("normal-tensor")
    );
    EXPECT_EQ(manumesh::simplification::WeightMode::Height, manumesh::simplification::parseWeightMode("height"));
    EXPECT_EQ(manumesh::simplification::WeightMode::XBand, manumesh::simplification::parseWeightMode("xband"));

    EXPECT_EQ("uniform", manumesh::simplification::toString(manumesh::simplification::WeightMode::Uniform));
    EXPECT_EQ("dihedral", manumesh::simplification::toString(manumesh::simplification::WeightMode::Dihedral));
    EXPECT_EQ("normal-tensor", manumesh::simplification::toString(manumesh::simplification::WeightMode::NormalTensor));
    EXPECT_EQ("height", manumesh::simplification::toString(manumesh::simplification::WeightMode::Height));
    EXPECT_EQ("xband", manumesh::simplification::toString(manumesh::simplification::WeightMode::XBand));

    EXPECT_THROW(manumesh::simplification::parseWeightMode("paper"), std::invalid_argument);
}

TEST(ManuMesh, SimplificationNamespaceApiIsProjectScoped) {
    static_assert(std::is_same_v<manumesh::simplification::SimplifyOptions, simplification::SimplifyOptions>);
    static_assert(std::is_same_v<manumesh::simplification::SimplifyReport, simplification::SimplifyReport>);
    static_assert(std::is_same_v<manumesh::simplification::QEMSimplifier, simplification::QEMSimplifier>);

    const manumesh::Mesh input = manumesh::generatePlaneGrid(4, 1.0, false);
    simplification::SimplifyOptions options = standardQemOptions(0.5);
    options.targetFaces = 8;

    simplification::SimplifyReport report;
    const manumesh::Mesh output = simplification::simplifyMesh(input, options, &report);
    EXPECT_LE(output.faces.size(), static_cast<std::size_t>(options.targetFaces));
    EXPECT_EQ(output.faces.size(), static_cast<std::size_t>(report.finalFaces));
    EXPECT_EQ("uniform", simplification::toString(simplification::WeightMode::Uniform));

    const manumesh::PlainMesh plainOutput =
        simplification::simplifyPlainMesh(manumesh::toPlainMesh(input), options, nullptr);
    EXPECT_EQ(output.faces.size(), plainOutput.faces.size());
}

TEST(ManuMesh, LegacyMetricsApiForwardsToAnalysisDuringMigration) {
    const manumesh::Mesh mesh = manumesh::generatePlaneGrid(2, 1.0, false);

    const simplification::MeshStats legacy = simplification::computeMeshStats(mesh);
    const manumesh::analysis::MeshStats current = manumesh::analysis::computeMeshStats(mesh);
    EXPECT_EQ(current.vertices, legacy.vertices);
    EXPECT_EQ(current.faces, legacy.faces);
    EXPECT_EQ(current.edges, legacy.edges);
    EXPECT_DOUBLE_EQ(current.area, legacy.area);

    const simplification::DistanceStats distance = simplification::compareMeshesBySampledDistance(mesh, mesh, 16);
    EXPECT_DOUBLE_EQ(0.0, distance.maxOriginalToSimplified);
    EXPECT_EQ(
        "label,vertices,faces,edges,boundary_edges,non_manifold_edges,area,"
        "mean_triangle_quality,min_triangle_quality,mean_edge_length,"
        "edge_length_cv,mean_orig_to_simp,max_orig_to_simp,"
        "mean_simp_to_orig,max_simp_to_orig",
        simplification::statsHeaderCsv()
    );
    const std::string row = simplification::statsRowCsv("plane", legacy, &distance);
    EXPECT_EQ(15u, static_cast<std::size_t>(std::count(row.begin(), row.end(), ',') + 1));
}

TEST(ManuMesh, PlainMeshRoundTripsWithoutEigenInExchangeType) {
    manumesh::PlainMesh plain;
    plain.vertices = {
        {0.0, 0.0, 0.0},
        {1.0, 0.0, 0.0},
        {0.0, 1.0, 0.0},
    };
    plain.faces = {{{{0, 1, 2}}}};

    const manumesh::Mesh mesh = manumesh::toMesh(plain);
    ASSERT_EQ(3u, mesh.vertices.size());
    ASSERT_EQ(1u, mesh.faces.size());

    const manumesh::PlainMesh roundTrip = manumesh::toPlainMesh(mesh);
    EXPECT_EQ(plain.vertices.size(), roundTrip.vertices.size());
    EXPECT_EQ(plain.faces.size(), roundTrip.faces.size());
    EXPECT_DOUBLE_EQ(1.0, roundTrip.vertices[1].x);
    EXPECT_EQ(2, roundTrip.faces[0].v[2]);
}

TEST(ManuMesh, SimplifiesPlainMeshThroughEigenFreeEntryPoint) {
    const manumesh::PlainMesh input = manumesh::toPlainMesh(manumesh::generatePlaneGrid(8, 1.0, false));

    manumesh::simplification::SimplifyOptions options = standardQemOptions(0.50);
    options.maxNormalDeviationDeg = 180.0;

    manumesh::simplification::SimplifyReport report;
    const manumesh::PlainMesh output = manumesh::simplification::simplifyPlainMesh(input, options, &report);

    EXPECT_FALSE(output.faces.empty());
    EXPECT_EQ(report.initialFaces, static_cast<int>(input.faces.size()));
    EXPECT_EQ(report.finalFaces, static_cast<int>(output.faces.size()));
    EXPECT_LT(report.finalFaces, report.initialFaces);
    EXPECT_EQ(manumesh::simplification::SimplifyTerminationReason::ReachedTarget, report.terminationReason);
}

TEST(ManuMesh, QEMSimplifierObjectStoresOptionsAndLatestReport) {
    const manumesh::Mesh input = manumesh::generateCylinderGrid(24, 6, 1.0, 2.0);

    manumesh::simplification::SimplifyOptions options = paperLineQuadricsOptions(0.50);
    manumesh::simplification::QEMSimplifier simplifier(options);

    manumesh::simplification::SimplifyReport copiedReport;
    const manumesh::Mesh output = simplifier.simplify(input, &copiedReport);

    EXPECT_FALSE(output.empty());
    EXPECT_EQ(options.targetRatio, simplifier.options().targetRatio);
    EXPECT_EQ(copiedReport.finalFaces, simplifier.report().finalFaces);
    EXPECT_EQ(copiedReport.collapsedEdges, simplifier.report().collapsedEdges);
    EXPECT_LT(simplifier.report().finalFaces, simplifier.report().initialFaces);
    EXPECT_EQ(
        manumesh::simplification::SimplifyTerminationReason::ReachedTarget, simplifier.report().terminationReason
    );
}

TEST(ManuMesh, QEMSimplifierConsumesPrecomputedFeatureAnalysis) {
    const manumesh::Mesh input = manumesh::generateCylinderGrid(24, 6, 1.0, 2.0);
    const manumesh::feature::FeatureAnalysis features =
        manumesh::feature::detectFeatureCurves(input, circularFeatureOptions());
    ASSERT_FALSE(features.loops.empty());

    manumesh::simplification::SimplifyOptions options = protectedIndustrialFeatureOptions(0.75);
    manumesh::simplification::QEMSimplifier simplifier(options);

    manumesh::simplification::SimplifyReport objectReport;
    const manumesh::Mesh objectOutput = simplifier.simplify(input, features, &objectReport);

    manumesh::simplification::SimplifyReport freeReport;
    const manumesh::Mesh freeOutput = manumesh::simplification::simplifyMesh(input, options, features, &freeReport);

    EXPECT_FALSE(objectOutput.empty());
    EXPECT_FALSE(freeOutput.empty());
    EXPECT_EQ(static_cast<int>(features.loops.size()), objectReport.featureLoops);
    EXPECT_EQ(objectReport.featureLoops, simplifier.report().featureLoops);
    EXPECT_EQ(objectReport.featureLoops, freeReport.featureLoops);
    EXPECT_EQ(objectReport.finalFaces, freeReport.finalFaces);
}

TEST(ManuMesh, QEMSimplifierCopiesPimplStateIndependently) {
    manumesh::simplification::SimplifyOptions options = paperLineQuadricsOptions(0.60);
    manumesh::simplification::QEMSimplifier original(options);

    const manumesh::Mesh input = manumesh::generatePlaneGrid(8, 1.0, false);
    const manumesh::Mesh output = original.simplify(input);
    ASSERT_FALSE(output.empty());

    manumesh::simplification::QEMSimplifier copied = original;
    EXPECT_EQ(original.options().targetRatio, copied.options().targetRatio);
    EXPECT_EQ(original.report().finalFaces, copied.report().finalFaces);

    manumesh::simplification::QEMSimplifier moved = std::move(copied);
    EXPECT_EQ(original.report().finalFaces, moved.report().finalFaces);

    manumesh::simplification::SimplifyOptions movedFromOptions;
    movedFromOptions.targetRatio = 0.75;
    copied.setOptions(movedFromOptions);
    EXPECT_DOUBLE_EQ(0.75, copied.options().targetRatio);

    manumesh::simplification::SimplifyOptions copiedOptions = copied.options();
    copiedOptions.targetRatio = 0.25;
    copied.setOptions(copiedOptions);

    EXPECT_DOUBLE_EQ(0.60, original.options().targetRatio);
    EXPECT_DOUBLE_EQ(0.25, copied.options().targetRatio);
}
