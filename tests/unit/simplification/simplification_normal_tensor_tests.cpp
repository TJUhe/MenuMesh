#include "SimplificationTestSupport.h"
#include "algorithms/feature_detection/FeatureDetector.h"
#include "algorithms/simplification/Metrics.h"
#include "algorithms/simplification/PlainSimplifier.h"
#include "algorithms/simplification/QEMSimplifier.h"
#include "core/MeshGenerators.h"
#include "core/MeshTopology.h"
#include "core/PlainMesh.h"
#include "simplification/detail/FeatureGuidance.h"

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

namespace {

manumesh::Mesh makeRectangularBipyramidMesh() {
    manumesh::Mesh mesh;
    mesh.vertices = {
        manumesh::Vec3(0.0, 0.0, 0.3),
        manumesh::Vec3(0.0, 0.0, -0.3),
        manumesh::Vec3(2.0, 0.3, 0.0),
        manumesh::Vec3(-2.0, 0.3, 0.0),
        manumesh::Vec3(-2.0, -0.3, 0.0),
        manumesh::Vec3(2.0, -0.3, 0.0),
    };
    for (int i = 0; i < 4; ++i) {
        const int a = 2 + i;
        const int b = 2 + ((i + 1) % 4);
        mesh.faces.push_back({{0, b, a}});
        mesh.faces.push_back({{1, a, b}});
    }
    return mesh;
}

} // namespace

TEST(ManuMesh, DihedralWeightModeUsesReflexKnifeEdgeAngle) {
    const manumesh::Mesh input = makeRectangularBipyramidMesh();
    manumesh::simplification::SimplifyOptions options;
    options.weightMode = manumesh::simplification::WeightMode::Dihedral;
    options.featureAngleDeg = 120.0;

    const manumesh::simplification::FeatureWeightScores scores =
        manumesh::simplification::computeFeatureWeightScores(input, options);

    ASSERT_EQ(input.vertices.size(), scores.values.size());
    EXPECT_DOUBLE_EQ(0.0, scores.values[0]);
    EXPECT_DOUBLE_EQ(0.0, scores.values[1]);
    for (int vertex = 2; vertex < 6; ++vertex) {
        EXPECT_GT(scores.values[vertex], 0.70) << "vertex=" << vertex;
    }
}

TEST(ManuMesh, NormalTensorScoresSeparatePlaneFromRidge) {
    const manumesh::Mesh plane = manumesh::generatePlaneGrid(24, 2.0, false);
    const manumesh::Mesh ridge = manumesh::generateRidgeGrid(24, 2.0, 0.5);

    const std::vector<manumesh::feature::NormalTensorVertex> planeTensor =
        manumesh::feature::computeNormalTensorFeatures(plane);
    const std::vector<manumesh::feature::NormalTensorVertex> ridgeTensor =
        manumesh::feature::computeNormalTensorFeatures(ridge);

    double planeMax = 0.0;
    double ridgeMax = 0.0;
    for (const manumesh::feature::NormalTensorVertex& vertex : planeTensor) {
        planeMax = std::max(planeMax, vertex.featureScore);
    }
    for (const manumesh::feature::NormalTensorVertex& vertex : ridgeTensor) {
        ridgeMax = std::max(ridgeMax, vertex.featureScore);
    }

    EXPECT_LT(planeMax, 1e-8);
    EXPECT_GT(ridgeMax, 0.08);
}

TEST(ManuMesh, NormalTensorAddsFeatureEdgesWhenDihedralThresholdIsStrict) {
    const manumesh::Mesh input = manumesh::generateRidgeGrid(32, 2.0, 0.6);
    manumesh::feature::FeatureOptions options;
    options.featureAngleDeg = 179.0;
    options.normalTensorFeatureThreshold = 0.06;
    options.normalTensorMinEdgeAlignment = 0.2;
    options.normalTensorScaleCount = 3;
    options.normalTensorMinPersistentScales = 2;

    const manumesh::feature::FeatureAnalysis features = manumesh::feature::detectFeatureCurves(input, options);

    EXPECT_EQ(0, features.dihedralFeatureEdges);
    EXPECT_GT(features.normalTensorFeatureEdges, 0);
    EXPECT_GT(features.maxNormalTensorFeatureScore, 0.06);
    EXPECT_GT(features.normalTensorScoredVertices, 0);
    EXPECT_GT(features.maxNormalTensorPersistentScore, 0.06);
    EXPECT_GT(features.meanNormalTensorLocalScale, 0.0);
    EXPECT_GT(features.meanNormalTensorPersistence, 1.0);
    EXPECT_GT(features.components.size(), 0u);
    EXPECT_GT(features.weakFeatureComponents, 0);
    EXPECT_GT(features.meanFeatureComponentConfidence, 0.0);
}

