/**
 * @file tests/unit/simplification/simplification_normal_tensor_tests.cpp
 * @brief 验证法向张量证据、持久尺度和特征加权的几何不变性。
 * @ingroup manumesh_tests
 */

#include "SimplificationTestSupport.h"
#include "algorithms/feature_detection/FeatureDetector.h"
#include "algorithms/simplification/Metrics.h"
#include "algorithms/simplification/PlainSimplifier.h"
#include "algorithms/simplification/QEMSimplifier.h"
#include "common/detail/MeshQueries.h"
#include "core/MeshGenerators.h"
#include "core/MeshTopology.h"
#include "core/PlainMesh.h"
#include "simplification/detail/FeatureGuidance.h"

#include "core/Filesystem.h"
#include <Eigen/Geometry>
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <gtest/gtest.h>
#include <limits>
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

manumesh::Mesh makeRegularTetrahedronMesh() {
    manumesh::Mesh mesh;
    mesh.vertices = {
        manumesh::Vec3(1.0, 1.0, 1.0),
        manumesh::Vec3(-1.0, -1.0, 1.0),
        manumesh::Vec3(-1.0, 1.0, -1.0),
        manumesh::Vec3(1.0, -1.0, -1.0),
    };
    mesh.faces = {
        {{0, 2, 1}},
        {{0, 1, 3}},
        {{0, 3, 2}},
        {{1, 2, 3}},
    };
    return mesh;
}
manumesh::Mesh uniformlyScaleNormalTensorMesh(manumesh::Mesh mesh, double factor) {
    for (manumesh::Vec3& vertex : mesh.vertices) {
        vertex *= factor;
    }
    return mesh;
}

manumesh::Mesh makeClusteredNormalTensorRidge(int subdivisions, double size, double height) {
    manumesh::Mesh mesh = manumesh::generatePlaneGrid(subdivisions, size, true);
    for (manumesh::Vec3& vertex : mesh.vertices) {
        vertex.z() = height * (1.0 - std::abs(vertex.x()) / (0.5 * size));
    }
    return mesh;
}

double maximumNormalTensorPersistentScore(const std::vector<manumesh::feature::NormalTensorVertex>& tensor) {
    double maximum = 0.0;
    for (const manumesh::feature::NormalTensorVertex& vertex : tensor) {
        maximum = std::max(maximum, vertex.persistentFeatureScore);
    }
    return maximum;
}

std::pair<int, int> oneSidedAlignedNormalTensorEdge(
    const manumesh::Mesh& mesh,
    const std::vector<manumesh::feature::NormalTensorVertex>& tensor,
    double featureThreshold,
    int minimumPersistentScales,
    double alignmentThreshold
) {
    const std::vector<char> boundary = manumesh::common::computeBoundaryVertices(mesh);
    for (const auto& pairEntry : manumesh::uniqueEdges(mesh)) {
        const int a = pairEntry.first;
        const int b = pairEntry.second;
        const manumesh::feature::NormalTensorVertex& ta = tensor[a];
        const manumesh::feature::NormalTensorVertex& tb = tensor[b];
        if (boundary[a] || boundary[b] ||
            std::min(ta.persistentScales, tb.persistentScales) < minimumPersistentScales ||
            std::min(ta.persistentFeatureScore, tb.persistentFeatureScore) < featureThreshold ||
            ta.creaseSaliency < ta.cornerSaliency || tb.creaseSaliency < tb.cornerSaliency) {
            continue;
        }

        manumesh::Vec3 direction = mesh.vertices[b] - mesh.vertices[a];
        if (direction.norm() <= 1e-20) {
            continue;
        }
        direction.normalize();
        const double alignA = std::abs(direction.dot(ta.creaseTangent));
        const double alignB = std::abs(direction.dot(tb.creaseTangent));
        if (std::min(alignA, alignB) < alignmentThreshold && std::max(alignA, alignB) >= alignmentThreshold) {
            return {a, b};
        }
    }
    return {-1, -1};
}
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
        manumesh::feature::computeNormalTensorFeatures(plane, {1, 4, {}}, 0.04);
    const std::vector<manumesh::feature::NormalTensorVertex> ridgeTensor =
        manumesh::feature::computeNormalTensorFeatures(ridge, {1, 4, {}}, 0.04);

    double planeMax = 0.0;
    double ridgeMax = 0.0;
    for (const manumesh::feature::NormalTensorVertex& vertex : planeTensor) {
        planeMax = std::max(planeMax, vertex.featureScore);
    }
    for (const manumesh::feature::NormalTensorVertex& vertex : ridgeTensor) {
        ridgeMax = std::max(ridgeMax, vertex.featureScore);
    }

    EXPECT_LT(planeMax, 1e-8);
    EXPECT_LT(maximumNormalTensorPersistentScore(planeTensor), 1e-8);
    EXPECT_GT(ridgeMax, 0.08);
    EXPECT_GT(maximumNormalTensorPersistentScore(ridgeTensor), 0.08);
}

