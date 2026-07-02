#include "line_quadrics_qem/api/CApi.h"

#include <filesystem>
#include <gtest/gtest.h>
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

std::filesystem::path dataRoot() {
#ifdef LQ_TEST_DATA_DIR
  return std::filesystem::path(LQ_TEST_DATA_DIR);
#else
  return std::filesystem::path(__FILE__).parent_path() / "data";
#endif
}

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