TEST(ManuMesh, NormalTensorReportsLocalScaleAndPersistentScore) {
    const manumesh::Mesh input = manumesh::generateRidgeGrid(16, 2.0, 0.5);

    const std::vector<manumesh::feature::NormalTensorVertex> tensor =
        manumesh::feature::computeNormalTensorFeatures(input, {1, 3}, 0.04);

    ASSERT_EQ(input.vertices.size(), tensor.size());
    double maxPersistentScore = 0.0;
    int persistentVertices = 0;
    for (const manumesh::feature::NormalTensorVertex& vertex : tensor) {
        EXPECT_GT(vertex.localScale, 0.0);
        EXPECT_GE(vertex.persistentFeatureScore, 0.0);
        EXPECT_LE(vertex.persistentFeatureScore, vertex.featureScore + 1e-12);
        maxPersistentScore = std::max(maxPersistentScore, vertex.persistentFeatureScore);
        if (vertex.persistentScales >= 2) {
            ++persistentVertices;
        }
    }

    EXPECT_GT(maxPersistentScore, 0.04);
    EXPECT_GT(persistentVertices, 0);
}

TEST(ManuMesh, NormalTensorPersistenceRequiresSignificantScaleSupport) {
    const manumesh::Mesh input = manumesh::generateRidgeGrid(16, 2.0, 0.5);

    const std::vector<manumesh::feature::NormalTensorVertex> permissive =
        manumesh::feature::computeNormalTensorFeatures(input, {1, 3}, 0.04);
    const std::vector<manumesh::feature::NormalTensorVertex> suppressed =
        manumesh::feature::computeNormalTensorFeatures(input, {1, 3}, 1.0);

    int permissivePersistentVertices = 0;
    for (const manumesh::feature::NormalTensorVertex& vertex : permissive) {
        if (vertex.persistentScales >= 2) {
            ++permissivePersistentVertices;
        }
    }
    EXPECT_GT(permissivePersistentVertices, 0);
    for (const manumesh::feature::NormalTensorVertex& vertex : suppressed) {
        EXPECT_EQ(0, vertex.persistentScales);
        EXPECT_DOUBLE_EQ(0.0, vertex.persistentFeatureScore);
    }
}

TEST(ManuMesh, NormalTensorFeatureThresholdControlsPersistentEdgeEvidence) {
    const manumesh::Mesh input = manumesh::generateRidgeGrid(32, 2.0, 0.6);
    manumesh::feature::FeatureOptions options;
    options.featureAngleDeg = 179.0;
    options.normalTensorMinEdgeAlignment = 0.2;
    options.normalTensorScaleCount = 3;
    options.normalTensorMinPersistentScales = 2;

    options.normalTensorFeatureThreshold = 0.06;
    const manumesh::feature::FeatureAnalysis detected = manumesh::feature::detectFeatureCurves(input, options);
    EXPECT_GT(detected.normalTensorFeatureEdges, 0);

    options.normalTensorFeatureThreshold = 1.0;
    const manumesh::feature::FeatureAnalysis suppressed = manumesh::feature::detectFeatureCurves(input, options);
    EXPECT_EQ(0, suppressed.normalTensorFeatureEdges);
    EXPECT_EQ(0, suppressed.weakFeatureComponents);
}

TEST(ManuMesh, NormalTensorWeightModeAppliesSpatiallyVaryingWeights) {
    const manumesh::Mesh input = manumesh::generateRidgeGrid(32, 2.0, 0.6);
    manumesh::simplification::SimplifyOptions options = paperLineQuadricsOptions(0.70);
    options.weightMode = manumesh::simplification::WeightMode::NormalTensor;
    options.normalTensorFeatureThreshold = 0.04;
    options.normalTensorSmoothingIterations = 1;
    options.normalTensorScaleCount = 3;
    options.normalTensorMinPersistentScales = 2;

    const SimplifiedMesh result = simplifyWithReport(input, options);

    expectBudgetedSimplification(result, input, 0.70);
    EXPECT_GT(result.report.maxAppliedLineWeight, result.report.minAppliedLineWeight);
    EXPECT_GT(result.report.normalTensorScoredVertices, 0);
    EXPECT_GT(result.report.maxNormalTensorPersistentScore, 0.0);
    EXPECT_GT(result.report.meanNormalTensorLocalScale, 0.0);
    EXPECT_GT(result.report.meanNormalTensorPersistence, 1.0);
    EXPECT_GE(result.report.featureComponents, 0);
}

TEST(ManuMesh, FeatureProtectionReportsComponentConfidenceDiagnostics) {
    const manumesh::Mesh input = manumesh::generateRidgeGrid(32, 2.0, 0.6);
    manumesh::simplification::SimplifyOptions options = paperLineQuadricsOptions(0.80);
    options.preserveFeatureCurves = true;
    options.featureAngleDeg = 179.0;
    options.normalTensorFeatureThreshold = 0.06;
    options.normalTensorMinEdgeAlignment = 0.2;
    options.normalTensorScaleCount = 3;
    options.normalTensorMinPersistentScales = 2;
    options.featureProtectionMode = manumesh::simplification::FeatureProtectionMode::AllFeatureEdges;

    const SimplifiedMesh result = simplifyWithReport(input, options);

    expectBudgetedSimplification(result, input, 0.80);
    EXPECT_GT(result.report.featureComponents, 0);
    EXPECT_GT(result.report.weakFeatureComponents, 0);
    EXPECT_GT(result.report.highConfidenceFeatureComponents, 0);
    EXPECT_GT(result.report.meanFeatureComponentConfidence, 0.0);
    EXPECT_GE(result.report.minFeatureComponentConfidence, 0.0);
}