TEST(ManuMesh, NormalTensorStandaloneEntryRejectsInvalidOptions) {
    const manumesh::Mesh input = manumesh::generatePlaneGrid(4, 1.0, false);
    manumesh::feature::NormalTensorOptions options;

    options.smoothingIterations = -1;
    EXPECT_THROW(manumesh::feature::computeNormalTensorFeatures(input, options), std::invalid_argument);

    options.smoothingIterations = manumesh::feature::kMaxNormalTensorSmoothingIterations + 1;
    EXPECT_THROW(manumesh::feature::computeNormalTensorFeatures(input, options), std::invalid_argument);

    options = {};
    options.scaleCount = 0;
    EXPECT_THROW(manumesh::feature::computeNormalTensorFeatures(input, options), std::invalid_argument);

    options.scaleCount = manumesh::feature::kMaxNormalTensorScaleCount + 1;
    EXPECT_THROW(manumesh::feature::computeNormalTensorFeatures(input, options), std::invalid_argument);

    options = {};
    EXPECT_THROW(manumesh::feature::computeNormalTensorFeatures(input, options, -0.01), std::invalid_argument);
    EXPECT_THROW(
        manumesh::feature::computeNormalTensorFeatures(input, options, std::numeric_limits<double>::quiet_NaN()),
        std::invalid_argument
    );
    EXPECT_THROW(
        manumesh::feature::computeNormalTensorFeatures(input, options, std::numeric_limits<double>::infinity()),
        std::invalid_argument
    );
}

TEST(ManuMesh, NormalTensorEvidenceIsInvariantUnderUniformScaling) {
    const manumesh::Mesh input = manumesh::generateRidgeGrid(32, 2.0, 0.6);
    constexpr double scaleFactor = 7.5;
    const manumesh::Mesh scaled = uniformlyScaleNormalTensorMesh(input, scaleFactor);
    const manumesh::feature::NormalTensorOptions options{1, 4, {}};
    const auto original = manumesh::feature::computeNormalTensorFeatures(input, options, 0.04);
    const auto enlarged = manumesh::feature::computeNormalTensorFeatures(scaled, options, 0.04);

    ASSERT_EQ(original.size(), enlarged.size());
    for (std::size_t i = 0; i < original.size(); ++i) {
        EXPECT_NEAR(original[i].surfaceSaliency, enlarged[i].surfaceSaliency, 1e-12) << "vertex=" << i;
        EXPECT_NEAR(original[i].creaseSaliency, enlarged[i].creaseSaliency, 1e-12) << "vertex=" << i;
        EXPECT_NEAR(original[i].cornerSaliency, enlarged[i].cornerSaliency, 1e-12) << "vertex=" << i;
        EXPECT_NEAR(original[i].featureScore, enlarged[i].featureScore, 1e-12) << "vertex=" << i;
        EXPECT_NEAR(original[i].averageFeatureScore, enlarged[i].averageFeatureScore, 1e-12) << "vertex=" << i;
        EXPECT_NEAR(original[i].persistentFeatureScore, enlarged[i].persistentFeatureScore, 1e-12) << "vertex=" << i;
        EXPECT_EQ(original[i].persistentScales, enlarged[i].persistentScales) << "vertex=" << i;
        EXPECT_EQ(original[i].selectedScale, enlarged[i].selectedScale) << "vertex=" << i;
        EXPECT_EQ(original[i].smoothingSteps, enlarged[i].smoothingSteps) << "vertex=" << i;
        EXPECT_NEAR(scaleFactor * original[i].localScale, enlarged[i].localScale, 1e-11) << "vertex=" << i;
        EXPECT_NEAR(scaleFactor * original[i].effectiveRadius, enlarged[i].effectiveRadius, 1e-11) << "vertex=" << i;
        if (original[i].featureScore > 1e-10) {
            EXPECT_NEAR(1.0, std::abs(original[i].normal.dot(enlarged[i].normal)), 1e-12) << "vertex=" << i;
            EXPECT_NEAR(1.0, std::abs(original[i].creaseTangent.dot(enlarged[i].creaseTangent)), 1e-12)
                << "vertex=" << i;
        }
    }
}

