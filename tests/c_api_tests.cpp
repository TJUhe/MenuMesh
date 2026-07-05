#include "TestSupport.h"
#include "line_quadrics_qem/api/CApi.h"

#include <gtest/gtest.h>
#include <limits>
#include <string>
#include <vector>

namespace {

class CApiTest : public ::testing::Test {
protected:
  void SetUp() override {
    context = lq_context_create();
    ASSERT_NE(context, nullptr);
  }

  void TearDown() override { lq_context_destroy(context); }

  LqContext* context = nullptr;
};

using lq::test::dataRoot;

} // namespace

TEST_F(CApiTest, SimplifiesRealStlThroughOpaqueHandles) {
  LqMeshHandle* input = lq_mesh_create(context);
  LqMeshHandle* output = lq_mesh_create(context);
  ASSERT_NE(input, nullptr);
  ASSERT_NE(output, nullptr);

  const std::string inputPath =
      (dataRoot() / "external" / "nasa_antenna_azimuth_track.stl").string();
  EXPECT_EQ(LQ_STATUS_OK, lq_load_mesh(context, inputPath.c_str(), input, 1e-8));

  size_t inputVertices = 0;
  size_t inputFaces = 0;
  EXPECT_EQ(LQ_STATUS_OK,
            lq_mesh_get_counts(context, input, &inputVertices, &inputFaces));
  EXPECT_GT(inputVertices, 0u);
  EXPECT_GT(inputFaces, 0u);

  LqSimplifyOptions options;
  lq_simplify_options_init(&options);
  options.target_ratio = 0.80;
  options.weight_mode = LQ_WEIGHT_MODE_DIHEDRAL;
  options.feature_boost = 0.08;
  options.feature_angle_deg = 25.0;

  LqSimplifyReport report;
  EXPECT_EQ(LQ_STATUS_OK, lq_simplify_mesh(context, input, &options, output, &report));
  EXPECT_EQ(static_cast<int>(inputFaces), report.initial_faces);
  EXPECT_GT(report.collapsed_edges, 0);
  EXPECT_LT(report.final_faces, report.initial_faces);
  EXPECT_EQ(LQ_SIMPLIFY_TERMINATION_REACHED_TARGET, report.termination_reason);

  LqMeshStats stats;
  EXPECT_EQ(LQ_STATUS_OK, lq_compute_mesh_stats(context, output, &stats));
  EXPECT_EQ(report.final_faces, stats.faces);
  EXPECT_GT(stats.area, 0.0);

  lq_mesh_destroy(output);
  lq_mesh_destroy(input);
}

TEST_F(CApiTest, CopiesMeshDataOnlyIntoCallerOwnedBuffers) {
  LqMeshHandle* mesh = lq_mesh_create(context);
  ASSERT_NE(mesh, nullptr);

  const LqVec3 vertices[] = {
      {0.0, 0.0, 0.0},
      {1.0, 0.0, 0.0},
      {0.0, 1.0, 0.0},
  };
  const LqFace faces[] = {{{0, 1, 2}}};
  EXPECT_EQ(LQ_STATUS_OK, lq_mesh_set_data(context, mesh, vertices, 3, faces, 1));

  size_t required = 0;
  EXPECT_EQ(LQ_STATUS_BUFFER_TOO_SMALL,
            lq_mesh_copy_vertices(context, mesh, nullptr, 0, &required));
  EXPECT_EQ(3u, required);

  std::vector<LqVec3> copiedVertices(required);
  EXPECT_EQ(LQ_STATUS_OK, lq_mesh_copy_vertices(context, mesh, copiedVertices.data(),
                                                copiedVertices.size(), &required));
  EXPECT_EQ(1.0, copiedVertices[1].x);

  size_t copiedFaces = 0;
  LqFace copiedFace;
  EXPECT_EQ(LQ_STATUS_OK,
            lq_mesh_copy_faces(context, mesh, &copiedFace, 1, &copiedFaces));
  EXPECT_EQ(1u, copiedFaces);
  EXPECT_EQ(2, copiedFace.v[2]);

  lq_mesh_destroy(mesh);
}

