#include "CApiTestSupport.h"

#include <cstddef>
#include <gtest/gtest.h>
#include <limits>
#include <string>
#include <vector>

using manumesh::test::dataRoot;
TEST_F(CApiTest, ExposesNormalTensorOptionsAndDiagnostics) {
  ManuMeshMeshHandle* input = manumesh_mesh_create(context);
  ManuMeshMeshHandle* output = manumesh_mesh_create(context);
  ASSERT_NE(input, nullptr);
  ASSERT_NE(output, nullptr);

  ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_generate_mesh(context, "ridge", 32, input));

  ManuMeshSimplifyOptions options;
  manumesh_simplify_options_init(&options);
  options.target_ratio = 0.80;
  options.preserve_feature_curves = 1;
  options.weight_mode = MANUMESH_WEIGHT_MODE_NORMAL_TENSOR;
  options.feature_angle_deg = 179.0;
  options.loop_trace_angle_deg = 179.0;
  options.normal_tensor_feature_threshold = 0.06;
  options.normal_tensor_min_edge_alignment = 0.2;
  options.normal_tensor_smoothing_iterations = 1;
  options.normal_tensor_scale_count = 3;
  options.normal_tensor_min_persistent_scales = 2;

  ManuMeshSimplifyReport report;
  EXPECT_EQ(MANUMESH_STATUS_OK,
            manumesh_simplify_mesh(context, input, &options, output, &report));
  EXPECT_GT(report.normal_tensor_feature_edges, 0);
  EXPECT_GT(report.traced_feature_edges, 0);
  EXPECT_EQ(0, report.untraced_feature_edges);
  EXPECT_GT(report.normal_tensor_scored_vertices, 0);
  EXPECT_GT(report.max_normal_tensor_persistent_score, 0.0);
  EXPECT_GT(report.mean_normal_tensor_local_scale, 0.0);
  EXPECT_GT(report.mean_normal_tensor_persistence, 1.0);

  manumesh_mesh_destroy(output);
  manumesh_mesh_destroy(input);
}

TEST_F(CApiTest, ExposesLegalityOptionsAndDetailedRejectDiagnostics) {
  ManuMeshMeshHandle* input = manumesh_mesh_create(context);
  ManuMeshMeshHandle* output = manumesh_mesh_create(context);
  ASSERT_NE(input, nullptr);
  ASSERT_NE(output, nullptr);

  ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_generate_mesh(context, "plane", 8, input));

  ManuMeshSimplifyOptions options;
  manumesh_simplify_options_init(&options);
  options.target_ratio = 0.08;
  options.use_line_quadrics = 0;
  options.preserve_boundary = 1;
  options.min_triangle_quality = 0.80;
  options.max_normal_deviation_deg = 180.0;

  ManuMeshSimplifyReport report;
  EXPECT_EQ(MANUMESH_STATUS_OK,
            manumesh_simplify_mesh(context, input, &options, output, &report));
  EXPECT_GT(report.boundary_rejected_collapses, 0);
  EXPECT_GT(report.quality_rejected_collapses, 0);
  EXPECT_EQ(
      report.rejected_collapses,
      report.feature_rejected_collapses + report.boundary_rejected_collapses +
          report.topology_rejected_collapses + report.normal_flip_rejected_collapses +
          report.quality_rejected_collapses +
          report.self_intersection_rejected_collapses +
          report.curve_budget_rejected_collapses + report.error_rejected_collapses);

  manumesh_mesh_destroy(output);
  manumesh_mesh_destroy(input);
}

TEST_F(CApiTest, ExposesFeatureCurveProtectionDiagnostics) {
  ManuMeshMeshHandle* input = manumesh_mesh_create(context);
  ManuMeshMeshHandle* output = manumesh_mesh_create(context);
  ASSERT_NE(input, nullptr);
  ASSERT_NE(output, nullptr);

  const ManuMeshVec3 vertices[] = {
      {0.0, 0.0, 0.0}, {3.0, 0.0, 0.0},  {2.5, 1.0, 0.0},
      {1.2, 2.2, 0.0}, {-0.4, 1.3, 0.0},
  };
  const ManuMeshFace faces[] = {
      {{0, 1, 3}},
      {{1, 2, 3}},
      {{0, 3, 4}},
  };
  ASSERT_EQ(MANUMESH_STATUS_OK,
            manumesh_mesh_set_data(context, input, vertices, 5, faces, 3));

  ManuMeshSimplifyOptions options;
  manumesh_simplify_options_init(&options);
  options.target_faces = 1;
  options.target_ratio = 0.25;
  options.use_line_quadrics = 0;
  options.preserve_feature_curves = 1;
  options.feature_protection_mode = MANUMESH_FEATURE_PROTECTION_ALL_FEATURE_EDGES;
  options.use_normal_tensor_features = 0;
  options.feature_angle_deg = 179.0;
  options.circle_fit_relative_threshold = 0.0;
  options.ellipse_fit_relative_threshold = 0.0;
  options.min_feature_loop_vertices = 3;
  options.max_feature_curve_deviation_ratio = 1e-9;
  options.max_normal_deviation_deg = 180.0;
  options.min_triangle_quality = 0.0;

  ManuMeshSimplifyReport report;
  EXPECT_EQ(MANUMESH_STATUS_OK,
            manumesh_simplify_mesh(context, input, &options, output, &report));
  EXPECT_GT(report.feature_loops, 0);
  EXPECT_GT(report.feature_rejected_collapses, 0);
  EXPECT_GT(report.generic_feature_rejected_collapses, 0);
  EXPECT_EQ(MANUMESH_SIMPLIFY_TERMINATION_REJECTION_LIMIT, report.termination_reason);
  EXPECT_EQ(
      report.rejected_collapses,
      report.feature_rejected_collapses + report.boundary_rejected_collapses +
          report.topology_rejected_collapses + report.normal_flip_rejected_collapses +
          report.quality_rejected_collapses +
          report.self_intersection_rejected_collapses +
          report.curve_budget_rejected_collapses + report.error_rejected_collapses);

  manumesh_mesh_destroy(output);
  manumesh_mesh_destroy(input);
}