TEST(ManuMesh, NormalTensorEvidenceIsEquivariantUnderRigidRotation) {
    const manumesh::Mesh input = manumesh::generateRidgeGrid(32, 2.0, 0.6);
    const Eigen::Matrix3d rotation =
        Eigen::AngleAxisd(0.73, manumesh::Vec3(1.0, 2.0, 3.0).normalized()).toRotationMatrix();
    manumesh::Mesh rotated = input;
    for (manumesh::Vec3& vertex : rotated.vertices) {
        vertex = rotation * vertex;
    }

    const manumesh::feature::NormalTensorOptions options{1, 4, {}};
    const auto original = manumesh::feature::computeNormalTensorFeatures(input, options, 0.04);
    const auto transformed = manumesh::feature::computeNormalTensorFeatures(rotated, options, 0.04);

    ASSERT_EQ(original.size(), transformed.size());
    int comparedDirections = 0;
    for (std::size_t i = 0; i < original.size(); ++i) {
        EXPECT_NEAR(original[i].surfaceSaliency, transformed[i].surfaceSaliency, 1e-11) << "vertex=" << i;
        EXPECT_NEAR(original[i].creaseSaliency, transformed[i].creaseSaliency, 1e-11) << "vertex=" << i;
        EXPECT_NEAR(original[i].cornerSaliency, transformed[i].cornerSaliency, 1e-11) << "vertex=" << i;
        EXPECT_NEAR(original[i].featureScore, transformed[i].featureScore, 1e-11) << "vertex=" << i;
        EXPECT_NEAR(original[i].averageFeatureScore, transformed[i].averageFeatureScore, 1e-11) << "vertex=" << i;
        EXPECT_NEAR(original[i].persistentFeatureScore, transformed[i].persistentFeatureScore, 1e-11) << "vertex=" << i;
        EXPECT_NEAR(original[i].localScale, transformed[i].localScale, 1e-11) << "vertex=" << i;
        EXPECT_NEAR(original[i].effectiveRadius, transformed[i].effectiveRadius, 1e-11) << "vertex=" << i;
        EXPECT_EQ(original[i].persistentScales, transformed[i].persistentScales) << "vertex=" << i;
        EXPECT_EQ(original[i].selectedScale, transformed[i].selectedScale) << "vertex=" << i;
        EXPECT_EQ(original[i].smoothingSteps, transformed[i].smoothingSteps) << "vertex=" << i;

        if (original[i].persistentFeatureScore > 0.04 &&
            original[i].creaseSaliency > original[i].cornerSaliency + 1e-8) {
            const manumesh::Vec3 expectedNormal = rotation * original[i].normal;
            const manumesh::Vec3 expectedTangent = rotation * original[i].creaseTangent;
            EXPECT_NEAR(1.0, std::abs(expectedNormal.dot(transformed[i].normal)), 1e-10) << "vertex=" << i;
            EXPECT_NEAR(1.0, std::abs(expectedTangent.dot(transformed[i].creaseTangent)), 1e-10) << "vertex=" << i;
            ++comparedDirections;
        }
    }
    EXPECT_GT(comparedDirections, 0);
}