TEST_F(CApiTest, RejectsNonFiniteVertexCoordinatesWithoutReplacingMesh) {
  LqMeshHandle* mesh = lq_mesh_create(context);
  ASSERT_NE(mesh, nullptr);

  const LqVec3 vertices[] = {
      {0.0, 0.0, 0.0},
      {1.0, 0.0, 0.0},
      {0.0, 1.0, 0.0},
  };
  const LqFace faces[] = {{{0, 1, 2}}};
  ASSERT_EQ(LQ_STATUS_OK, lq_mesh_set_data(context, mesh, vertices, 3, faces, 1));

  const LqVec3 invalidVertices[] = {
      {0.0, 0.0, 0.0},
      {std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0},
      {0.0, 1.0, 0.0},
  };
  EXPECT_EQ(LQ_STATUS_INVALID_ARGUMENT,
            lq_mesh_set_data(context, mesh, invalidVertices, 3, faces, 1));
  EXPECT_NE('\0', lq_context_last_error(context)[0]);

  size_t vertexCount = 0;
  size_t faceCount = 0;
  EXPECT_EQ(LQ_STATUS_OK, lq_mesh_get_counts(context, mesh, &vertexCount, &faceCount));
  EXPECT_EQ(3u, vertexCount);
  EXPECT_EQ(1u, faceCount);

  lq_mesh_destroy(mesh);
}

TEST_F(CApiTest, RejectsInvalidFaceIndicesWithoutReplacingMesh) {
  LqMeshHandle* mesh = lq_mesh_create(context);
  ASSERT_NE(mesh, nullptr);

  const LqVec3 vertices[] = {
      {0.0, 0.0, 0.0},
      {1.0, 0.0, 0.0},
      {0.0, 1.0, 0.0},
  };
  const LqFace validFaces[] = {{{0, 1, 2}}};
  ASSERT_EQ(LQ_STATUS_OK, lq_mesh_set_data(context, mesh, vertices, 3, validFaces, 1));

  const LqFace invalidFaces[] = {{{0, 1, 5}}};
  EXPECT_EQ(LQ_STATUS_INVALID_ARGUMENT,
            lq_mesh_set_data(context, mesh, vertices, 3, invalidFaces, 1));
  EXPECT_NE('\0', lq_context_last_error(context)[0]);

  size_t vertexCount = 0;
  size_t faceCount = 0;
  EXPECT_EQ(LQ_STATUS_OK, lq_mesh_get_counts(context, mesh, &vertexCount, &faceCount));
  EXPECT_EQ(3u, vertexCount);
  EXPECT_EQ(1u, faceCount);

  lq_mesh_destroy(mesh);
}

TEST_F(CApiTest, MapsInvalidSimplifyOptionsToInvalidArgumentStatus) {
  LqMeshHandle* input = lq_mesh_create(context);
  LqMeshHandle* output = lq_mesh_create(context);
  ASSERT_NE(input, nullptr);
  ASSERT_NE(output, nullptr);

  ASSERT_EQ(LQ_STATUS_OK, lq_generate_mesh(context, "plane", 8, input));

  LqSimplifyOptions options;
  lq_simplify_options_init(&options);
  options.target_ratio = 0.0;

  LqSimplifyReport report;
  EXPECT_EQ(LQ_STATUS_INVALID_ARGUMENT,
            lq_simplify_mesh(context, input, &options, output, &report));
  EXPECT_NE('\0', lq_context_last_error(context)[0]);

  lq_mesh_destroy(output);
  lq_mesh_destroy(input);
}

