/**
 * @file tests/unit/feature_detection/feature_detection_discrete_tests.cpp
 * @brief 验证边界、非流形边和带符号二面角等离散特征证据。
 * @ingroup manumesh_tests
 */

#include "FeatureDetectionTestSupport.h"

#include <algorithm>
#include <array>
#include <gtest/gtest.h>
#include <set>
#include <utility>
namespace {

namespace feature = manumesh::feature;

using FeatureAnalysis = feature::FeatureAnalysis;
using FeatureOptions = feature::FeatureOptions;
using Mesh = manumesh::Mesh;
using Vec3 = manumesh::Vec3;
using VertexFeature = feature::VertexFeature;
using manumesh::test::feature_detection::countClosedLoops;
using manumesh::test::feature_detection::discreteOnlyOptions;
using manumesh::test::feature_detection::makeMixedDiscreteEvidenceMesh;

/// (n0 x n1) . d = (z x -x) . y = -1.
Mesh makeRightAngleFoldMesh(double wallZ) {
    Mesh mesh;
    mesh.vertices = {
        Vec3(0.0, 0.0, 0.0),
        Vec3(0.0, 1.0, 0.0),
        Vec3(-1.0, 0.5, 0.0),
        Vec3(0.0, 0.5, wallZ),
    };
    mesh.faces = {
        {{0, 1, 2}},
        {{1, 0, 3}},
    };
    return mesh;
}

Mesh makeLShapedPrismMesh() {
    const std::array<std::pair<double, double>, 6> polygon = {
        std::make_pair(0.0, 0.0),
        std::make_pair(2.0, 0.0),
        std::make_pair(2.0, 1.0),
        std::make_pair(1.0, 1.0),
        std::make_pair(1.0, 2.0),
        std::make_pair(0.0, 2.0),
    };
    Mesh mesh;
    for (const auto& pairEntry : polygon) {
        const double x = pairEntry.first;
        const double y = pairEntry.second;
        mesh.vertices.emplace_back(x, y, 0.0);
    }
    for (const auto& pairEntry : polygon) {
        const double x = pairEntry.first;
        const double y = pairEntry.second;
        mesh.vertices.emplace_back(x, y, 1.0);
    }
    mesh.faces = {
        {{3, 5, 4}},
        {{3, 0, 5}},
        {{3, 1, 0}},
        {{3, 2, 1}},
        {{9, 10, 11}},
        {{9, 11, 6}},
        {{9, 6, 7}},
        {{9, 7, 8}},
    };
    for (int i = 0; i < 6; ++i) {
        const int j = (i + 1) % 6;
        mesh.faces.push_back({{i, j, j + 6}});
        mesh.faces.push_back({{i, j + 6, i + 6}});
    }
    return mesh;
}

Mesh makeStaircaseSheetMesh() {
    const std::array<std::pair<double, double>, 6> profile = {
        std::make_pair(0.0, 0.0),
        std::make_pair(1.0, 0.0),
        std::make_pair(1.0, 1.0),
        std::make_pair(2.0, 1.0),
        std::make_pair(2.0, 2.0),
        std::make_pair(3.0, 2.0),
    };
    Mesh mesh;
    for (const auto& pairEntry : profile) {
        const double x = pairEntry.first;
        const double z = pairEntry.second;
        mesh.vertices.emplace_back(x, 0.0, z);
    }
    for (const auto& pairEntry : profile) {
        const double x = pairEntry.first;
        const double z = pairEntry.second;
        mesh.vertices.emplace_back(x, 1.0, z);
    }
    for (int i = 0; i < 5; ++i) {
        mesh.faces.push_back({{i, i + 1, i + 7}});
        mesh.faces.push_back({{i, i + 7, i + 6}});
    }
    return mesh;
}

std::set<std::pair<std::pair<int, int>, int>> dihedralSignedEdges(const FeatureAnalysis& features) {
    std::set<std::pair<std::pair<int, int>, int>> result;
    for (const feature::FeatureGraphEdge& edge : features.graph.edges) {
        if (edge.dihedral) {
            result.insert({{edge.a, edge.b}, edge.signedKind});
        }
    }
    return result;
}

} // 命名空间

