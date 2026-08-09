/**
 * @file tests/unit/feature_detection/feature_detection_analytic_tests.cpp
 * @brief 验证 ManuMesh 测试中的特征检测 解析测试行为。
 * @ingroup manumesh_tests
 *
 * @details 测试夹具和断言记录可观察契约、数值容差、确定性要求以及已修复的回归问题。
 */

// 检查该步骤的边界条件，并确保结果保持确定性。
//
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
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
#include <cstddef>
#include <utility>
#include <vector>

namespace manumesh::test::feature_detection {
namespace {

namespace analytic = manumesh::test::analytic;

double maximumPersistentScore(const std::vector<feature::SmoothCurvatureVertex>& values) {
    double maximum = 0.0;
    for (const auto& value : values) {
        maximum = std::max(maximum, value.persistentFeatureScore);
    }
    return maximum;
}

double median(std::vector<double> values) {
    if (values.empty()) {
        return 0.0;
    }
    const std::size_t middle = values.size() / 2;
    std::nth_element(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(middle), values.end());
    return values[middle];
}

/// 说明该辅助函数的输入、输出和边界条件。
/// 说明该辅助函数的输入、输出和边界条件。
FeatureOptions smoothChannelOptions() {
    FeatureOptions options = discreteOnlyOptions();
    options.featureAngleDeg = 180.0;
    options.loopTraceAngleDeg = 180.0;
    options.useSmoothCurvatureFeatures = true;
    options.smoothCurvatureFeatureThreshold = 0.008;
    options.smoothCurvatureMinEdgeAlignment = 0.45;
    options.smoothCurvatureMinTangentConsistency = 0.55;
    return options;
}

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

/// 说明该辅助函数的输入、输出和边界条件。
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

/// 说明该辅助函数的输入、输出和边界条件。
/// 说明该辅助函数的输入、输出和边界条件。
/// 说明该辅助函数的输入、输出和边界条件。
/// 说明该辅助函数的输入、输出和边界条件。
double crestPersistentRecall(
    const std::vector<feature::SmoothCurvatureVertex>& values,
    const std::vector<int>& crestVertices,
    int minScales,
    double threshold
) {
    if (crestVertices.empty()) {
        return 0.0;
    }
    int detected = 0;
    for (int vertex : crestVertices) {
        const feature::SmoothCurvatureVertex& value = values[vertex];
        if (value.persistentScales >= minScales && value.persistentFeatureScore > threshold) {
            ++detected;
        }
    }
    return static_cast<double>(detected) / static_cast<double>(crestVertices.size());
}

} // 命名空间

// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
TEST(FeatureDetectionAnalytic, SphereAndCylinderProduceNoSmoothCurvatureFeatures) {
    const analytic::SphereFixture sphere = analytic::makeUvSphere(24, 48, 1.0);
    const analytic::CylinderFixture cylinder = analytic::makeCylinder(48, 12, 1.0, 2.0, false);

    const feature::SmoothCurvatureOptions curvatureOptions{2, 3, 2, 0.55};
    const FeatureOptions graphOptions = smoothChannelOptions();
    struct Case {
        const char* name;
        const Mesh* mesh;
    };
    const Case cases[] = {
        {"sphere", &sphere.mesh},
        {"cylinder", &cylinder.mesh},
    };
    for (const Case& item : cases) {
        SCOPED_TRACE(item.name);
        const auto values = feature::computeSmoothCurvatureFeatures(*item.mesh, curvatureOptions, 0.008);
        EXPECT_LT(maximumPersistentScore(values), 8e-4);

        const FeatureAnalysis analysis = feature::detectFeatureCurves(*item.mesh, graphOptions);
        EXPECT_EQ(0, analysis.smoothCurvatureFeatureEdges);
    }
}

// 命名空间
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
TEST(FeatureDetectionAnalytic, TorusInnerSideProducesNoSmoothCurvatureFeatures) {
    const analytic::TorusFixture torus = analytic::makeTorus(48, 24, 1.0, 0.3);
    const feature::SmoothCurvatureOptions curvatureOptions{2, 3, 2, 0.55};
    const auto values = feature::computeSmoothCurvatureFeatures(torus.mesh, curvatureOptions, 0.008);
    EXPECT_LT(maximumPersistentScore(values), 8e-4);

    const FeatureAnalysis analysis = feature::detectFeatureCurves(torus.mesh, smoothChannelOptions());
    EXPECT_EQ(0, analysis.smoothCurvatureFeatureEdges);
}

// 命名空间
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
TEST(FeatureDetectionAnalytic, GaussianRidgeCrestCurvatureMatchesAnalyticProfile) {
    const analytic::GaussianRidgeSheetFixture ridge = analytic::makeGaussianRidgeSheet(48, 2.0, 0.35, 6.0);
    const double crestCurvature = ridge.analyticCrestCurvature();
    ASSERT_GT(crestCurvature, 0.0);

    // 检查该步骤的边界条件，并确保结果保持确定性。
    // 检查该步骤的边界条件，并确保结果保持确定性。
    const auto values =
        feature::computeSmoothCurvatureFeatures(ridge.mesh, feature::SmoothCurvatureOptions{2, 1, 0, 0.55}, 1e-6);

    std::vector<double> relativeErrors;
    std::vector<double> tangentAlignments;
    for (int vertex : ridge.interiorCrestVertices()) {
        const feature::SmoothCurvatureVertex& value = values[vertex];
        if (value.signedKind == 0 || value.localScale <= 0.0) {
            continue;
        }
        // 命名空间
        // 检查该步骤的边界条件，并确保结果保持确定性。
        // 检查该步骤的边界条件，并确保结果保持确定性。
        const double estimated = std::abs(value.principalCurvature) / value.localScale;
        relativeErrors.push_back(std::abs(estimated - crestCurvature) / crestCurvature);
        tangentAlignments.push_back(std::abs(value.curveTangent.dot(ridge.crestTangent())));
    }

    // 命名空间
    // 检查该步骤的边界条件，并确保结果保持确定性。
    EXPECT_GT(static_cast<int>(relativeErrors.size()), static_cast<int>(ridge.interiorCrestVertices().size()) / 2);
    EXPECT_LT(median(relativeErrors), 0.15);
    // 检查该步骤的边界条件，并确保结果保持确定性。
    // 检查该步骤的边界条件，并确保结果保持确定性。
    EXPECT_GT(median(tangentAlignments), 0.9);
}

// 命名空间
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
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
        // 检查该步骤的边界条件，并确保结果保持确定性。
        EXPECT_GT(std::abs(loop->normal.dot(truth.normal)), 1.0 - 1e-9);
    }
}

