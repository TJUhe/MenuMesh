/**
 * @file tests/unit/feature_detection/feature_detection_fixture_tests.cpp
 * @brief 验证 ManuMesh 测试中的特征检测 夹具测试行为。
 * @ingroup manumesh_tests
 *
 * @details 测试夹具和断言记录可观察契约、数值容差、确定性要求以及已修复的回归问题。
 */

#include "FeatureDetectionTestSupport.h"
#include "TestSupport.h"

#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <vector>

namespace {

namespace feature = manumesh::feature;

using FeatureAnalysis = feature::FeatureAnalysis;
using FeatureLoop = feature::FeatureLoop;
using FeatureOptions = feature::FeatureOptions;
using FeaturePrimitiveType = feature::FeaturePrimitiveType;
using Mesh = manumesh::Mesh;
using Vec3 = manumesh::Vec3;
using VertexFeature = feature::VertexFeature;
using manumesh::test::FeatureLabels;
using manumesh::test::loadFixtureMesh;
using manumesh::test::readFeatureLabels;
using manumesh::test::feature_detection::circularLoopsNearRadius;
using manumesh::test::feature_detection::clusterCoplanarFaces;
using manumesh::test::feature_detection::countLoopsOfType;
using manumesh::test::feature_detection::discreteOnlyOptions;
using manumesh::test::feature_detection::hasPlaneCluster;
using manumesh::test::feature_detection::makeMultiJunctionPolygonalBoundaryMesh;
using manumesh::test::feature_detection::parallelError;
using manumesh::test::feature_detection::PlaneCluster;
using manumesh::test::feature_detection::radialCenterOffsetBetweenLoops;

} // 命名空间

TEST(FeatureDetection, FixtureDetectsCoaxialHoleLoopsAndPlanarFaces) {
    const Mesh mesh = loadFixtureMesh("feature_fixtures/coaxial_hole_plate.obj");
    ASSERT_FALSE(mesh.empty());

    FeatureOptions options = discreteOnlyOptions();
    options.featureAngleDeg = 25.0;
    options.circleFitRelativeThreshold = 0.03;
    options.minFeatureLoopVertices = 16;
    const FeatureAnalysis features = feature::detectFeatureCurves(mesh, options);

    const std::vector<FeatureLoop> innerHoleLoops = circularLoopsNearRadius(features, 0.6, 1e-6);

    ASSERT_EQ(2u, innerHoleLoops.size());
    for (const FeatureLoop& loop : innerHoleLoops) {
        EXPECT_EQ(24, static_cast<int>(loop.vertices.size()));
        EXPECT_NEAR(loop.center.x(), 0.0, 1e-10);
        EXPECT_NEAR(loop.center.y(), 0.0, 1e-10);
        EXPECT_NEAR(std::abs(loop.center.z()), 0.5, 1e-10);
        EXPECT_NEAR(loop.radius, 0.6, 1e-10);
        EXPECT_LT(loop.rmsRadialError, 1e-10);
        EXPECT_LT(loop.rmsPlaneError, 1e-10);
    }

    const Vec3 axisA = innerHoleLoops[0].normal.normalized();
    const Vec3 axisB = innerHoleLoops[1].normal.normalized();
    const double coaxialAngleError = 1.0 - std::abs(axisA.dot(axisB));
    const double radialCenterOffset = radialCenterOffsetBetweenLoops(innerHoleLoops[0], innerHoleLoops[1]);
    EXPECT_LT(coaxialAngleError, 1e-12);
    EXPECT_LT(radialCenterOffset, 1e-10);

    const std::vector<PlaneCluster> planes = clusterCoplanarFaces(mesh, 1.0 - 1e-12, 1e-10);
    const auto hasLargeHorizontalPlane = [&](double z) {
        return std::any_of(planes.begin(), planes.end(), [&](const PlaneCluster& plane) {
            return std::abs(std::abs(plane.normal.z()) - 1.0) < 1e-12 &&
                   std::abs(plane.offset - plane.normal.z() * z) < 1e-10 && plane.area > 10.0;
        });
    };
    EXPECT_TRUE(hasLargeHorizontalPlane(0.5));
    EXPECT_TRUE(hasLargeHorizontalPlane(-0.5));
}

