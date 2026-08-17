/**
 * @file tests/unit/simplification/simplification_api_tests.cpp
 * @brief 验证简化公共 API、分组配置、紧凑摘要和兼容适配。
 * @ingroup manumesh_tests
 */

#include "SimplificationTestSupport.h"
#include "algorithms/feature_detection/FeatureDetector.h"
#include "algorithms/simplification/Metrics.h"
#include "algorithms/simplification/PlainSimplifier.h"
#include "algorithms/simplification/QEMSimplifier.h"
#include "core/MeshGenerators.h"
#include "core/MeshTopology.h"
#include "core/PlainMesh.h"

#include "core/Filesystem.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <gtest/gtest.h>
#include <locale>
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

namespace {

class CommaDecimalPunct : public std::numpunct<char> {
protected:
    char do_decimal_point() const override { return ','; }
};

class GlobalLocaleRestore {
public:
    GlobalLocaleRestore()
        : previous_(std::locale()) {}
    ~GlobalLocaleRestore() { std::locale::global(previous_); }

private:
    std::locale previous_;
};

} // namespace

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
    static_assert(
        std::is_same<manumesh::simplification::SimplifyOptions, simplification::SimplifyOptions>::value,
        "SimplifyOptions namespace alias changed"
    );
    static_assert(
        std::is_same<manumesh::simplification::SimplifyReport, simplification::SimplifyReport>::value,
        "SimplifyReport namespace alias changed"
    );
    static_assert(
        std::is_same<manumesh::simplification::QEMSimplifier, simplification::QEMSimplifier>::value,
        "QEMSimplifier namespace alias changed"
    );

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

TEST(ManuMesh, GroupedSimplifyConfigDefaultsMatchLegacyFeatureDefaults) {
    const simplification::SimplifyOptions legacy{};
    const simplification::SimplifyOptions grouped =
        simplification::makeSimplifyOptions(simplification::SimplifyConfig{});

    EXPECT_EQ(legacy.targetFaces, grouped.targetFaces);
    EXPECT_DOUBLE_EQ(legacy.targetRatio, grouped.targetRatio);
    ASSERT_TRUE(grouped.featureOptionsOverride.has_value());
    EXPECT_DOUBLE_EQ(legacy.featureAngleDeg, grouped.featureOptionsOverride->featureAngleDeg);
    EXPECT_EQ(legacy.minFeatureLoopVertices, grouped.featureOptionsOverride->minFeatureLoopVertices);
    EXPECT_EQ(legacy.minCircularFeatureLoopVertices, grouped.minCircularFeatureLoopVertices);
}

TEST(ManuMesh, LegacyFreeFunctionBraceOptionsRemainUnambiguous) {
    const manumesh::Mesh input = manumesh::generatePlaneGrid(2, 1.0, false);
    simplification::QEMSimplifier simplifier(simplification::SimplifyOptions{});
    const manumesh::Mesh output = simplification::simplifyMesh(input, {});
    const manumesh::PlainMesh plainOutput = simplification::simplifyPlainMesh(manumesh::toPlainMesh(input), {});

    EXPECT_FALSE(output.empty());
    EXPECT_FALSE(plainOutput.faces.empty());
    EXPECT_EQ(simplifier.options().targetFaces, -1);
}