TEST_F(CApiTest, RejectsUninitializedSimplifyOptionsAbiStruct) {
  LqMeshHandle* input = lq_mesh_create(context);
  LqMeshHandle* output = lq_mesh_create(context);
  ASSERT_NE(input, nullptr);
  ASSERT_NE(output, nullptr);

  ASSERT_EQ(LQ_STATUS_OK, lq_generate_mesh(context, "plane", 8, input));

  LqSimplifyOptions options{};
  options.target_ratio = 0.5;

  LqSimplifyReport report;
  EXPECT_EQ(LQ_STATUS_INVALID_ARGUMENT,
            lq_simplify_mesh(context, input, &options, output, &report));
  EXPECT_NE('\0', lq_context_last_error(context)[0]);

  lq_mesh_destroy(output);
  lq_mesh_destroy(input);
}

TEST_F(CApiTest, ExposesNormalTensorOptionsAndDiagnostics) {
  LqMeshHandle* input = lq_mesh_create(context);
  LqMeshHandle* output = lq_mesh_create(context);
  ASSERT_NE(input, nullptr);
  ASSERT_NE(output, nullptr);

  ASSERT_EQ(LQ_STATUS_OK, lq_generate_mesh(context, "ridge", 32, input));

  LqSimplifyOptions options;
  lq_simplify_options_init(&options);
  options.target_ratio = 0.80;
  options.preserve_feature_curves = 1;
  options.weight_mode = LQ_WEIGHT_MODE_NORMAL_TENSOR;
  options.feature_angle_deg = 179.0;
  options.normal_tensor_feature_threshold = 0.06;
  options.normal_tensor_min_edge_alignment = 0.2;
  options.normal_tensor_smoothing_iterations = 1;

  LqSimplifyReport report;
  EXPECT_EQ(LQ_STATUS_OK, lq_simplify_mesh(context, input, &options, output, &report));
  EXPECT_GT(report.normal_tensor_feature_edges, 0);

  lq_mesh_destroy(output);
  lq_mesh_destroy(input);
}

TEST_F(CApiTest, ExposesLegalityOptionsAndDetailedRejectDiagnostics) {
  LqMeshHandle* input = lq_mesh_create(context);
  LqMeshHandle* output = lq_mesh_create(context);
  ASSERT_NE(input, nullptr);
  ASSERT_NE(output, nullptr);

  ASSERT_EQ(LQ_STATUS_OK, lq_generate_mesh(context, "plane", 8, input));

  LqSimplifyOptions options;
  lq_simplify_options_init(&options);
  options.target_ratio = 0.08;
  options.use_line_quadrics = 0;
  options.preserve_boundary = 1;
  options.min_triangle_quality = 0.80;
  options.max_normal_deviation_deg = 180.0;

  LqSimplifyReport report;
  EXPECT_EQ(LQ_STATUS_OK, lq_simplify_mesh(context, input, &options, output, &report));
  EXPECT_GT(report.boundary_rejected_collapses, 0);
  EXPECT_GT(report.quality_rejected_collapses, 0);
  EXPECT_EQ(
      report.rejected_collapses,
      report.feature_rejected_collapses + report.boundary_rejected_collapses +
          report.topology_rejected_collapses + report.normal_flip_rejected_collapses +
          report.quality_rejected_collapses +
          report.self_intersection_rejected_collapses +
          report.curve_budget_rejected_collapses + report.error_rejected_collapses);

  lq_mesh_destroy(output);
  lq_mesh_destroy(input);
}

