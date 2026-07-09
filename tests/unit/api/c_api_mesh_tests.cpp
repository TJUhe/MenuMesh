#include "CApiTestSupport.h"

#include <cstddef>
#include <gtest/gtest.h>
#include <limits>
#include <string>
#include <vector>

using manumesh::test::dataRoot;
TEST_F(CApiTest, SimplifiesRealStlThroughOpaqueHandles) {
  ManuMeshMeshHandle* input = manumesh_mesh_create(context);
  ManuMeshMeshHandle* output = manumesh_mesh_create(context);
  ASSERT_NE(input, nullptr);
  ASSERT_NE(output, nullptr);

  const std::string inputPath =
      (dataRoot() / "external" / "nasa_antenna_azimuth_track.stl").string();
  EXPECT_EQ(MANUMESH_STATUS_OK,
            manumesh_load_mesh(context, inputPath.c_str(), input, 1e-8));

  size_t inputVertices = 0;
  size_t inputFaces = 0;
  EXPECT_EQ(MANUMESH_STATUS_OK,
            manumesh_mesh_get_counts(context, input, &inputVertices, &inputFaces));
  EXPECT_GT(inputVertices, 0u);
  EXPECT_GT(inputFaces, 0u);

  ManuMeshSimplifyOptions options;
  manumesh_simplify_options_init(&options);
  options.target_ratio = 0.80;
  options.weight_mode = MANUMESH_WEIGHT_MODE_DIHEDRAL;
  options.feature_boost = 0.08;
  options.feature_angle_deg = 25.0;

  ManuMeshSimplifyReport report;
  EXPECT_EQ(MANUMESH_STATUS_OK,
            manumesh_simplify_mesh(context, input, &options, output, &report));
  EXPECT_EQ(static_cast<int>(inputFaces), report.initial_faces);
  EXPECT_GT(report.collapsed_edges, 0);
  EXPECT_LT(report.final_faces, report.initial_faces);
  EXPECT_EQ(MANUMESH_SIMPLIFY_TERMINATION_REACHED_TARGET, report.termination_reason);

  ManuMeshMeshStats stats;
  EXPECT_EQ(MANUMESH_STATUS_OK, manumesh_compute_mesh_stats(context, output, &stats));
  EXPECT_EQ(report.final_faces, stats.faces);
  EXPECT_GT(stats.area, 0.0);

  manumesh_mesh_destroy(output);
  manumesh_mesh_destroy(input);
}

TEST_F(CApiTest, CopiesMeshDataOnlyIntoCallerOwnedBuffers) {
  ManuMeshMeshHandle* mesh = manumesh_mesh_create(context);
  ASSERT_NE(mesh, nullptr);

  const ManuMeshVec3 vertices[] = {
      {0.0, 0.0, 0.0},
      {1.0, 0.0, 0.0},
      {0.0, 1.0, 0.0},
  };
  const ManuMeshFace faces[] = {{{0, 1, 2}}};
  EXPECT_EQ(MANUMESH_STATUS_OK,
            manumesh_mesh_set_data(context, mesh, vertices, 3, faces, 1));

  size_t required = 0;
  EXPECT_EQ(MANUMESH_STATUS_BUFFER_TOO_SMALL,
            manumesh_mesh_copy_vertices(context, mesh, nullptr, 0, &required));
  EXPECT_EQ(3u, required);

  std::vector<ManuMeshVec3> copiedVertices(required);
  EXPECT_EQ(MANUMESH_STATUS_OK,
            manumesh_mesh_copy_vertices(context, mesh, copiedVertices.data(),
                                        copiedVertices.size(), &required));
  EXPECT_EQ(1.0, copiedVertices[1].x);

  size_t copiedFaces = 0;
  ManuMeshFace copiedFace;
  EXPECT_EQ(MANUMESH_STATUS_OK,
            manumesh_mesh_copy_faces(context, mesh, &copiedFace, 1, &copiedFaces));
  EXPECT_EQ(1u, copiedFaces);
  EXPECT_EQ(2, copiedFace.v[2]);

  manumesh_mesh_destroy(mesh);
}