TEST(ManuMesh, GroupedFeatureConfigCopiesOneDetectionObject) {
    simplification::SimplifyConfig config;
    config.features.enabled = true;
    config.features.protectionMode = simplification::FeatureProtectionMode::AllFeatureEdges;
    config.features.curveWeight = 0.12;
    config.features.maxCurveDeviationRatio = 0.015;
    config.features.minCircularLoopVertices = 7;
    config.features.detection.featureAngleDeg = 31.0;
    config.features.detection.useSmoothCurvatureFeatures = true;
    config.features.detection.normalFilter.iterations = 3;
    config.features.detection.graphConsolidation.minAlignment = 0.8;
    config.features.detection.surfacePatches.includeWeakEvidence = false;

    const simplification::SimplifyOptions options = simplification::makeSimplifyOptions(config);
    EXPECT_TRUE(options.preserveFeatureCurves);
    EXPECT_EQ(simplification::FeatureProtectionMode::AllFeatureEdges, options.featureProtectionMode);
    EXPECT_DOUBLE_EQ(0.12, options.featureCurveWeight);
    EXPECT_DOUBLE_EQ(0.015, options.maxFeatureCurveDeviationRatio);
    EXPECT_EQ(7, options.minCircularFeatureLoopVertices);
    ASSERT_TRUE(options.featureOptionsOverride.has_value());
    const manumesh::feature::FeatureOptions& detection = *options.featureOptionsOverride;
    EXPECT_DOUBLE_EQ(31.0, detection.featureAngleDeg);
    EXPECT_TRUE(detection.useSmoothCurvatureFeatures);
    EXPECT_EQ(3, detection.normalFilter.iterations);
    EXPECT_DOUBLE_EQ(0.8, detection.graphConsolidation.minAlignment);
    EXPECT_FALSE(detection.surfacePatches.includeWeakEvidence);
}

TEST(ManuMesh, GroupedQualityTextureAndLoggingMapDirectly) {
    simplification::SimplifyConfig config;
    config.quality.preserveBoundary = true;
    config.quality.minTriangleQuality = 0.001;
    config.quality.maxNormalDeviationDeg = 120.0;
    config.quality.localError = simplification::SimplifyErrorLimit::boundingBoxRatio(0.03);
    config.quality.preventLocalIntersections = true;
    config.quality.refinementIterations = 1;
    config.texture.preserveTexture = true;
    config.texture.weight = 2.5;
    config.texture.seamTolerance = 1e-7;
    config.texture.minAreaRatio = 2e-8;
    config.verbose = true;

    const simplification::SimplifyOptions options = simplification::makeSimplifyOptions(config);
    EXPECT_TRUE(options.preserveBoundary);
    EXPECT_DOUBLE_EQ(0.001, options.minTriangleQuality);
    EXPECT_DOUBLE_EQ(120.0, options.maxNormalDeviationDeg);
    EXPECT_DOUBLE_EQ(0.0, options.maxLocalError);
    EXPECT_DOUBLE_EQ(0.03, options.maxLocalErrorRatio);
    EXPECT_TRUE(options.preventLocalIntersections);
    EXPECT_TRUE(options.preserveTexture);
    EXPECT_DOUBLE_EQ(2.5, options.textureWeight);
    EXPECT_DOUBLE_EQ(1e-7, options.textureSeamTolerance);
    EXPECT_DOUBLE_EQ(2e-8, options.minTextureAreaRatio);
    EXPECT_TRUE(options.verbose);
    EXPECT_EQ(1, options.qualityRefinementIterations);
}

TEST(ManuMesh, GroupedTargetSelectsExactlyOneUnit) {
    simplification::SimplifyConfig config;
    config.target = simplification::SimplifyTarget::faceCount(8);
    const simplification::SimplifyOptions faceCount = simplification::makeSimplifyOptions(config);
    EXPECT_EQ(8, faceCount.targetFaces);

    config.target = simplification::SimplifyTarget::ratio(0.9);
    const simplification::SimplifyOptions ratio = simplification::makeSimplifyOptions(config);
    EXPECT_EQ(-1, ratio.targetFaces);
    EXPECT_DOUBLE_EQ(0.9, ratio.targetRatio);
}

TEST(ManuMesh, GroupedConfigRunsThroughObjectAndPlainMeshApis) {
    simplification::SimplifyConfig config;
    config.target = simplification::SimplifyTarget::faceCount(8);
    config.cost.lineQuadrics = simplification::LineQuadricConfig::uniform(1e-3);

    const manumesh::Mesh input = manumesh::generatePlaneGrid(4, 1.0, false);
    simplification::QEMSimplifier simplifier;
    simplifier.setConfig(config);
    simplification::SimplifyReport report;
    const manumesh::Mesh output = simplifier.simplify(input, &report);
    EXPECT_LE(output.faces.size(), 8u);
    EXPECT_EQ(static_cast<int>(output.faces.size()), report.finalFaces);

    simplification::SimplifyReport plainReport;
    const manumesh::PlainMesh plainOutput = simplification::simplifyPlainMesh(
        manumesh::toPlainMesh(input), simplification::makeSimplifyOptions(config), &plainReport
    );
    EXPECT_EQ(output.faces.size(), plainOutput.faces.size());
    EXPECT_EQ(report.finalFaces, plainReport.finalFaces);
}