TEST(FeatureDetection, ClassifiesBoundaryEdgesOnOpenTriangle) {
    Mesh mesh;
    mesh.vertices = {
        Vec3(0.0, 0.0, 0.0),
        Vec3(1.0, 0.0, 0.0),
        Vec3(0.0, 1.0, 0.0),
    };
    mesh.faces = {{{0, 1, 2}}};

    const FeatureAnalysis features = feature::detectFeatureCurves(mesh, discreteOnlyOptions());

    EXPECT_EQ(3, features.featureEdges);
    EXPECT_EQ(3, features.boundaryFeatureEdges);
    EXPECT_EQ(0, features.dihedralFeatureEdges);
    EXPECT_EQ(0, features.nonManifoldFeatureEdges);
    EXPECT_EQ(1, countClosedLoops(features));
    ASSERT_EQ(mesh.vertices.size(), features.vertices.size());
    for (const VertexFeature& vertex : features.vertices) {
        EXPECT_TRUE(vertex.isFeature);
    }
    ASSERT_EQ(1u, features.components.size());
    EXPECT_GT(features.components.front().confidence, 0.70);
    EXPECT_EQ(1, features.highConfidenceFeatureComponents);
    EXPECT_GT(features.meanFeatureComponentConfidence, 0.70);
    ASSERT_FALSE(features.loops.empty());
    EXPECT_EQ(0, features.loops.front().componentId);
    EXPECT_GT(features.loops.front().componentConfidence, 0.70);
}

TEST(FeatureDetection, BenchmarksDetectedEdgesAgainstGroundTruthLabels) {
    Mesh mesh;
    mesh.vertices = {
        Vec3(0.0, 0.0, 0.0),
        Vec3(1.0, 0.0, 0.0),
        Vec3(0.0, 1.0, 0.0),
    };
    mesh.faces = {{{0, 1, 2}}};

    const FeatureAnalysis features = feature::detectFeatureCurves(mesh, discreteOnlyOptions());
    const feature::FeatureEdgeBenchmark benchmark = feature::benchmarkFeatureEdges(features, {{0, 1}, {1, 2}});

    EXPECT_EQ(2, benchmark.groundTruthEdges);
    EXPECT_EQ(3, benchmark.detectedEdges);
    EXPECT_EQ(2, benchmark.truePositiveEdges);
    EXPECT_EQ(1, benchmark.falsePositiveEdges);
    EXPECT_EQ(0, benchmark.falseNegativeEdges);
    EXPECT_NEAR(2.0 / 3.0, benchmark.edgePrecision, 1e-12);
    EXPECT_DOUBLE_EQ(1.0, benchmark.edgeRecall);
    EXPECT_GT(benchmark.loopClosureRate, 0.9);
    EXPECT_GT(benchmark.meanComponentConfidence, 0.70);
}

TEST(FeatureDetection, ClassifiesNonManifoldEdgesSeparately) {
    Mesh mesh;
    mesh.vertices = {
        Vec3(0.0, 0.0, 0.0),
        Vec3(1.0, 0.0, 0.0),
        Vec3(0.0, 1.0, 0.0),
        Vec3(0.0, 0.0, 1.0),
        Vec3(0.0, 0.0, -1.0),
    };
    mesh.faces = {
        {{0, 1, 2}},
        {{1, 0, 3}},
        {{0, 1, 4}},
    };

    const FeatureAnalysis features = feature::detectFeatureCurves(mesh, discreteOnlyOptions());

    EXPECT_EQ(1, features.nonManifoldFeatureEdges);
    EXPECT_GT(features.boundaryFeatureEdges, 0);
    EXPECT_GE(features.featureEdges, features.nonManifoldFeatureEdges);
    EXPECT_TRUE(features.vertices[0].isFeature);
    EXPECT_TRUE(features.vertices[1].isFeature);
}