TEST(FeatureDetection, FixtureBenchmarkUsesCoaxialHoleGroundTruthLabels) {
    const Mesh mesh = loadFixtureMesh("feature_fixtures/coaxial_hole_plate.obj");
    const FeatureLabels labels = readFeatureLabels("feature_labels/coaxial_hole_plate_inner_top_edges.csv");
    ASSERT_FALSE(mesh.empty());
    ASSERT_EQ(24u, labels.edges.size());

    FeatureOptions options = discreteOnlyOptions();
    options.featureAngleDeg = 25.0;
    options.circleFitRelativeThreshold = 0.03;
    options.minFeatureLoopVertices = 16;
    const FeatureAnalysis features = feature::detectFeatureCurves(mesh, options);
    const feature::FeatureEdgeBenchmark benchmark =
        feature::benchmarkFeatureEdges(features, labels.edges, labels.junctions);

    EXPECT_EQ(24, benchmark.groundTruthEdges);
    EXPECT_EQ(24, benchmark.truePositiveEdges);
    EXPECT_EQ(0, benchmark.falseNegativeEdges);
    EXPECT_DOUBLE_EQ(1.0, benchmark.edgeRecall);
    // 检查该步骤的边界条件，并确保结果保持确定性。
    // 检查该步骤的边界条件，并确保结果保持确定性。
    // 检查该步骤的边界条件，并确保结果保持确定性。
    // 检查该步骤的边界条件，并确保结果保持确定性。
    // 检查该步骤的边界条件，并确保结果保持确定性。
    // 检查该步骤的边界条件，并确保结果保持确定性。
    EXPECT_GE(benchmark.edgePrecision, 0.20);
    EXPECT_GT(benchmark.loopClosureRate, 0.95);
}

TEST(FeatureDetection, FixtureBenchmarkUsesEllipticalHoleGroundTruthLabels) {
    const Mesh mesh = loadFixtureMesh("feature_fixtures/elliptical_hole_plate.obj");
    const FeatureLabels labels = readFeatureLabels("feature_labels/elliptical_hole_plate_inner_top_edges.csv");
    ASSERT_FALSE(mesh.empty());
    ASSERT_EQ(40u, labels.edges.size());

    FeatureOptions options = discreteOnlyOptions();
    options.featureAngleDeg = 25.0;
    options.circleFitRelativeThreshold = 0.03;
    options.ellipseFitRelativeThreshold = 0.03;
    options.minFeatureLoopVertices = 16;
    const FeatureAnalysis features = feature::detectFeatureCurves(mesh, options);
    const feature::FeatureEdgeBenchmark benchmark =
        feature::benchmarkFeatureEdges(features, labels.edges, labels.junctions);

    EXPECT_EQ(40, benchmark.truePositiveEdges);
    EXPECT_EQ(0, benchmark.falseNegativeEdges);
    EXPECT_DOUBLE_EQ(1.0, benchmark.edgeRecall);
    // 检查该步骤的边界条件，并确保结果保持确定性。
    // 检查该步骤的边界条件，并确保结果保持确定性。
    // 检查该步骤的边界条件，并确保结果保持确定性。
    // 检查该步骤的边界条件，并确保结果保持确定性。
    // 检查该步骤的边界条件，并确保结果保持确定性。
    EXPECT_GE(benchmark.edgePrecision, 0.20);
    EXPECT_GT(benchmark.loopClosureRate, 0.95);
}

