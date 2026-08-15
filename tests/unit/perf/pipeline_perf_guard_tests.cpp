/**
 * @file tests/unit/perf/pipeline_perf_guard_tests.cpp
 * @brief 验证 ManuMesh 测试中的流水线性能门禁测试行为。
 * @ingroup manumesh_tests
 *
 * @details 测试夹具和断言记录可观察契约、数值容差、确定性要求以及已修复的回归问题。
 */

// 针对两个流水线入口、约 1.6 万面解析球体的粗粒度墙钟回归保护。
// 这不是基准测试：阈值是宽松的绝对上限，目的仅是快速发现误将 O(n^2) 路径
//（例如每次折叠都扫描整个网格，或以二次复杂度收集邻域）放入热循环的情况。
// 2026-07 之前确实出现过这类问题：SimplificationRun::tryCollapse 每次尝试
// 都重新计算输入包围盒对角线（O(V) 扫描），该保护应能捕获它（约 1.6 万面简化
// 从 2 秒增至 15 秒）。
//
// 机器基线（Windows x64 Release，桌面 CPU；阈值按跨机器保守上限设置）：
//   detectFeatureCurves（二面角 + 法向张量）约 0.18 秒 -> 限制 2 秒
//   简化至 0.2 比例（默认选项）约 2.0 秒          -> 限制 8 秒
// 两个限制均至少是实测时间的 3 倍，因此较慢的 CI 机器也能通过；但该规模下
// 超过 10 倍的二次复杂度回归无法通过。平滑曲率通道在 1.6 万面上约需 6 秒，
// 会超出快速套件预算，故有意排除；其功能由较小夹具上的
// feature_detection_analytic_tests.cpp 覆盖。
#include "AnalyticFixtures.h"

#include "algorithms/feature_detection/FeatureDetector.h"
#include "algorithms/simplification/QEMSimplifier.h"

#include <gtest/gtest.h>

#include <chrono>

namespace {
namespace analytic = manumesh::test::analytic;
namespace feature = manumesh::feature;
namespace simplification = manumesh::simplification;

double elapsedSeconds(const std::chrono::steady_clock::time_point& start) {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}

} // namespace
TEST(PipelinePerfGuard, FeatureDetectionOnSixteenThousandFaceSphereStaysBounded) {
    const analytic::SphereFixture sphere = analytic::makeUvSphere(64, 128, 1.0);
    ASSERT_GT(static_cast<int>(sphere.mesh.faces.size()), 15000);

    feature::FeatureOptions options;
    options.featureAngleDeg = 40.0;
    options.useNormalTensorFeatures = true;
    options.useSmoothCurvatureFeatures = false;

    const auto start = std::chrono::steady_clock::now();
    const feature::FeatureAnalysis analysis = feature::detectFeatureCurves(sphere.mesh, options);
    const double seconds = elapsedSeconds(start);

    // 平滑闭球不应包含硬特征；读取结果还可避免调用被优化器删除。
    EXPECT_EQ(0, analysis.dihedralFeatureEdges);
    EXPECT_LT(seconds, 2.0) << "detectFeatureCurves took " << seconds << " s on ~16k faces";
}

TEST(PipelinePerfGuard, SimplifyOnSixteenThousandFaceSphereStaysBounded) {
    const analytic::SphereFixture sphere = analytic::makeUvSphere(64, 128, 1.0);

    simplification::SimplifyOptions options;
    options.targetRatio = 0.2;

    const auto start = std::chrono::steady_clock::now();
    simplification::QEMSimplifier simplifier(options);
    const manumesh::Mesh output = simplifier.simplify(sphere.mesh);
    const double seconds = elapsedSeconds(start);

    EXPECT_FALSE(output.empty());
    EXPECT_LT(seconds, 8.0) << "simplify took " << seconds << " s on ~16k faces";
}