TEST_F(CApiTest, ExposesFeatureCurveProtectionDiagnostics) {
  LqMeshHandle* input = lq_mesh_create(context);
  LqMeshHandle* output = lq_mesh_create(context);
  ASSERT_NE(input, nullptr);
  ASSERT_NE(output, nullptr);

  const LqVec3 vertices[] = {
      {0.0, 0.0, 0.0}, {3.0, 0.0, 0.0},  {2.5, 1.0, 0.0},
      {1.2, 2.2, 0.0}, {-0.4, 1.3, 0.0},
  };
  const LqFace faces[] = {
      {{0, 1, 3}},
      {{1, 2, 3}},
      {{0, 3, 4}},
  };
  ASSERT_EQ(LQ_STATUS_OK, lq_mesh_set_data(context, input, vertices, 5, faces, 3));

  LqSimplifyOptions options;
  lq_simplify_options_init(&options);
  options.target_faces = 1;
  options.target_ratio = 0.25;
  options.use_line_quadrics = 0;
  options.preserve_feature_curves = 1;
  options.protect_all_feature_edges = 0;
  options.feature_protection_mode = LQ_FEATURE_PROTECTION_ALL_FEATURE_EDGES;
  options.use_normal_tensor_features = 0;
  options.feature_angle_deg = 179.0;
  options.circle_fit_relative_threshold = 0.0;
  options.ellipse_fit_relative_threshold = 0.0;
  options.min_feature_loop_vertices = 3;
  options.max_feature_curve_deviation_ratio = 1e-9;
  options.max_normal_deviation_deg = 180.0;
  options.min_triangle_quality = 0.0;

  LqSimplifyReport report;
  EXPECT_EQ(LQ_STATUS_OK, lq_simplify_mesh(context, input, &options, output, &report));
  EXPECT_GT(report.feature_loops, 0);
  EXPECT_GT(report.feature_rejected_collapses, 0);
  EXPECT_GT(report.generic_feature_rejected_collapses, 0);
  EXPECT_EQ(LQ_SIMPLIFY_TERMINATION_REJECTION_LIMIT, report.termination_reason);
  EXPECT_EQ(
      report.rejected_collapses,
      report.feature_rejected_collapses + report.boundary_rejected_collapses +
          report.topology_rejected_collapses + report.normal_flip_rejected_collapses +
          report.quality_rejected_collapses +
          report.self_intersection_rejected_collapses +
          report.curve_budget_rejected_collapses + report.error_rejected_collapses);

  lq_mesh_destroy(output);
  lq_mesh_destroy(input);
}

TEST_F(CApiTest, InitializesPrimitiveFitOptions) {
  LqSimplifyOptions options;
  lq_simplify_options_init(&options);

  EXPECT_EQ(sizeof(LqSimplifyOptions), options.struct_size);
  EXPECT_EQ(LQ_ABI_VERSION, options.abi_version);
  EXPECT_DOUBLE_EQ(0.05, options.circle_fit_relative_threshold);
  EXPECT_DOUBLE_EQ(0.05, options.ellipse_fit_relative_threshold);
  EXPECT_DOUBLE_EQ(0.08, options.near_circle_axis_ratio_tolerance);
  EXPECT_DOUBLE_EQ(0.0, options.max_feature_curve_deviation_ratio);
  EXPECT_EQ(6, options.min_circular_feature_loop_vertices);
  EXPECT_EQ(0, options.preserve_boundary);
  EXPECT_DOUBLE_EQ(0.0, options.min_triangle_quality);
  EXPECT_DOUBLE_EQ(90.0, options.max_normal_deviation_deg);
  EXPECT_EQ(1, options.normal_tensor_scale_count);
  EXPECT_DOUBLE_EQ(0.0, options.max_local_error);
  EXPECT_DOUBLE_EQ(0.0, options.max_local_error_ratio);
  EXPECT_EQ(0, options.prevent_local_intersections);
  EXPECT_EQ(LQ_FEATURE_PROTECTION_PRIMITIVE_CURVES,
            options.feature_protection_mode);

  LqSimplifyReport report;
  lq_simplify_report_init(&report);
  EXPECT_EQ(sizeof(LqSimplifyReport), report.struct_size);
  EXPECT_EQ(LQ_ABI_VERSION, report.abi_version);

  LqMeshStats stats;
  lq_mesh_stats_init(&stats);
  EXPECT_EQ(sizeof(LqMeshStats), stats.struct_size);
  EXPECT_EQ(LQ_ABI_VERSION, stats.abi_version);
}