TEST(ManuMesh, NormalTensorReportsTheSelectedSmoothingRadius) {
    const manumesh::Mesh input = manumesh::generateBumpGrid(32, 2.0);
    const std::vector<double> averageEdgeLength = manumesh::common::computeVertexAverageEdgeLength(input);
    const auto tensor = manumesh::feature::computeNormalTensorFeatures(input, {1, 4, {}}, 0.01);
    constexpr std::array<double, 4> expectedMultipliers = {1.0, 1.5, 2.0, 2.5};
    bool selectedCoarserScale = false;

    ASSERT_EQ(averageEdgeLength.size(), tensor.size());
    for (std::size_t i = 0; i < tensor.size(); ++i) {
        ASSERT_GT(averageEdgeLength[i], 0.0);
        const double multiplier = tensor[i].localScale / averageEdgeLength[i];
        double closest = 1e30;
        for (double expected : expectedMultipliers) {
            closest = std::min(closest, std::abs(multiplier - expected));
        }
        EXPECT_LT(closest, 1e-11) << "vertex=" << i << " multiplier=" << multiplier;
        EXPECT_GE(tensor[i].selectedScale, 0) << "vertex=" << i;
        EXPECT_LT(tensor[i].selectedScale, 4) << "vertex=" << i;
        EXPECT_EQ(1 + tensor[i].selectedScale, tensor[i].smoothingSteps) << "vertex=" << i;
        EXPECT_GE(tensor[i].effectiveRadius, tensor[i].localScale) << "vertex=" << i;
        selectedCoarserScale = selectedCoarserScale || (tensor[i].featureScore > 0.01 && multiplier > 1.25);
    }
    EXPECT_TRUE(selectedCoarserScale);
}

TEST(ManuMesh, NormalTensorRidgeSupportIsStableUnderIrregularSampling) {
    constexpr int subdivisions = 32;
    const manumesh::Mesh uniform = manumesh::generateRidgeGrid(subdivisions, 2.0, 0.6);
    const manumesh::Mesh clustered = makeClusteredNormalTensorRidge(subdivisions, 2.0, 0.6);
    const auto uniformTensor = manumesh::feature::computeNormalTensorFeatures(uniform, {1, 4, {}}, 0.04);
    const auto clusteredTensor = manumesh::feature::computeNormalTensorFeatures(clustered, {1, 4, {}}, 0.04);

    double uniformMean = 0.0;
    double clusteredMean = 0.0;
    int crestVertices = 0;
    for (int y = 1; y < subdivisions; ++y) {
        const int vertex = y * (subdivisions + 1) + subdivisions / 2;
        ASSERT_LT(vertex, static_cast<int>(uniformTensor.size()));
        ASSERT_LT(vertex, static_cast<int>(clusteredTensor.size()));
        EXPECT_EQ(4, uniformTensor[vertex].persistentScales) << "vertex=" << vertex;
        EXPECT_EQ(4, clusteredTensor[vertex].persistentScales) << "vertex=" << vertex;
        uniformMean += uniformTensor[vertex].persistentFeatureScore;
        clusteredMean += clusteredTensor[vertex].persistentFeatureScore;
        ++crestVertices;
    }
    ASSERT_GT(crestVertices, 0);
    uniformMean /= static_cast<double>(crestVertices);
    clusteredMean /= static_cast<double>(crestVertices);
    EXPECT_NEAR(uniformMean, clusteredMean, 5e-4);
}