TEST(ManuMesh, GroupedLineQuadricConfigUsesOneModeAndOneWeight) {
    simplification::SimplifyConfig disabledConfig;
    disabledConfig.cost.lineQuadrics = simplification::LineQuadricConfig::disabled();
    const simplification::SimplifyOptions disabled = simplification::makeSimplifyOptions(disabledConfig);
    EXPECT_FALSE(disabled.useLineQuadrics);
    EXPECT_FALSE(disabled.adaptiveScale);
    EXPECT_DOUBLE_EQ(0.0, disabled.lineWeight);
    EXPECT_DOUBLE_EQ(0.0, disabled.adaptiveBaseLineWeight);

    simplification::SimplifyConfig uniformConfig;
    uniformConfig.cost.lineQuadrics = simplification::LineQuadricConfig::uniform(0.002);
    const simplification::SimplifyOptions uniform = simplification::makeSimplifyOptions(uniformConfig);
    EXPECT_TRUE(uniform.useLineQuadrics);
    EXPECT_FALSE(uniform.adaptiveScale);
    EXPECT_DOUBLE_EQ(0.002, uniform.lineWeight);
    EXPECT_DOUBLE_EQ(0.0, uniform.adaptiveBaseLineWeight);

    simplification::SimplifyConfig adaptiveConfig;
    adaptiveConfig.cost.lineQuadrics = simplification::LineQuadricConfig::adaptive(0.02);
    adaptiveConfig.cost.weightMode = simplification::WeightMode::Dihedral;
    adaptiveConfig.cost.featureBoost = 0.12;
    adaptiveConfig.cost.boundaryWeight = 0.03;
    const simplification::SimplifyOptions adaptive = simplification::makeSimplifyOptions(adaptiveConfig);
    EXPECT_TRUE(adaptive.useLineQuadrics);
    EXPECT_TRUE(adaptive.adaptiveScale);
    EXPECT_DOUBLE_EQ(0.0, adaptive.lineWeight);
    EXPECT_DOUBLE_EQ(0.02, adaptive.adaptiveBaseLineWeight);
    EXPECT_EQ(simplification::WeightMode::Dihedral, adaptive.weightMode);
    EXPECT_DOUBLE_EQ(0.12, adaptive.featureBoost);
    EXPECT_DOUBLE_EQ(0.03, adaptive.boundaryWeight);

    simplification::QEMSimplifier simplifier;
    uniformConfig.cost.lineQuadrics = simplification::LineQuadricConfig::uniform(-1.0);
    EXPECT_THROW(simplifier.setConfig(uniformConfig), std::invalid_argument);
}

TEST(ManuMesh, TaggedFaceCountDoesNotReuseLegacyRatioSentinel) {
    simplification::SimplifyConfig config;
    config.target = simplification::SimplifyTarget::faceCount(-1);

    simplification::QEMSimplifier simplifier;
    EXPECT_THROW(simplifier.setConfig(config), std::invalid_argument);
}

TEST(ManuMesh, GroupedLocalErrorUsesExactlyOneUnit) {
    simplification::SimplifyConfig absoluteConfig;
    absoluteConfig.quality.localError = simplification::SimplifyErrorLimit::absolute(0.02);
    const simplification::SimplifyOptions absoluteOptions = simplification::makeSimplifyOptions(absoluteConfig);
    EXPECT_DOUBLE_EQ(0.02, absoluteOptions.maxLocalError);
    EXPECT_DOUBLE_EQ(0.0, absoluteOptions.maxLocalErrorRatio);

    simplification::SimplifyConfig relativeConfig;
    relativeConfig.quality.localError = simplification::SimplifyErrorLimit::boundingBoxRatio(0.03);
    const simplification::SimplifyOptions relativeOptions = simplification::makeSimplifyOptions(relativeConfig);
    EXPECT_DOUBLE_EQ(0.0, relativeOptions.maxLocalError);
    EXPECT_DOUBLE_EQ(0.03, relativeOptions.maxLocalErrorRatio);
}

