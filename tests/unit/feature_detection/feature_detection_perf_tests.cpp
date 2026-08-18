/**
 * @file tests/unit/feature_detection/feature_detection_perf_tests.cpp
 * @brief 提供特征检测手动计时用例，记录不同网格规模的运行时间。
 * @ingroup manumesh_tests
 */

#include "FeatureDetectionTestSupport.h"

#include "common/detail/ParallelExecution.h"
#include "core/MeshGenerators.h"
#include "feature_detection/detail/FeaturePrimitiveRecovery.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <vector>

namespace manumesh {
namespace test {
namespace feature_detection {
namespace {

namespace feature = manumesh::feature;

double timeAnalysisMs(const Mesh& mesh, const feature::FeatureOptions& options, int repeats) {
    double best = 1e300;
    for (int i = 0; i < repeats; ++i) {
        const auto start = std::chrono::steady_clock::now();
        const feature::FeatureAnalysis analysis = feature::detectFeatureCurves(mesh, options);
        const auto stop = std::chrono::steady_clock::now();
        if (analysis.featureEdges < 0) {
            std::printf("unexpected\n");
        }
        best = std::min(best, std::chrono::duration<double, std::milli>(stop - start).count());
    }
    return best;
}

struct PrimitiveRecoveryBenchmarkFixture {
    Mesh mesh;
    feature::detector_detail::TraceGraph trace;
    int componentCount = 0;
};

PrimitiveRecoveryBenchmarkFixture makePrimitiveRecoveryBenchmarkFixture(int componentCount, int samplesPerComponent) {
    PrimitiveRecoveryBenchmarkFixture fixture;
    fixture.componentCount = componentCount;
    fixture.mesh.vertices.reserve(static_cast<std::size_t>(componentCount * samplesPerComponent));
    constexpr double twoPi = 6.28318530717958647692;
    for (int component = 0; component < componentCount; ++component) {
        const int first = static_cast<int>(fixture.mesh.vertices.size());
        const double centerX = 4.0 * static_cast<double>(component);
        for (int sample = 0; sample < samplesPerComponent; ++sample) {
            const double angle = twoPi * static_cast<double>(sample) / static_cast<double>(samplesPerComponent);
            fixture.mesh.vertices.emplace_back(centerX + std::cos(angle), std::sin(angle), 0.0);
        }
        for (int sample = 0; sample < samplesPerComponent; ++sample) {
            const int a = first + sample;
            const int b = first + (sample + 1) % samplesPerComponent;
            fixture.trace.adjacency.resize(fixture.mesh.vertices.size());
            fixture.trace.adjacency[a].push_back(b);
            fixture.trace.adjacency[b].push_back(a);
        }
    }
    fixture.trace.adjacency.resize(fixture.mesh.vertices.size());
    return fixture;
}

double timePrimitiveRecovery(
    const PrimitiveRecoveryBenchmarkFixture& fixture,
    const feature::FeatureOptions& options,
    const manumesh::common::parallel::RangeExecutionOptions& executionOptions
) {
    const auto start = std::chrono::steady_clock::now();
    feature::FeatureAnalysis analysis;
    analysis.vertices.assign(fixture.mesh.vertices.size(), feature::VertexFeature{});
    analysis.graph.vertices.assign(fixture.mesh.vertices.size(), feature::FeatureGraphVertex{});
    int loopId = 0;
    feature::detector_detail::recoverPrimitiveComponents(
        fixture.mesh, options, fixture.trace, analysis, loopId, executionOptions
    );
    const auto stop = std::chrono::steady_clock::now();
    EXPECT_EQ(analysis.loops.size(), static_cast<std::size_t>(fixture.componentCount));
    return std::chrono::duration<double, std::milli>(stop - start).count();
}

} // namespace

//   该实现需保持边界条件，并保证结果具有确定性。
//     该实现需保持边界条件，并保证结果具有确定性。
TEST(FeatureDetectionPerf, DISABLED_AnalyzeTiming) {
    feature::FeatureOptions weakOptions;
    weakOptions.featureAngleDeg = 40.0;
    weakOptions.useNormalTensorFeatures = true;
    weakOptions.normalTensorScaleCount = 3;
    weakOptions.normalTensorSmoothingIterations = 1;
    weakOptions.useSmoothCurvatureFeatures = true;
    weakOptions.smoothCurvatureScaleCount = 3;
    weakOptions.smoothCurvatureRobustFitIterations = 2;

    feature::FeatureOptions hardOptions;
    hardOptions.featureAngleDeg = 25.0;
    hardOptions.useNormalTensorFeatures = false;

    struct Case {
        const char* name;
        Mesh mesh;
        const feature::FeatureOptions* options;
    };
    std::vector<Case> cases;
    cases.push_back({"bump64-weak", manumesh::generateBumpGrid(64, 2.0), &weakOptions});
    cases.push_back({"terrace64-weak", manumesh::generateTerraceGrid(64, 2.0), &weakOptions});
    cases.push_back({"pulley48-hard", manumesh::generatePulleyGrid(48), &hardOptions});
    for (const Case& item : cases) {
        const double ms = timeAnalysisMs(item.mesh, *item.options, 3);
        std::printf(
            "[perf] %-14s vertices=%zu faces=%zu best=%.2f ms\n",
            item.name,
            item.mesh.vertices.size(),
            item.mesh.faces.size(),
            ms
        );
    }

    {
        const Mesh& mesh = cases.front().mesh;
        const auto t0 = std::chrono::steady_clock::now();
        const auto curvature =
            feature::computeSmoothCurvatureFeatures(mesh, feature::SmoothCurvatureOptions{2, 3, 2, 0.65}, 0.015);
        const auto t1 = std::chrono::steady_clock::now();
        const auto tensor = feature::computeNormalTensorFeatures(mesh, feature::NormalTensorOptions{1, 3, {}}, 0.16);
        const auto t2 = std::chrono::steady_clock::now();
        const auto curvatureFast =
            feature::computeSmoothCurvatureFeatures(mesh, feature::SmoothCurvatureOptions{2, 1, 0, 0.65}, 0.015);
        const auto t3 = std::chrono::steady_clock::now();
        std::printf(
            "[perf] stage smoothCurvature(scale1,robust0)=%.2f ms (size %zu)\n",
            std::chrono::duration<double, std::milli>(t3 - t2).count(),
            curvatureFast.size()
        );
        std::printf(
            "[perf] stage smoothCurvature=%.2f ms normalTensor=%.2f ms (sizes %zu/%zu)\n",
            std::chrono::duration<double, std::milli>(t1 - t0).count(),
            std::chrono::duration<double, std::milli>(t2 - t1).count(),
            curvature.size(),
            tensor.size()
        );
    }
    SUCCEED();
}

TEST(FeatureDetectionPerf, DISABLED_PrimitiveRecoveryTiming) {
    const PrimitiveRecoveryBenchmarkFixture fixture = makePrimitiveRecoveryBenchmarkFixture(1024, 128);
    feature::FeatureOptions options;
    options.minFeatureLoopVertices = 16;
    options.circleFitRelativeThreshold = 0.01;

    manumesh::common::parallel::RangeExecutionOptions serial;
    serial.enabled = false;
    serial.grainSize = 1;
    manumesh::common::parallel::RangeExecutionOptions parallel;
    parallel.enabled = true;
    parallel.maxConcurrency = 8;
    parallel.grainSize = 4096;

    // Warm up oneTBB and the Eigen code path before recording the comparison.
    EXPECT_GT(timePrimitiveRecovery(fixture, options, serial), 0.0);
    EXPECT_GT(timePrimitiveRecovery(fixture, options, parallel), 0.0);
    const double serialMs = timePrimitiveRecovery(fixture, options, serial);
    const double parallelMs = timePrimitiveRecovery(fixture, options, parallel);
    std::printf(
        "[perf] primitiveRecovery components=%d vertices=%zu serial=%.2f ms parallel8=%.2f ms speedup=%.2fx\n",
        fixture.componentCount,
        fixture.mesh.vertices.size(),
        serialMs,
        parallelMs,
        parallelMs > 0.0 ? serialMs / parallelMs : 0.0
    );
}

} // namespace feature_detection
} // namespace test
} // namespace manumesh