TEST(ManuMesh, NormalTensorEdgeEvidenceRequiresBothEndpointTangents) {
    const manumesh::Mesh input = manumesh::generateBumpGrid(32, 2.0);
    manumesh::feature::FeatureOptions options;
    options.featureAngleDeg = 180.0;
    options.loopTraceAngleDeg = 180.0;
    options.normalTensorFeatureThreshold = 0.01;
    options.normalTensorMinEdgeAlignment = 0.65;
    options.normalTensorSmoothingIterations = 1;
    options.normalTensorScaleCount = 4;
    options.normalTensorMinPersistentScales = 2;
    options.cleanupFeatureGraph = false;

    const auto tensor = manumesh::feature::computeNormalTensorFeatures(
        input,
        {options.normalTensorSmoothingIterations, options.normalTensorScaleCount, {}},
        options.normalTensorFeatureThreshold
    );
    const std::pair<int, int> oneSided = oneSidedAlignedNormalTensorEdge(
        input,
        tensor,
        options.normalTensorFeatureThreshold,
        options.normalTensorMinPersistentScales,
        options.normalTensorMinEdgeAlignment
    );
    ASSERT_GE(oneSided.first, 0);
    ASSERT_GE(oneSided.second, 0);

    const manumesh::feature::FeatureAnalysis analysis = manumesh::feature::detectFeatureCurves(input, options);
    const auto accepted = std::find_if(
        analysis.graph.edges.begin(), analysis.graph.edges.end(), [&](const manumesh::feature::FeatureGraphEdge& edge) {
            return edge.normalTensor && std::min(edge.a, edge.b) == oneSided.first &&
                   std::max(edge.a, edge.b) == oneSided.second;
        }
    );
    EXPECT_EQ(analysis.graph.edges.end(), accepted);
}

TEST(ManuMesh, NormalTensorSoftChainCanTerminateAtHardBoundaryJunction) {
    const manumesh::Mesh input = manumesh::generateRidgeGrid(32, 2.0, 0.6);
    const std::vector<char> boundary = manumesh::common::computeBoundaryVertices(input);
    manumesh::feature::FeatureOptions options;
    options.featureAngleDeg = 180.0;
    options.loopTraceAngleDeg = 180.0;
    options.normalTensorFeatureThreshold = 0.04;
    options.normalTensorMinEdgeAlignment = 0.45;
    options.normalTensorSmoothingIterations = 1;
    options.normalTensorScaleCount = 4;
    options.normalTensorMinPersistentScales = 2;
    options.useSmoothCurvatureFeatures = false;
    options.cleanupFeatureGraph = false;

    const manumesh::feature::FeatureAnalysis analysis = manumesh::feature::detectFeatureCurves(input, options);
    int attachedSoftEdges = 0;
    for (const manumesh::feature::FeatureGraphEdge& edge : analysis.graph.edges) {
        if (!edge.normalTensor || edge.boundary || edge.removedByCleanup) {
            continue;
        }
        if ((boundary[edge.a] != 0) != (boundary[edge.b] != 0)) {
            ++attachedSoftEdges;
        }
    }
    EXPECT_GT(attachedSoftEdges, 0);
}

TEST(ManuMesh, NormalTensorPersistenceIsMonotoneAcrossThresholdsAndBoundedByScaleCount) {
    const manumesh::Mesh input = manumesh::generateRidgeGrid(24, 2.0, 0.5);
    const auto lowThreshold = manumesh::feature::computeNormalTensorFeatures(input, {1, 4, {}}, 0.02);
    const auto highThreshold = manumesh::feature::computeNormalTensorFeatures(input, {1, 4, {}}, 0.08);
    const auto twoScales = manumesh::feature::computeNormalTensorFeatures(input, {1, 2, {}}, 0.04);
    const auto fourScales = manumesh::feature::computeNormalTensorFeatures(input, {1, 4, {}}, 0.04);

    ASSERT_EQ(lowThreshold.size(), highThreshold.size());
    ASSERT_EQ(twoScales.size(), fourScales.size());
    for (std::size_t i = 0; i < lowThreshold.size(); ++i) {
        EXPECT_LE(highThreshold[i].persistentScales, lowThreshold[i].persistentScales) << "vertex=" << i;
        EXPECT_LE(highThreshold[i].persistentFeatureScore, lowThreshold[i].persistentFeatureScore + 1e-12)
            << "vertex=" << i;
        EXPECT_GE(twoScales[i].persistentScales, 0) << "vertex=" << i;
        EXPECT_LE(twoScales[i].persistentScales, 2) << "vertex=" << i;
        EXPECT_GE(fourScales[i].persistentScales, 0) << "vertex=" << i;
        EXPECT_LE(fourScales[i].persistentScales, 4) << "vertex=" << i;
        EXPECT_GE(fourScales[i].featureScore + 1e-12, twoScales[i].featureScore) << "vertex=" << i;
    }
}