TEST(FeatureDetection, DihedralThresholdControlsCreaseEdges) {
    Mesh mesh;
    mesh.vertices = {
        Vec3(0.0, 0.0, 0.0),
        Vec3(1.0, 0.0, 0.0),
        Vec3(0.0, 1.0, 0.0),
        Vec3(0.0, 0.0, 1.0),
    };
    mesh.faces = {
        {{0, 1, 2}},
        {{1, 0, 3}},
    };

    FeatureOptions strict = discreteOnlyOptions();
    strict.featureAngleDeg = 120.0;
    const FeatureAnalysis strictFeatures = feature::detectFeatureCurves(mesh, strict);

    FeatureOptions permissive = discreteOnlyOptions();
    permissive.featureAngleDeg = 30.0;
    const FeatureAnalysis permissiveFeatures = feature::detectFeatureCurves(mesh, permissive);

    EXPECT_EQ(0, strictFeatures.dihedralFeatureEdges);
    EXPECT_EQ(1, permissiveFeatures.dihedralFeatureEdges);
    EXPECT_GT(permissiveFeatures.featureEdges, strictFeatures.featureEdges);
}

TEST(FeatureDetection, ComposesDiscreteEvidenceSourceCounters) {
    const Mesh mesh = makeMixedDiscreteEvidenceMesh();

    FeatureOptions options = discreteOnlyOptions();
    options.featureAngleDeg = 30.0;
    const FeatureAnalysis features = feature::detectFeatureCurves(mesh, options);

    EXPECT_EQ(0, features.normalTensorFeatureEdges);
    EXPECT_EQ(1, features.nonManifoldFeatureEdges);
    EXPECT_EQ(1, features.dihedralFeatureEdges);
    EXPECT_GT(features.boundaryFeatureEdges, 0);
    EXPECT_EQ(features.featureEdges, static_cast<int>(features.graph.edges.size()));
    EXPECT_EQ(
        features.featureEdges,
        features.boundaryFeatureEdges + features.dihedralFeatureEdges + features.normalTensorFeatureEdges +
            features.nonManifoldFeatureEdges
    );
}

TEST(FeatureDetection, SplitsBranchedFeatureGraphAndMarksJunctions) {
    Mesh mesh;
    mesh.vertices = {
        Vec3(0.0, 0.0, 0.0),
        Vec3(1.0, 0.0, 0.0),
        Vec3(0.5, 1.0, 0.0),
        Vec3(-1.0, 0.0, 0.0),
        Vec3(-0.5, 1.0, 0.0),
        Vec3(0.0, -1.0, 0.0),
        Vec3(1.0, -1.0, 0.0),
    };
    mesh.faces = {
        {{0, 1, 2}},
        {{0, 3, 4}},
        {{0, 5, 6}},
    };

    const FeatureAnalysis features = feature::detectFeatureCurves(mesh, discreteOnlyOptions());

    EXPECT_GT(features.loops.size(), 1u);
    EXPECT_EQ(features.featureEdges, static_cast<int>(features.graph.edges.size()));
    EXPECT_EQ(mesh.vertices.size(), features.graph.vertices.size());
    EXPECT_FALSE(features.graph.junctionVertices.empty());
    ASSERT_LT(0u, features.vertices.size());
    EXPECT_TRUE(features.vertices[0].junction);
    EXPECT_TRUE(features.graph.vertices[0].junction);
}