TEST(ManuMesh, SimplifyReportProvidesCommonSummary) {
    simplification::SimplifyReport report;
    report.initialVertices = 10;
    report.initialFaces = 12;
    report.finalVertices = 7;
    report.finalFaces = 6;
    report.collapsedEdges = 3;
    report.rejectedCollapses = 4;
    report.terminationReason = simplification::SimplifyTerminationReason::ReachedTarget;
    simplification::SimplifySummary summary = report.summary();
    EXPECT_TRUE(summary.reachedTarget());
    EXPECT_EQ(7, summary.finalVertices);
    EXPECT_EQ(6, summary.finalFaces);
    EXPECT_EQ(3, summary.collapsedEdges);

    summary.terminationReason = simplification::SimplifyTerminationReason::AlreadyAtOrBelowTarget;
    EXPECT_TRUE(summary.reachedTarget());
    summary.terminationReason = simplification::SimplifyTerminationReason::NoCandidates;
    EXPECT_FALSE(summary.reachedTarget());
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

TEST(ManuMesh, LegacyMetricsCsvUsesClassicLocaleAndEscapesLabels) {
    GlobalLocaleRestore restore;
    std::locale::global(std::locale(std::locale(), new CommaDecimalPunct()));

    simplification::MeshStats stats;
    stats.area = 1.5;
    const std::string row = simplification::statsRowCsv("case, \"A\"", stats);

    EXPECT_EQ(0u, row.find("\"case, \"\"A\"\"\""));
    EXPECT_NE(std::string::npos, row.find(",1.5,"));
    EXPECT_EQ(std::string::npos, row.find(",1,5,"));
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

TEST(ManuMesh, QEMSimplifierRejectsMismatchedOrCorruptPrecomputedFeatureAnalysis) {
    const manumesh::Mesh input = manumesh::generateCylinderGrid(24, 6, 1.0, 2.0);
    const manumesh::feature::FeatureAnalysis features =
        manumesh::feature::detectFeatureCurves(input, circularFeatureOptions());
    ASSERT_FALSE(features.loops.empty());

    manumesh::simplification::QEMSimplifier simplifier(protectedIndustrialFeatureOptions(0.75));

    manumesh::Mesh changedGeometry = input;
    changedGeometry.vertices[0].x() += 0.01;
    EXPECT_THROW(simplifier.simplify(changedGeometry, features), std::invalid_argument);

    manumesh::Mesh changedTopology = input;
    std::swap(changedTopology.faces[0].v[0], changedTopology.faces[0].v[1]);
    EXPECT_THROW(simplifier.simplify(changedTopology, features), std::invalid_argument);

    manumesh::feature::FeatureAnalysis badLoopVertex = features;
    badLoopVertex.loops.front().vertices.front() = static_cast<int>(input.vertices.size());
    EXPECT_THROW(simplifier.simplify(input, badLoopVertex), std::invalid_argument);

    manumesh::feature::FeatureAnalysis badLoopId = features;
    badLoopId.loops.front().id = static_cast<int>(badLoopId.loops.size());
    EXPECT_THROW(
        manumesh::simplification::simplifyMesh(input, protectedIndustrialFeatureOptions(0.75), badLoopId, nullptr),
        std::invalid_argument
    );
}

TEST(ManuMesh, QEMSimplifierRequiresPrecomputedNormalTensorWeightsWhenRequested) {
    const manumesh::Mesh input = manumesh::generateRidgeGrid(16, 2.0, 0.5);
    manumesh::feature::FeatureOptions detectionOptions;
    detectionOptions.useNormalTensorFeatures = false;
    const manumesh::feature::FeatureAnalysis features = manumesh::feature::detectFeatureCurves(input, detectionOptions);
    ASSERT_TRUE(features.normalTensorVertexWeights.empty());

    manumesh::simplification::SimplifyOptions options = paperLineQuadricsOptions(0.75);
    options.weightMode = manumesh::simplification::WeightMode::NormalTensor;

    EXPECT_THROW(manumesh::simplification::simplifyMesh(input, options, features, nullptr), std::invalid_argument);
}

TEST(ManuMesh, PrimarySimplifyComputesNormalTensorWeightsWhenTensorGraphEvidenceIsDisabled) {
    const manumesh::Mesh input = manumesh::generateRidgeGrid(24, 2.0, 0.6);
    manumesh::simplification::SimplifyOptions options = paperLineQuadricsOptions(0.75);
    options.weightMode = manumesh::simplification::WeightMode::NormalTensor;
    options.preserveFeatureCurves = true;

    manumesh::feature::FeatureOptions featureOptions;
    featureOptions.featureAngleDeg = 179.0;
    featureOptions.useNormalTensorFeatures = false;
    featureOptions.normalTensorFeatureThreshold = 0.04;
    featureOptions.normalTensorSmoothingIterations = 1;
    featureOptions.normalTensorScaleCount = 3;
    featureOptions.normalTensorMinPersistentScales = 2;
    options.featureOptionsOverride = featureOptions;

    manumesh::simplification::SimplifyReport report;
    const manumesh::Mesh output = manumesh::simplification::simplifyMesh(input, options, &report);

    EXPECT_LT(output.faces.size(), input.faces.size());
    EXPECT_GT(report.normalTensorScoredVertices, 0);
    EXPECT_GT(report.maxAppliedLineWeight, report.minAppliedLineWeight);
}

TEST(ManuMesh, QEMSimplifierCopiesPimplStateIndependently) {
    static_assert(
        std::is_nothrow_move_constructible<manumesh::simplification::QEMSimplifier>::value,
        "QEMSimplifier move construction must remain noexcept"
    );
    static_assert(
        std::is_nothrow_move_assignable<manumesh::simplification::QEMSimplifier>::value,
        "QEMSimplifier move assignment must remain noexcept"
    );

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
    EXPECT_DOUBLE_EQ(manumesh::simplification::SimplifyOptions{}.targetRatio, copied.options().targetRatio);
    EXPECT_EQ(manumesh::simplification::SimplifyTerminationReason::NotStarted, copied.report().terminationReason);

    manumesh::simplification::SimplifyOptions movedFromOptions;
    movedFromOptions.targetRatio = 0.75;
    copied.setOptions(movedFromOptions);
    EXPECT_DOUBLE_EQ(0.75, copied.options().targetRatio);

    manumesh::simplification::SimplifyOptions copiedOptions = copied.options();
    copiedOptions.targetRatio = 0.25;
    copied.setOptions(copiedOptions);

    EXPECT_DOUBLE_EQ(0.60, original.options().targetRatio);
    EXPECT_DOUBLE_EQ(0.25, copied.options().targetRatio);
    const manumesh::Mesh reusedOutput = copied.simplify(manumesh::generatePlaneGrid(4, 1.0, false));
    EXPECT_FALSE(reusedOutput.empty());

    manumesh::simplification::QEMSimplifier moveAssigned;
    moveAssigned = std::move(moved);
    EXPECT_EQ(original.report().finalFaces, moveAssigned.report().finalFaces);
    EXPECT_DOUBLE_EQ(manumesh::simplification::SimplifyOptions{}.targetRatio, moved.options().targetRatio);
    EXPECT_EQ(manumesh::simplification::SimplifyTerminationReason::NotStarted, moved.report().terminationReason);
}

TEST(ManuMesh, QEMSimplifierValidatesOptionsWhenConfigured) {
    manumesh::simplification::SimplifyOptions invalidOptions;
    invalidOptions.targetRatio = 0.0;
    EXPECT_THROW((void)manumesh::simplification::QEMSimplifier{invalidOptions}, std::invalid_argument);

    manumesh::simplification::QEMSimplifier simplifier;
    const double originalRatio = simplifier.options().targetRatio;
    EXPECT_THROW(simplifier.setOptions(invalidOptions), std::invalid_argument);
    EXPECT_DOUBLE_EQ(originalRatio, simplifier.options().targetRatio);
}