TEST(ManuMesh, NormalTensorScaleSelectionIsIndependentOfPersistenceThreshold) {
    const manumesh::Mesh input = manumesh::generateRidgeGrid(24, 2.0, 0.5);
    const manumesh::feature::NormalTensorOptions options{1, 4, {}};
    const auto permissive = manumesh::feature::computeNormalTensorFeatures(input, options, 0.02);
    const auto suppressed = manumesh::feature::computeNormalTensorFeatures(input, options, 1.0);

    ASSERT_EQ(permissive.size(), suppressed.size());
    for (std::size_t i = 0; i < permissive.size(); ++i) {
        EXPECT_TRUE(permissive[i].normal.isApprox(suppressed[i].normal, 0.0)) << "vertex=" << i;
        EXPECT_TRUE(permissive[i].creaseTangent.isApprox(suppressed[i].creaseTangent, 0.0)) << "vertex=" << i;
        EXPECT_DOUBLE_EQ(permissive[i].surfaceSaliency, suppressed[i].surfaceSaliency) << "vertex=" << i;
        EXPECT_DOUBLE_EQ(permissive[i].creaseSaliency, suppressed[i].creaseSaliency) << "vertex=" << i;
        EXPECT_DOUBLE_EQ(permissive[i].cornerSaliency, suppressed[i].cornerSaliency) << "vertex=" << i;
        EXPECT_DOUBLE_EQ(permissive[i].featureScore, suppressed[i].featureScore) << "vertex=" << i;
        EXPECT_DOUBLE_EQ(permissive[i].averageFeatureScore, suppressed[i].averageFeatureScore) << "vertex=" << i;
        EXPECT_DOUBLE_EQ(permissive[i].localScale, suppressed[i].localScale) << "vertex=" << i;
        EXPECT_DOUBLE_EQ(permissive[i].effectiveRadius, suppressed[i].effectiveRadius) << "vertex=" << i;
        EXPECT_EQ(permissive[i].selectedScale, suppressed[i].selectedScale) << "vertex=" << i;
        EXPECT_EQ(permissive[i].smoothingSteps, suppressed[i].smoothingSteps) << "vertex=" << i;
        EXPECT_EQ(0, suppressed[i].persistentScales) << "vertex=" << i;
        EXPECT_DOUBLE_EQ(0.0, suppressed[i].persistentFeatureScore) << "vertex=" << i;
    }
}