TEST_F(CApiTest, RejectsNonFiniteVertexCoordinatesWithoutReplacingMesh) {
  ManuMeshMeshHandle* mesh = manumesh_mesh_create(context);
  ASSERT_NE(mesh, nullptr);

  const ManuMeshVec3 vertices[] = {
      {0.0, 0.0, 0.0},
      {1.0, 0.0, 0.0},
      {0.0, 1.0, 0.0},
  };
  const ManuMeshFace faces[] = {{{0, 1, 2}}};
  ASSERT_EQ(MANUMESH_STATUS_OK,
            manumesh_mesh_set_data(context, mesh, vertices, 3, faces, 1));

  const ManuMeshVec3 invalidVertices[] = {
      {0.0, 0.0, 0.0},
      {std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0},
      {0.0, 1.0, 0.0},
  };
  EXPECT_EQ(MANUMESH_STATUS_INVALID_ARGUMENT,
            manumesh_mesh_set_data(context, mesh, invalidVertices, 3, faces, 1));
  EXPECT_NE('\0', manumesh_context_last_error(context)[0]);

  size_t vertexCount = 0;
  size_t faceCount = 0;
  EXPECT_EQ(MANUMESH_STATUS_OK,
            manumesh_mesh_get_counts(context, mesh, &vertexCount, &faceCount));
  EXPECT_EQ(3u, vertexCount);
  EXPECT_EQ(1u, faceCount);

  manumesh_mesh_destroy(mesh);
}

TEST_F(CApiTest, RejectsInvalidFaceIndicesWithoutReplacingMesh) {
  ManuMeshMeshHandle* mesh = manumesh_mesh_create(context);
  ASSERT_NE(mesh, nullptr);

  const ManuMeshVec3 vertices[] = {
      {0.0, 0.0, 0.0},
      {1.0, 0.0, 0.0},
      {0.0, 1.0, 0.0},
  };
  const ManuMeshFace validFaces[] = {{{0, 1, 2}}};
  ASSERT_EQ(MANUMESH_STATUS_OK,
            manumesh_mesh_set_data(context, mesh, vertices, 3, validFaces, 1));

  const ManuMeshFace invalidFaces[] = {{{0, 1, 5}}};
  EXPECT_EQ(MANUMESH_STATUS_INVALID_ARGUMENT,
            manumesh_mesh_set_data(context, mesh, vertices, 3, invalidFaces, 1));
  EXPECT_NE('\0', manumesh_context_last_error(context)[0]);

  size_t vertexCount = 0;
  size_t faceCount = 0;
  EXPECT_EQ(MANUMESH_STATUS_OK,
            manumesh_mesh_get_counts(context, mesh, &vertexCount, &faceCount));
  EXPECT_EQ(3u, vertexCount);
  EXPECT_EQ(1u, faceCount);

  manumesh_mesh_destroy(mesh);
}

TEST_F(CApiTest, RejectsDegenerateFacesWithoutReplacingMesh) {
  ManuMeshMeshHandle* mesh = manumesh_mesh_create(context);
  ASSERT_NE(mesh, nullptr);

  const ManuMeshVec3 vertices[] = {
      {0.0, 0.0, 0.0},
      {1.0, 0.0, 0.0},
      {0.0, 1.0, 0.0},
  };
  const ManuMeshFace validFaces[] = {{{0, 1, 2}}};
  ASSERT_EQ(MANUMESH_STATUS_OK,
            manumesh_mesh_set_data(context, mesh, vertices, 3, validFaces, 1));

  const ManuMeshFace repeatedVertexFaces[] = {{{0, 1, 1}}};
  EXPECT_EQ(MANUMESH_STATUS_INVALID_ARGUMENT,
            manumesh_mesh_set_data(context, mesh, vertices, 3, repeatedVertexFaces, 1));
  EXPECT_NE('\0', manumesh_context_last_error(context)[0]);

  const ManuMeshVec3 collinearVertices[] = {
      {0.0, 0.0, 0.0},
      {1.0, 0.0, 0.0},
      {2.0, 0.0, 0.0},
  };
  EXPECT_EQ(MANUMESH_STATUS_INVALID_ARGUMENT,
            manumesh_mesh_set_data(context, mesh, collinearVertices, 3, validFaces, 1));
  EXPECT_NE('\0', manumesh_context_last_error(context)[0]);

  size_t vertexCount = 0;
  size_t faceCount = 0;
  EXPECT_EQ(MANUMESH_STATUS_OK,
            manumesh_mesh_get_counts(context, mesh, &vertexCount, &faceCount));
  EXPECT_EQ(3u, vertexCount);
  EXPECT_EQ(1u, faceCount);

  manumesh_mesh_destroy(mesh);
}