TEST(FeatureDetection, FixtureBenchmarkUsesBossAndPocketGroundTruthLabels) {
    const Mesh mesh = loadFixtureMesh("feature_fixtures/boss_pocket_plate.obj");
    const FeatureLabels labels = readFeatureLabels("feature_labels/boss_pocket_primary_edges.csv");
    ASSERT_FALSE(mesh.empty());
    ASSERT_EQ(60u, labels.edges.size());

    FeatureOptions options = discreteOnlyOptions();
    options.featureAngleDeg = 25.0;
    options.minFeatureLoopVertices = 4;
    const FeatureAnalysis features = feature::detectFeatureCurves(mesh, options);
    const feature::FeatureEdgeBenchmark benchmark =
        feature::benchmarkFeatureEdges(features, labels.edges, labels.junctions);

    EXPECT_EQ(60, benchmark.truePositiveEdges);
    EXPECT_EQ(0, benchmark.falseNegativeEdges);
    EXPECT_DOUBLE_EQ(1.0, benchmark.edgeRecall);
    EXPECT_GE(benchmark.edgePrecision, 0.90);
    EXPECT_GT(benchmark.loopClosureRate, 0.95);
}

TEST(FeatureDetection, SyntheticBenchmarkUsesPolygonAndJunctionGroundTruthLabels) {
    const Mesh mesh = makeMultiJunctionPolygonalBoundaryMesh();
    const FeatureLabels labels = readFeatureLabels("feature_labels/multi_junction_polygon_edges.csv");
    ASSERT_EQ(8u, labels.edges.size());
    ASSERT_EQ(3u, labels.junctions.size());

    FeatureOptions options = discreteOnlyOptions();
    options.minFeatureLoopVertices = 8;
    options.circleFitRelativeThreshold = 0.005;
    options.ellipseFitRelativeThreshold = 0.005;
    const FeatureAnalysis features = feature::detectFeatureCurves(mesh, options);
    const feature::FeatureEdgeBenchmark benchmark =
        feature::benchmarkFeatureEdges(features, labels.edges, labels.junctions);

    EXPECT_EQ(8, benchmark.truePositiveEdges);
    EXPECT_EQ(0, benchmark.falseNegativeEdges);
    EXPECT_DOUBLE_EQ(1.0, benchmark.edgeRecall);
    EXPECT_EQ(3, benchmark.truePositiveJunctions);
    EXPECT_EQ(0, benchmark.falseNegativeJunctions);
    EXPECT_DOUBLE_EQ(1.0, benchmark.junctionRecall);
    EXPECT_GT(benchmark.loopClosureRate, 0.95);
}

TEST(FeatureDetection, FixtureDetectsTiltedCoaxialHoleAxis) {
    const Mesh mesh = loadFixtureMesh("feature_fixtures/tilted_coaxial_hole_plate.obj");
    ASSERT_FALSE(mesh.empty());

    FeatureOptions options = discreteOnlyOptions();
    options.featureAngleDeg = 25.0;
    options.circleFitRelativeThreshold = 0.03;
    options.minFeatureLoopVertices = 16;
    const FeatureAnalysis features = feature::detectFeatureCurves(mesh, options);

    const std::vector<FeatureLoop> holeLoops = circularLoopsNearRadius(features, 0.5, 1e-6);
    ASSERT_EQ(2u, holeLoops.size());

    const Vec3 expectedAxis(0.35, 0.2, 1.0);
    const Vec3 centerDelta = holeLoops[1].center - holeLoops[0].center;
    EXPECT_LT(parallelError(centerDelta, expectedAxis), 1e-12);
    EXPECT_LT(parallelError(holeLoops[0].normal, expectedAxis), 1e-12);
    EXPECT_LT(parallelError(holeLoops[1].normal, expectedAxis), 1e-12);
    EXPECT_LT(radialCenterOffsetBetweenLoops(holeLoops[0], holeLoops[1]), 1e-10);
}

