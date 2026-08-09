/**
 * @file tests/unit/feature_detection/feature_detection_perf_tests.cpp
 * @brief 验证 ManuMesh 测试中的特征检测 性能测试行为。
 * @ingroup manumesh_tests
 *
 * @details 测试夹具和断言记录可观察契约、数值容差、确定性要求以及已修复的回归问题。
 */

#include "FeatureDetectionTestSupport.h"

#include "core/MeshGenerators.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <vector>

namespace manumesh::test::feature_detection {
namespace {

namespace feature = manumesh::feature;

double timeAnalysisMs(const Mesh& mesh, const feature::FeatureOptions& options, int repeats) {
    double best = 1e300;
    for (int i = 0; i < repeats; ++i) {
        const auto start = std::chrono::steady_clock::now();
        const feature::FeatureAnalysis analysis = feature::detectFeatureCurves(mesh, options);
        const auto stop = std::chrono::steady_clock::now();
        // 检查该步骤的边界条件，并确保结果保持确定性。
        if (analysis.featureEdges < 0) {
            std::printf("unexpected\n");
        }
        best = std::min(best, std::chrono::duration<double, std::milli>(stop - start).count());
    }
    return best;
}

} // 命名空间

// 检查该步骤的边界条件，并确保结果保持确定性。
//   该实现需保持边界条件，并保证结果具有确定性。
//     该实现需保持边界条件，并保证结果具有确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
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
        const auto tensor = feature::computeNormalTensorFeatures(mesh, feature::NormalTensorOptions{1, 3}, 0.16);
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

} // 命名空间
