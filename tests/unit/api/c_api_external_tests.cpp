/**
 * @file tests/unit/api/c_api_external_tests.cpp
 * @brief 使用真实 STL 夹具验证 C API 不透明句柄的加载与简化流程。
 * @ingroup manumesh_tests
 */

// 使用真实外部 STL 夹具执行 C API 冒烟测试（夹具位于 tests/data/external）。
// 测试归入 `external` CTest 标签，以免拖慢快速测试套件。
#include "CApiTestSupport.h"

#include <cstddef>
#include <gtest/gtest.h>
#include <string>

using manumesh::test::dataRoot;
TEST_F(CApiTest, SimplifiesRealStlThroughOpaqueHandles) {
    ManuMeshMeshHandle* input = manumesh_mesh_create(context);
    ManuMeshMeshHandle* output = manumesh_mesh_create(context);
    ASSERT_NE(input, nullptr);
    ASSERT_NE(output, nullptr);

    const std::string inputPath = (dataRoot() / "external" / "nasa_antenna_azimuth_track.stl").string();
    EXPECT_EQ(MANUMESH_STATUS_OK, manumesh_load_mesh(context, inputPath.c_str(), input, 1e-8));

    size_t inputVertices = 0;
    size_t inputFaces = 0;
    EXPECT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_get_counts(context, input, &inputVertices, &inputFaces));
    EXPECT_GT(inputVertices, 0u);
    EXPECT_GT(inputFaces, 0u);

    ManuMeshSimplifyOptions options;
    manumesh_simplify_options_init(&options);
    options.target_ratio = 0.80;
    options.weight_mode = MANUMESH_WEIGHT_MODE_DIHEDRAL;
    options.feature_boost = 0.08;
    options.feature_angle_deg = 25.0;

    ManuMeshSimplifyReport report;
    manumesh_simplify_report_init(&report);
    EXPECT_EQ(MANUMESH_STATUS_OK, manumesh_simplify_mesh(context, input, &options, output, &report));
    EXPECT_EQ(static_cast<int>(inputFaces), report.initial_faces);
    EXPECT_GT(report.collapsed_edges, 0);
    EXPECT_LT(report.final_faces, report.initial_faces);
    EXPECT_EQ(MANUMESH_SIMPLIFY_TERMINATION_REACHED_TARGET, report.termination_reason);

    ManuMeshMeshStats stats;
    manumesh_mesh_stats_init(&stats);
    EXPECT_EQ(MANUMESH_STATUS_OK, manumesh_compute_mesh_stats(context, output, &stats));
    EXPECT_EQ(report.final_faces, stats.faces);
    EXPECT_GT(stats.area, 0.0);

    manumesh_mesh_destroy(output);
    manumesh_mesh_destroy(input);
}