TEST(FeatureDetection, FixtureExposesEccentricHoleNonCoaxiality) {
    const Mesh mesh = loadFixtureMesh("feature_fixtures/eccentric_hole_plate.obj");
    ASSERT_FALSE(mesh.empty());

    FeatureOptions options = discreteOnlyOptions();
    options.featureAngleDeg = 25.0;
    options.circleFitRelativeThreshold = 0.03;
    options.minFeatureLoopVertices = 16;
    const FeatureAnalysis features = feature::detectFeatureCurves(mesh, options);

    const std::vector<FeatureLoop> holeLoops = circularLoopsNearRadius(features, 0.55, 1e-6);
    ASSERT_EQ(2u, holeLoops.size());
    EXPECT_NEAR(holeLoops[0].radius, 0.55, 1e-10);
    EXPECT_NEAR(holeLoops[1].radius, 0.55, 1e-10);
    EXPECT_GT(radialCenterOffsetBetweenLoops(holeLoops[0], holeLoops[1]), 0.25);
}

TEST(FeatureDetection, FixtureClassifiesEllipseAndNearCircleLoops) {
    FeatureOptions options = discreteOnlyOptions();
    options.featureAngleDeg = 25.0;
    options.circleFitRelativeThreshold = 0.03;
    options.ellipseFitRelativeThreshold = 0.03;
    options.nearCircleAxisRatioTolerance = 0.08;
    options.minFeatureLoopVertices = 16;

    const Mesh ellipseMesh = loadFixtureMesh("feature_fixtures/elliptical_hole_plate.obj");
    ASSERT_FALSE(ellipseMesh.empty());
    const FeatureAnalysis ellipseFeatures = feature::detectFeatureCurves(ellipseMesh, options);
    EXPECT_GE(countLoopsOfType(ellipseFeatures, FeaturePrimitiveType::Ellipse), 2);
    const auto ellipseIt =
        std::find_if(ellipseFeatures.loops.begin(), ellipseFeatures.loops.end(), [](const FeatureLoop& loop) {
            return loop.primitive == FeaturePrimitiveType::Ellipse && loop.majorRadius < 1.0;
        });
    ASSERT_NE(ellipseIt, ellipseFeatures.loops.end());
    EXPECT_NEAR(ellipseIt->majorRadius, 0.8, 1e-10);
    EXPECT_NEAR(ellipseIt->minorRadius, 0.45, 1e-10);
    EXPECT_LT(ellipseIt->axisRatio, 0.65);
    EXPECT_LT(ellipseIt->rmsEllipseError, 1e-10);
    ASSERT_FALSE(ellipseIt->vertices.empty());
    const VertexFeature& ellipseVertex = ellipseFeatures.vertices[ellipseIt->vertices.front()];
    EXPECT_EQ(FeaturePrimitiveType::Ellipse, ellipseVertex.primitive);
    EXPECT_NEAR(ellipseVertex.ellipseMajorRadius, 0.8, 1e-10);
    EXPECT_NEAR(ellipseVertex.ellipseMinorRadius, 0.45, 1e-10);
    EXPECT_GT(ellipseVertex.tangent.norm(), 0.9);

    const Mesh nearCircleMesh = loadFixtureMesh("feature_fixtures/near_circular_hole_plate.obj");
    ASSERT_FALSE(nearCircleMesh.empty());
    const FeatureAnalysis nearCircleFeatures = feature::detectFeatureCurves(nearCircleMesh, options);
    const int innerNearCircleLoops = static_cast<int>(
        std::count_if(nearCircleFeatures.loops.begin(), nearCircleFeatures.loops.end(), [](const FeatureLoop& loop) {
            return loop.primitive == FeaturePrimitiveType::NearCircle && loop.majorRadius < 1.0;
        })
    );
    EXPECT_GE(innerNearCircleLoops, 2);
}