// 命名空间
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
TEST(FeatureDetectionAnalytic, ChamferBoxHardEdgesRecoverWithHighPrecisionAndRecall) {
    const analytic::ChamferBoxFixture box = analytic::makeChamferBox(2.0, 0.3, 6);
    FeatureOptions options = discreteOnlyOptions();
    options.featureAngleDeg = 30.0;
    options.minFeatureLoopVertices = 8;
    const FeatureAnalysis analysis = feature::detectFeatureCurves(box.mesh, options);

    // 检查该步骤的边界条件，并确保结果保持确定性。
    // 检查该步骤的边界条件，并确保结果保持确定性。
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

// 命名空间
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
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

// 命名空间
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
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

// 命名空间
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
//
// 检查该步骤的边界条件，并确保结果保持确定性。
//   该实现需保持边界条件，并保证结果具有确定性。
//   该实现需保持边界条件，并保证结果具有确定性。
//   该实现需保持边界条件，并保证结果具有确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
TEST(FeatureDetectionAnalytic, GradedDensityGaussianRidgeCrestSurvivesDensityTransition) {
    const feature::SmoothCurvatureOptions options{2, 3, 2, 0.55};

    // 检查该步骤的边界条件，并确保结果保持确定性。
    // 检查该步骤的边界条件，并确保结果保持确定性。
    // 检查该步骤的边界条件，并确保结果保持确定性。
    const analytic::GradedGaussianRidgeSheetFixture ridge =
        analytic::makeGradedGaussianRidgeSheet(48, 2.0, 0.50, 14.0, 3);
    const auto values = feature::computeSmoothCurvatureFeatures(ridge.mesh, options, 0.008);
    std::vector<int> crest;
    for (int row = 1; row < ridge.rows; ++row) {
        crest.push_back(ridge.vertexAt(row, ridge.crestColumn()));
    }
    const double gradedRecall = crestPersistentRecall(values, crest, 2, 0.008);

    // 检查该步骤的边界条件，并确保结果保持确定性。
    // 检查该步骤的边界条件，并确保结果保持确定性。
    const analytic::GaussianRidgeSheetFixture uniform = analytic::makeGaussianRidgeSheet(64, 2.0, 0.50, 14.0);
    const auto uniformValues = feature::computeSmoothCurvatureFeatures(uniform.mesh, options, 0.008);
    const double uniformRecall = crestPersistentRecall(uniformValues, uniform.interiorCrestVertices(), 2, 0.008);

    EXPECT_GT(uniformRecall, 0.90);
    EXPECT_GE(gradedRecall, uniformRecall - 0.10);
}

// 命名空间
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
//
// 检查该步骤的边界条件，并确保结果保持确定性。
//   该实现需保持边界条件，并保证结果具有确定性。
//   该实现需保持边界条件，并保证结果具有确定性。
TEST(FeatureDetectionAnalytic, NarrowRidgeOnDenseSheetSurvivesCoarsestScale) {
    const analytic::GaussianRidgeSheetFixture ridge = analytic::makeGaussianRidgeSheet(64, 2.0, 0.05, 400.0);
    const feature::SmoothCurvatureOptions options{2, 5, 2, 0.55};
    const auto values = feature::computeSmoothCurvatureFeatures(ridge.mesh, options, 0.008);

    const double recall = crestPersistentRecall(values, ridge.interiorCrestVertices(), 2, 0.008);
    EXPECT_GT(recall, 0.90);
}

// 命名空间
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
TEST(FeatureDetectionAnalytic, FeatureEdgeSetIsExactlyScaleInvariant) {
    const analytic::ChamferBoxFixture box = analytic::makeChamferBox(2.0, 0.3, 6);
    const analytic::CylinderFixture cylinder = analytic::makeCylinder(48, 6, 1.0, 2.0, true);

    FeatureOptions options;
    options.featureAngleDeg = 30.0;
    options.minFeatureLoopVertices = 12;
    options.useNormalTensorFeatures = true;
    options.useSmoothCurvatureFeatures = true;

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

} // 命名空间