TEST(ManuMesh, NormalTensorCornerPersistenceIsRetainedWithoutCreatingCreaseEdges) {
    const manumesh::Mesh input = makeRegularTetrahedronMesh();
    const std::vector<manumesh::feature::NormalTensorVertex> tensor =
        manumesh::feature::computeNormalTensorFeatures(input, {1, 4, {}}, 0.01);

    ASSERT_EQ(input.vertices.size(), tensor.size());
    std::vector<std::size_t> cornerDominantVertices;
    for (std::size_t vertexId = 0; vertexId < tensor.size(); ++vertexId) {
        const manumesh::feature::NormalTensorVertex& vertex = tensor[vertexId];
        if (vertex.cornerSaliency > vertex.creaseSaliency) {
            cornerDominantVertices.push_back(vertexId);
            EXPECT_GT(vertex.persistentScales, 0);
            EXPECT_GT(vertex.persistentFeatureScore, 0.0);
        }
    }
    ASSERT_FALSE(cornerDominantVertices.empty());

    manumesh::feature::FeatureOptions options;
    options.featureAngleDeg = 180.0;
    options.loopTraceAngleDeg = 180.0;
    options.normalTensorFeatureThreshold = 0.01;
    options.normalTensorSmoothingIterations = 1;
    options.normalTensorScaleCount = 4;
    options.normalTensorMinPersistentScales = 1;
    options.cleanupFeatureGraph = false;
    const manumesh::feature::FeatureAnalysis analysis = manumesh::feature::detectFeatureCurves(input, options);

    ASSERT_EQ(input.vertices.size(), analysis.normalTensorVertexWeights.size());
    for (std::size_t vertexId : cornerDominantVertices) {
        EXPECT_GT(analysis.normalTensorVertexWeights[vertexId], 0.0) << "vertex=" << vertexId;
    }
    EXPECT_EQ(0, analysis.normalTensorFeatureEdges);
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
    ASSERT_EQ(input.vertices.size(), features.normalTensorVertexWeights.size());
    EXPECT_GT(
        *std::max_element(features.normalTensorVertexWeights.begin(), features.normalTensorVertexWeights.end()), 0.0
    );
    EXPECT_GT(features.components.size(), 0u);
    EXPECT_GT(features.weakFeatureComponents, 0);
    EXPECT_GT(features.meanFeatureComponentConfidence, 0.0);
}