TEST(FeatureDetection, ClassifiesRightAngleConvexRidgePerEdge) {
    const Mesh ridge = makeRightAngleFoldMesh(-1.0);

    FeatureOptions options = discreteOnlyOptions();
    options.featureAngleDeg = 40.0;
    const FeatureAnalysis features = feature::detectFeatureCurves(ridge, options);

    EXPECT_EQ(1, features.dihedralFeatureEdges);
    EXPECT_EQ(1, features.convexFeatureEdges);
    EXPECT_EQ(0, features.concaveFeatureEdges);
    EXPECT_EQ(0, features.unknownSignedFeatureEdges);
    EXPECT_EQ(0, features.inconsistentWindingEdges);
    const std::set<std::pair<std::pair<int, int>, int>> expected = {{{0, 1}, 1}};
    EXPECT_EQ(expected, dihedralSignedEdges(features));
}

TEST(FeatureDetection, ClassifiesRightAngleConcaveValleyPerEdge) {
    const Mesh valley = makeRightAngleFoldMesh(1.0);

    FeatureOptions options = discreteOnlyOptions();
    options.featureAngleDeg = 40.0;
    const FeatureAnalysis features = feature::detectFeatureCurves(valley, options);

    EXPECT_EQ(1, features.dihedralFeatureEdges);
    EXPECT_EQ(0, features.convexFeatureEdges);
    EXPECT_EQ(1, features.concaveFeatureEdges);
    EXPECT_EQ(0, features.unknownSignedFeatureEdges);
    EXPECT_EQ(0, features.inconsistentWindingEdges);
    const std::set<std::pair<std::pair<int, int>, int>> expected = {{{0, 1}, -1}};
    EXPECT_EQ(expected, dihedralSignedEdges(features));
}

TEST(FeatureDetection, ClassifiesLShapedPrismConvexityPerEdge) {
    const Mesh mesh = makeLShapedPrismMesh();

    FeatureOptions options = discreteOnlyOptions();
    options.featureAngleDeg = 40.0;
    const FeatureAnalysis features = feature::detectFeatureCurves(mesh, options);

    EXPECT_EQ(0, features.boundaryFeatureEdges);
    EXPECT_EQ(0, features.inconsistentWindingEdges);
    EXPECT_EQ(18, features.dihedralFeatureEdges);
    EXPECT_EQ(17, features.convexFeatureEdges);
    EXPECT_EQ(1, features.concaveFeatureEdges);
    EXPECT_EQ(0, features.unknownSignedFeatureEdges);

    const std::set<std::pair<std::pair<int, int>, int>> expected = {
        {{0, 1}, 1},
        {{1, 2}, 1},
        {{2, 3}, 1},
        {{3, 4}, 1},
        {{4, 5}, 1},
        {{0, 5}, 1},
        {{6, 7}, 1},
        {{7, 8}, 1},
        {{8, 9}, 1},
        {{9, 10}, 1},
        {{10, 11}, 1},
        {{6, 11}, 1},
        {{0, 6}, 1},
        {{1, 7}, 1},
        {{2, 8}, 1},
        {{3, 9}, -1},
        {{4, 10}, 1},
        {{5, 11}, 1},
    };
    EXPECT_EQ(expected, dihedralSignedEdges(features));
}

TEST(FeatureDetection, ClassifiesStaircaseFoldsPerEdge) {
    const Mesh mesh = makeStaircaseSheetMesh();

    FeatureOptions options = discreteOnlyOptions();
    options.featureAngleDeg = 40.0;
    const FeatureAnalysis features = feature::detectFeatureCurves(mesh, options);

    EXPECT_EQ(0, features.inconsistentWindingEdges);
    EXPECT_EQ(4, features.dihedralFeatureEdges);
    EXPECT_EQ(2, features.convexFeatureEdges);
    EXPECT_EQ(2, features.concaveFeatureEdges);
    EXPECT_EQ(0, features.unknownSignedFeatureEdges);

    const std::set<std::pair<std::pair<int, int>, int>> expected = {
        {{1, 7}, -1},
        {{2, 8}, 1},
        {{3, 9}, -1},
        {{4, 10}, 1},
    };
    EXPECT_EQ(expected, dihedralSignedEdges(features));
}