TEST(FeatureDetection, FixtureDetectsBossPocketPlanesAndHardEdges) {
    const Mesh mesh = loadFixtureMesh("feature_fixtures/boss_pocket_plate.obj");
    ASSERT_FALSE(mesh.empty());

    FeatureOptions options = discreteOnlyOptions();
    options.featureAngleDeg = 25.0;
    options.minFeatureLoopVertices = 4;
    const FeatureAnalysis features = feature::detectFeatureCurves(mesh, options);

    EXPECT_EQ(0, features.boundaryFeatureEdges);
    EXPECT_EQ(0, features.inconsistentWindingEdges);
    // 检查该步骤的边界条件，并确保结果保持确定性。
    // 检查该步骤的边界条件，并确保结果保持确定性。
    // 检查该步骤的边界条件，并确保结果保持确定性。
    // 检查该步骤的边界条件，并确保结果保持确定性。
    // 检查该步骤的边界条件，并确保结果保持确定性。
    // 检查该步骤的边界条件，并确保结果保持确定性。
    // 检查该步骤的边界条件，并确保结果保持确定性。
    // 60 - 12 = 48.
    EXPECT_EQ(60, features.dihedralFeatureEdges);
    EXPECT_EQ(48, features.convexFeatureEdges);
    EXPECT_EQ(12, features.concaveFeatureEdges);
    EXPECT_EQ(0, features.unknownSignedFeatureEdges);
    EXPECT_GT(features.loops.size(), 0u);
    EXPECT_TRUE(std::any_of(features.loops.begin(), features.loops.end(), [](const FeatureLoop& loop) {
        return loop.convexEdges > 0;
    }));
    EXPECT_TRUE(std::any_of(features.loops.begin(), features.loops.end(), [](const FeatureLoop& loop) {
        return loop.concaveEdges > 0;
    }));

    // 检查该步骤的边界条件，并确保结果保持确定性。
    // 检查该步骤的边界条件，并确保结果保持确定性。
    // 检查该步骤的边界条件，并确保结果保持确定性。
    // 检查该步骤的边界条件，并确保结果保持确定性。
    const auto approx = [](double value, double target) {
        return std::abs(value - target) < 1e-9;
    };
    const auto isConcaveTruthEdge = [&](const Vec3& pa, const Vec3& pb) {
        // 检查该步骤的边界条件，并确保结果保持确定性。
        if (approx(pa.z(), -0.25) && approx(pb.z(), -0.25)) {
            return true;
        }
        // 命名空间
        // 检查该步骤的边界条件，并确保结果保持确定性。
        if (approx(pa.x(), pb.x()) && approx(pa.y(), pb.y()) && (approx(pa.x(), 0.25) || approx(pa.x(), 0.85)) &&
            approx(std::abs(pa.y()), 0.35)) {
            return true;
        }
        // 命名空间
        // 检查该步骤的边界条件，并确保结果保持确定性。
        if (approx(pa.z(), 0.2) && approx(pb.z(), 0.2) && pa.x() >= -0.8 - 1e-9 && pa.x() <= -0.2 + 1e-9 &&
            pb.x() >= -0.8 - 1e-9 && pb.x() <= -0.2 + 1e-9 && std::abs(pa.y()) <= 0.35 + 1e-9 &&
            std::abs(pb.y()) <= 0.35 + 1e-9) {
            return true;
        }
        return false;
    };
    for (const feature::FeatureGraphEdge& edge : features.graph.edges) {
        if (!edge.dihedral) {
            continue;
        }
        const bool truthConcave = isConcaveTruthEdge(mesh.vertices[edge.a], mesh.vertices[edge.b]);
        EXPECT_EQ(truthConcave ? -1 : 1, edge.signedKind) << "edge (" << edge.a << ", " << edge.b << ") misclassified";
    }

    const std::vector<PlaneCluster> planes = clusterCoplanarFaces(mesh, 1.0 - 1e-12, 1e-10);
    EXPECT_TRUE(hasPlaneCluster(planes, Vec3(0.0, 0.0, 1.0), 0.7, 0.35));
    EXPECT_TRUE(hasPlaneCluster(planes, Vec3(0.0, 0.0, 1.0), -0.25, 0.35));
    EXPECT_TRUE(hasPlaneCluster(planes, Vec3(0.0, 0.0, 1.0), 0.2, 2.0));
}