TEST(ManuMesh, NormalTensorReportsLocalScaleAndPersistentScore) {
    const manumesh::Mesh input = manumesh::generateRidgeGrid(16, 2.0, 0.5);

    const std::vector<manumesh::feature::NormalTensorVertex> tensor =
        manumesh::feature::computeNormalTensorFeatures(input, {1, 3, {}}, 0.04);

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
        manumesh::feature::computeNormalTensorFeatures(input, {1, 3, {}}, 0.04);
    const std::vector<manumesh::feature::NormalTensorVertex> suppressed =
        manumesh::feature::computeNormalTensorFeatures(input, {1, 3, {}}, 1.0);

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

TEST(ManuMesh, NormalTensorWeightModeUsesResolvedFeatureOptions) {
    const manumesh::Mesh input = manumesh::generateRidgeGrid(32, 2.0, 0.6);
    manumesh::simplification::SimplifyOptions options;
    options.weightMode = manumesh::simplification::WeightMode::NormalTensor;
    options.normalTensorFeatureThreshold = 1.0;
    options.normalTensorScaleCount = 1;
    options.normalTensorMinPersistentScales = 1;

    manumesh::feature::FeatureOptions overrideOptions;
    overrideOptions.normalTensorFeatureThreshold = 0.04;
    overrideOptions.normalTensorSmoothingIterations = 1;
    overrideOptions.normalTensorScaleCount = 3;
    overrideOptions.normalTensorMinPersistentScales = 2;
    options.featureOptionsOverride = overrideOptions;

    const manumesh::simplification::FeatureWeightScores scores =
        manumesh::simplification::computeFeatureWeightScores(input, options);

    ASSERT_EQ(input.vertices.size(), scores.values.size());
    EXPECT_GT(*std::max_element(scores.values.begin(), scores.values.end()), 0.04);
    EXPECT_GT(scores.normalTensorScoredVertices, 0);
    EXPECT_GT(scores.meanNormalTensorPersistence, 1.0);
}

TEST(ManuMesh, NormalTensorWeightModeUsesTheResolvedNormalFilter) {
    const manumesh::Mesh input = manumesh::generateRidgeGrid(32, 2.0, 0.6);

    manumesh::feature::FeatureOptions featureOptions;
    featureOptions.featureAngleDeg = 179.0;
    featureOptions.normalTensorFeatureThreshold = 0.04;
    featureOptions.normalTensorSmoothingIterations = 1;
    featureOptions.normalTensorScaleCount = 3;
    featureOptions.normalTensorMinPersistentScales = 2;
    featureOptions.normalFilter.enabled = true;
    featureOptions.normalFilter.iterations = 3;
    featureOptions.normalFilter.angleSigmaDeg = 30.0;
    featureOptions.normalFilter.preserveAngleDeg = 80.0;

    const manumesh::feature::FeatureAnalysis analysis = manumesh::feature::detectFeatureCurves(input, featureOptions);
    ASSERT_EQ(input.vertices.size(), analysis.normalTensorVertexWeights.size());

    manumesh::simplification::SimplifyOptions options;
    options.weightMode = manumesh::simplification::WeightMode::NormalTensor;
    options.featureOptionsOverride = featureOptions;
    const manumesh::simplification::FeatureWeightScores scores =
        manumesh::simplification::computeFeatureWeightScores(input, options);

    EXPECT_EQ(analysis.normalTensorVertexWeights, scores.values);
    EXPECT_EQ(analysis.normalTensorScoredVertices, scores.normalTensorScoredVertices);
    EXPECT_DOUBLE_EQ(analysis.maxNormalTensorPersistentScore, scores.maxNormalTensorPersistentScore);
}

TEST(ManuMesh, PrecomputedNormalTensorWeightsRemainCanonicalDuringSimplification) {
    const manumesh::Mesh input = manumesh::generateRidgeGrid(32, 2.0, 0.6);

    manumesh::feature::FeatureOptions detectionOptions;
    detectionOptions.featureAngleDeg = 179.0;
    detectionOptions.normalTensorFeatureThreshold = 0.04;
    detectionOptions.normalTensorMinEdgeAlignment = 0.2;
    detectionOptions.normalTensorSmoothingIterations = 1;
    detectionOptions.normalTensorScaleCount = 3;
    detectionOptions.normalTensorMinPersistentScales = 2;
    const manumesh::feature::FeatureAnalysis analysis = manumesh::feature::detectFeatureCurves(input, detectionOptions);
    ASSERT_EQ(input.vertices.size(), analysis.normalTensorVertexWeights.size());
    ASSERT_GT(
        *std::max_element(analysis.normalTensorVertexWeights.begin(), analysis.normalTensorVertexWeights.end()), 0.0
    );

    manumesh::simplification::SimplifyOptions options = paperLineQuadricsOptions(0.90);
    options.weightMode = manumesh::simplification::WeightMode::NormalTensor;
    manumesh::feature::FeatureOptions conflictingOptions = detectionOptions;
    conflictingOptions.normalTensorFeatureThreshold = 1.0;
    conflictingOptions.normalTensorSmoothingIterations = 0;
    conflictingOptions.normalTensorScaleCount = 1;
    conflictingOptions.normalTensorMinPersistentScales = 1;
    options.featureOptionsOverride = conflictingOptions;

    const manumesh::simplification::FeatureWeightScores recomputed =
        manumesh::simplification::computeFeatureWeightScores(input, options);
    EXPECT_DOUBLE_EQ(0.0, *std::max_element(recomputed.values.begin(), recomputed.values.end()));

    const manumesh::simplification::FeatureWeightScores reused =
        manumesh::simplification::computeFeatureWeightScores(input, options, &analysis);
    EXPECT_EQ(analysis.normalTensorVertexWeights, reused.values);
    EXPECT_EQ(analysis.normalTensorScoredVertices, reused.normalTensorScoredVertices);
    EXPECT_DOUBLE_EQ(analysis.maxNormalTensorPersistentScore, reused.maxNormalTensorPersistentScore);

    manumesh::simplification::SimplifyReport report;
    const manumesh::Mesh output = manumesh::simplification::simplifyMesh(input, options, analysis, &report);
    EXPECT_LT(output.faces.size(), input.faces.size());
    EXPECT_GT(report.maxAppliedLineWeight, report.minAppliedLineWeight);
    EXPECT_EQ(analysis.normalTensorScoredVertices, report.normalTensorScoredVertices);
    EXPECT_DOUBLE_EQ(analysis.maxNormalTensorPersistentScore, report.maxNormalTensorPersistentScore);
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
