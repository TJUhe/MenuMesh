#include "CApiTestSupport.h"

#include <cstddef>
#include <gtest/gtest.h>
#include <limits>
#include <string>
#include <vector>

using manumesh::test::dataRoot;
TEST_F(CApiTest, MapsInvalidSimplifyOptionsToInvalidArgumentStatus) {
  ManuMeshMeshHandle* input = manumesh_mesh_create(context);
  ManuMeshMeshHandle* output = manumesh_mesh_create(context);
  ASSERT_NE(input, nullptr);
  ASSERT_NE(output, nullptr);

  ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_generate_mesh(context, "plane", 8, input));

  ManuMeshSimplifyOptions options;
  manumesh_simplify_options_init(&options);
  options.target_ratio = 0.0;

  ManuMeshSimplifyReport report;
  EXPECT_EQ(MANUMESH_STATUS_INVALID_ARGUMENT,
            manumesh_simplify_mesh(context, input, &options, output, &report));
  EXPECT_NE('\0', manumesh_context_last_error(context)[0]);

  manumesh_mesh_destroy(output);
  manumesh_mesh_destroy(input);
}

TEST_F(CApiTest, RejectsUninitializedSimplifyOptionsAbiStruct) {
  ManuMeshMeshHandle* input = manumesh_mesh_create(context);
  ManuMeshMeshHandle* output = manumesh_mesh_create(context);
  ASSERT_NE(input, nullptr);
  ASSERT_NE(output, nullptr);

  ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_generate_mesh(context, "plane", 8, input));

  ManuMeshSimplifyOptions options{};
  options.target_ratio = 0.5;

  ManuMeshSimplifyReport report;
  EXPECT_EQ(MANUMESH_STATUS_INVALID_ARGUMENT,
            manumesh_simplify_mesh(context, input, &options, output, &report));
  EXPECT_NE('\0', manumesh_context_last_error(context)[0]);

  manumesh_mesh_destroy(output);
  manumesh_mesh_destroy(input);
}

TEST_F(CApiTest, AcceptsOlderTrailingSimplifyOptionsAbiStruct) {
  ManuMeshMeshHandle* input = manumesh_mesh_create(context);
  ManuMeshMeshHandle* output = manumesh_mesh_create(context);
  ASSERT_NE(input, nullptr);
  ASSERT_NE(output, nullptr);

  ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_generate_mesh(context, "plane", 8, input));

  ManuMeshSimplifyOptions options;
  manumesh_simplify_options_init(&options);
  options.target_ratio = 0.75;
  options.feature_protection_mode = static_cast<ManuMeshFeatureProtectionMode>(999);
  options.struct_size = offsetof(ManuMeshSimplifyOptions, feature_protection_mode);

  ManuMeshSimplifyReport report;
  EXPECT_EQ(MANUMESH_STATUS_OK,
            manumesh_simplify_mesh(context, input, &options, output, &report));
  EXPECT_GT(report.initial_faces, report.final_faces);
  EXPECT_EQ(MANUMESH_SIMPLIFY_TERMINATION_REACHED_TARGET, report.termination_reason);

  manumesh_mesh_destroy(output);
  manumesh_mesh_destroy(input);
}

TEST_F(CApiTest, InitializesPrimitiveFitOptions) {
  ManuMeshSimplifyOptions options;
  manumesh_simplify_options_init(&options);

  EXPECT_EQ(sizeof(ManuMeshSimplifyOptions), options.struct_size);
  EXPECT_EQ(MANUMESH_ABI_VERSION, options.abi_version);
  EXPECT_DOUBLE_EQ(0.05, options.circle_fit_relative_threshold);
  EXPECT_DOUBLE_EQ(0.05, options.ellipse_fit_relative_threshold);
  EXPECT_DOUBLE_EQ(0.08, options.near_circle_axis_ratio_tolerance);
  EXPECT_DOUBLE_EQ(-1.0, options.loop_trace_angle_deg);
  EXPECT_DOUBLE_EQ(0.0, options.max_feature_curve_deviation_ratio);
  EXPECT_EQ(6, options.min_circular_feature_loop_vertices);
  EXPECT_EQ(0, options.preserve_boundary);
  EXPECT_DOUBLE_EQ(0.0, options.min_triangle_quality);
  EXPECT_DOUBLE_EQ(90.0, options.max_normal_deviation_deg);
  EXPECT_EQ(1, options.normal_tensor_scale_count);
  EXPECT_EQ(1, options.normal_tensor_min_persistent_scales);
  EXPECT_EQ(1, options.cleanup_feature_graph);
  EXPECT_DOUBLE_EQ(1.25, options.feature_graph_gap_length_ratio);
  EXPECT_EQ(2, options.feature_graph_max_weak_spur_edges);
  EXPECT_DOUBLE_EQ(0.35, options.feature_component_min_confidence);
  EXPECT_DOUBLE_EQ(0.0, options.max_local_error);
  EXPECT_DOUBLE_EQ(0.0, options.max_local_error_ratio);
  EXPECT_EQ(0, options.prevent_local_intersections);
  EXPECT_EQ(MANUMESH_FEATURE_PROTECTION_PRIMITIVE_CURVES,
            options.feature_protection_mode);

  ManuMeshSimplifyReport report;
  manumesh_simplify_report_init(&report);
  EXPECT_EQ(sizeof(ManuMeshSimplifyReport), report.struct_size);
  EXPECT_EQ(MANUMESH_ABI_VERSION, report.abi_version);

  ManuMeshMeshStats stats;
  manumesh_mesh_stats_init(&stats);
  EXPECT_EQ(sizeof(ManuMeshMeshStats), stats.struct_size);
  EXPECT_EQ(MANUMESH_ABI_VERSION, stats.abi_version);
}
