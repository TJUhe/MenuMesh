/**
 * @file tests/unit/feature_detection/feature_detection_analytic_tests.cpp
 * @brief 在解析曲面上验证硬边恢复和尺度不变性。
 * @ingroup manumesh_tests
 */

//
//  该实现需保持边界条件，并保证结果具有确定性。
//    该实现需保持边界条件，并保证结果具有确定性。
//    该实现需保持边界条件，并保证结果具有确定性。
//  该实现需保持边界条件，并保证结果具有确定性。
//    该实现需保持边界条件，并保证结果具有确定性。
//  该实现需保持边界条件，并保证结果具有确定性。
//    该实现需保持边界条件，并保证结果具有确定性。
//    该实现需保持边界条件，并保证结果具有确定性。
#include "AnalyticFixtures.h"
#include "FeatureDetectionTestSupport.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace manumesh {
namespace test {
namespace feature_detection {
namespace {

namespace analytic = manumesh::test::analytic;

FeatureOptions rimDetectionOptions(double circleFitRelativeThreshold = 0.05) {
    FeatureOptions options = discreteOnlyOptions();
    options.featureAngleDeg = 40.0;
    options.minFeatureLoopVertices = 8;
    options.circleFitRelativeThreshold = circleFitRelativeThreshold;
    return options;
}

int countCircular(const FeatureAnalysis& analysis) {
    return static_cast<int>(std::count_if(analysis.loops.begin(), analysis.loops.end(), [](const FeatureLoop& loop) {
        return loop.circular;
    }));
}

const FeatureLoop* circularLoopNearestTo(const FeatureAnalysis& analysis, const Vec3& center) {
    const FeatureLoop* best = nullptr;
    double bestDistance = 0.0;
    for (const FeatureLoop& loop : analysis.loops) {
        if (!loop.circular) {
            continue;
        }
        const double distance = (loop.center - center).norm();
        if (best == nullptr || distance < bestDistance) {
            best = &loop;
            bestDistance = distance;
        }
    }
    return best;
}

std::vector<std::pair<int, int>> activeGraphEdges(const FeatureAnalysis& analysis) {
    std::vector<std::pair<int, int>> edges;
    for (const feature::FeatureGraphEdge& edge : analysis.graph.edges) {
        if (edge.removedByCleanup || edge.a < 0 || edge.b < 0) {
            continue;
        }
        edges.emplace_back(std::min(edge.a, edge.b), std::max(edge.a, edge.b));
    }
    std::sort(edges.begin(), edges.end());
    return edges;
}

} // namespace

TEST(FeatureDetectionAnalytic, CappedCylinderRimsRecoverAsTwoExactCircles) {
    const analytic::CylinderFixture cylinder = analytic::makeCylinder(64, 6, 1.0, 2.0, true);
    const FeatureAnalysis analysis = feature::detectFeatureCurves(cylinder.mesh, rimDetectionOptions());

    ASSERT_EQ(2, countCircular(analysis));
    for (const analytic::GroundTruthCircle& truth : cylinder.groundTruthCircles()) {
        SCOPED_TRACE(truth.center.z());
        const FeatureLoop* loop = circularLoopNearestTo(analysis, truth.center);
        ASSERT_NE(nullptr, loop);
        EXPECT_TRUE(loop->closed);
        EXPECT_EQ(cylinder.segments, loop->edgeCount);
        EXPECT_NEAR(truth.radius, loop->radius, 1e-7 * truth.radius);
        EXPECT_LT((loop->center - truth.center).norm(), 1e-7 * truth.radius);
        EXPECT_GT(std::abs(loop->normal.dot(truth.normal)), 1.0 - 1e-9);
    }
}

TEST(FeatureDetectionAnalytic, ChamferBoxHardEdgesRecoverWithHighPrecisionAndRecall) {
    const analytic::ChamferBoxFixture box = analytic::makeChamferBox(2.0, 0.3, 6);
    FeatureOptions options = discreteOnlyOptions();
    options.featureAngleDeg = 30.0;
    options.minFeatureLoopVertices = 8;
    const FeatureAnalysis analysis = feature::detectFeatureCurves(box.mesh, options);

    std::vector<int> junctions;
    for (int corner = 0; corner < 8; ++corner) {
        junctions.push_back(corner);
        junctions.push_back(box.divisions * 8 + corner);
    }
    const feature::FeatureEdgeBenchmark benchmark =
        feature::benchmarkFeatureEdges(analysis, box.groundTruthHardEdges(), junctions);

    EXPECT_GT(benchmark.groundTruthEdges, 0);
    EXPECT_GE(benchmark.edgeRecall, 0.99);
    EXPECT_GE(benchmark.edgePrecision, 0.99);
    EXPECT_GE(benchmark.junctionRecall, 0.99);
}

TEST(FeatureDetectionAnalytic, ChamferBoxJunctionPrecisionIsNotFloodedByCircularRecovery) {
    const analytic::ChamferBoxFixture box = analytic::makeChamferBox(2.0, 0.3, 6);
    FeatureOptions options = discreteOnlyOptions();
    options.featureAngleDeg = 30.0;
    options.minFeatureLoopVertices = 8;
    const FeatureAnalysis analysis = feature::detectFeatureCurves(box.mesh, options);

    std::vector<int> junctions;
    for (int corner = 0; corner < 8; ++corner) {
        junctions.push_back(corner);
        junctions.push_back(box.divisions * 8 + corner);
    }
    const feature::FeatureEdgeBenchmark benchmark =
        feature::benchmarkFeatureEdges(analysis, box.groundTruthHardEdges(), junctions);
    EXPECT_GE(benchmark.junctionPrecision, 0.99);
}

//  该实现需保持边界条件，并保证结果具有确定性。
//    该实现需保持边界条件，并保证结果具有确定性。
//    该实现需保持边界条件，并保证结果具有确定性。
//  该实现需保持边界条件，并保证结果具有确定性。
//    该实现需保持边界条件，并保证结果具有确定性。
//    该实现需保持边界条件，并保证结果具有确定性。
TEST(FeatureDetectionAnalytic, NoisyCappedCylinderStillRecoversBothRimCircles) {
    const analytic::CylinderFixture cylinder = analytic::makeCylinder(64, 20, 1.0, 2.0, true);
    const double amplitude = 0.1 * analytic::meanEdgeLength(cylinder.mesh);
    const Mesh noisy = analytic::withDeterministicNoise(cylinder.mesh, amplitude, 20260712u);

    const FeatureAnalysis analysis = feature::detectFeatureCurves(noisy, rimDetectionOptions(0.08));

    ASSERT_EQ(2, countCircular(analysis));
    for (const analytic::GroundTruthCircle& truth : cylinder.groundTruthCircles()) {
        SCOPED_TRACE(truth.center.z());
        const FeatureLoop* loop = circularLoopNearestTo(analysis, truth.center);
        ASSERT_NE(nullptr, loop);
        EXPECT_NEAR(truth.radius, loop->radius, 0.05 * truth.radius);
        EXPECT_LT((loop->center - truth.center).norm(), 0.05 * truth.radius);
        EXPECT_GT(std::abs(loop->normal.dot(truth.normal)), 0.95);
    }
}

TEST(FeatureDetectionAnalytic, FeatureEdgeSetIsExactlyScaleInvariant) {
    const analytic::ChamferBoxFixture box = analytic::makeChamferBox(2.0, 0.3, 6);
    const analytic::CylinderFixture cylinder = analytic::makeCylinder(48, 6, 1.0, 2.0, true);

    FeatureOptions options;
    options.featureAngleDeg = 30.0;
    options.minFeatureLoopVertices = 12;
    options.useNormalTensorFeatures = true;

    const Mesh* meshes[] = {&box.mesh, &cylinder.mesh};
    const char* names[] = {"chamfer-box", "capped-cylinder"};
    for (int index = 0; index < 2; ++index) {
        SCOPED_TRACE(names[index]);
        const std::vector<std::pair<int, int>> reference =
            activeGraphEdges(feature::detectFeatureCurves(*meshes[index], options));
        EXPECT_FALSE(reference.empty());
        for (double factor : {1e-3, 1e3}) {
            SCOPED_TRACE(factor);
            const std::vector<std::pair<int, int>> scaled = activeGraphEdges(
                feature::detectFeatureCurves(analytic::uniformlyScaled(*meshes[index], factor), options)
            );
            EXPECT_EQ(reference, scaled);
        }
    }
}

} // namespace feature_detection
} // namespace test
} // namespace manumesh
