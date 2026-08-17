/**
 * @file tests/unit/api/c_api_mesh_tests.cpp
 * @brief 验证 C API 网格缓冲区、错误处理以及 STL 往返契约。
 * @ingroup manumesh_tests
 */

#include "CApiTestSupport.h"

#include "core/Filesystem.h"
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <gtest/gtest.h>
#include <limits>
#include <string>
#include <thread>
#include <vector>

static_assert(sizeof(ManuMeshVec2) == 16, "ManuMeshVec2 ABI size changed");
static_assert(sizeof(ManuMeshVec3) == 24, "ManuMeshVec3 ABI size changed");
static_assert(sizeof(ManuMeshFace) == 12, "ManuMeshFace ABI size changed");
static_assert(sizeof(ManuMeshFaceTexCoords) == 56, "ManuMeshFaceTexCoords ABI size changed");
static_assert(offsetof(ManuMeshFaceTexCoords, valid) == 48, "ManuMeshFaceTexCoords ABI offset changed");
static_assert(sizeof(ManuMeshEdge) == 24, "ManuMeshEdge ABI size changed");
static_assert(offsetof(ManuMeshEdge, face_count) == 8, "ManuMeshEdge ABI offset changed");
static_assert(sizeof(ManuMeshBounds) == 56, "ManuMeshBounds ABI size changed");
static_assert(sizeof(ManuMeshTopologySummary) == 40, "ManuMeshTopologySummary ABI size changed");
static_assert(offsetof(ManuMeshTopologySummary, closed_manifold) == 32, "ManuMeshTopologySummary ABI offset changed");

TEST_F(CApiTest, CopiesMeshDataOnlyIntoCallerOwnedBuffers) {
    ManuMeshMeshHandle* mesh = manumesh_mesh_create(context);
    ASSERT_NE(mesh, nullptr);

    const ManuMeshVec3 vertices[] = {
        {0.0, 0.0, 0.0},
        {1.0, 0.0, 0.0},
        {0.0, 1.0, 0.0},
    };
    const ManuMeshFace faces[] = {{{0, 1, 2}}};
    EXPECT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_set_data(context, mesh, vertices, 3, faces, 1));

    size_t required = 0;
    EXPECT_EQ(MANUMESH_STATUS_BUFFER_TOO_SMALL, manumesh_mesh_copy_vertices(context, mesh, nullptr, 0, &required));
    EXPECT_EQ(3u, required);

    ManuMeshVec3 undersizedVertices[] = {
        {11.0, 12.0, 13.0},
        {21.0, 22.0, 23.0},
    };
    EXPECT_EQ(
        MANUMESH_STATUS_BUFFER_TOO_SMALL, manumesh_mesh_copy_vertices(context, mesh, undersizedVertices, 2, &required)
    );
    EXPECT_EQ(3u, required);
    EXPECT_DOUBLE_EQ(11.0, undersizedVertices[0].x);
    EXPECT_DOUBLE_EQ(12.0, undersizedVertices[0].y);
    EXPECT_DOUBLE_EQ(13.0, undersizedVertices[0].z);
    EXPECT_DOUBLE_EQ(21.0, undersizedVertices[1].x);
    EXPECT_DOUBLE_EQ(22.0, undersizedVertices[1].y);
    EXPECT_DOUBLE_EQ(23.0, undersizedVertices[1].z);

    std::vector<ManuMeshVec3> copiedVertices(required);
    EXPECT_EQ(
        MANUMESH_STATUS_OK,
        manumesh_mesh_copy_vertices(context, mesh, copiedVertices.data(), copiedVertices.size(), &required)
    );
    EXPECT_EQ(1.0, copiedVertices[1].x);

    size_t copiedFaces = 0;
    ManuMeshFace copiedFace = {{7, 8, 9}};
    EXPECT_EQ(MANUMESH_STATUS_BUFFER_TOO_SMALL, manumesh_mesh_copy_faces(context, mesh, &copiedFace, 0, &copiedFaces));
    EXPECT_EQ(1u, copiedFaces);
    EXPECT_EQ(7, copiedFace.v[0]);
    EXPECT_EQ(8, copiedFace.v[1]);
    EXPECT_EQ(9, copiedFace.v[2]);

    EXPECT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_copy_faces(context, mesh, &copiedFace, 1, &copiedFaces));
    EXPECT_EQ(1u, copiedFaces);
    EXPECT_EQ(2, copiedFace.v[2]);

    manumesh_mesh_destroy(mesh);
}

TEST(CApiMeshCreate, AllowsNullErrorContext) {
    ManuMeshMeshHandle* mesh = manumesh_mesh_create(nullptr);
    ASSERT_NE(mesh, nullptr);
    EXPECT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_clear(nullptr, mesh));
    EXPECT_EQ(MANUMESH_STATUS_INVALID_ARGUMENT, manumesh_mesh_clear(nullptr, nullptr));
    manumesh_mesh_destroy(mesh);
}

TEST(CApiContext, AllowsNullContextForErrorQueries) {
    EXPECT_STREQ("ManuMeshContext is null.", manumesh_context_last_error(nullptr));
    manumesh_context_clear_error(nullptr);
}

TEST_F(CApiTest, PreservesFaceIndexOrderAcrossCAbiBoundary) {
    ManuMeshMeshHandle* mesh = manumesh_mesh_create(context);
    ASSERT_NE(mesh, nullptr);

    const ManuMeshVec3 vertices[] = {
        {0.0, 0.0, 0.0},
        {1.0, 0.0, 0.0},
        {0.0, 1.0, 0.0},
    };
    const ManuMeshFace inputFace[] = {{{2, 0, 1}}};
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_set_data(context, mesh, vertices, 3, inputFace, 1));

    ManuMeshFace outputFace{};
    size_t written = 0;
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_copy_faces(context, mesh, &outputFace, 1, &written));
    ASSERT_EQ(1u, written);
    EXPECT_EQ(inputFace[0].v[0], outputFace.v[0]);
    EXPECT_EQ(inputFace[0].v[1], outputFace.v[1]);
    EXPECT_EQ(inputFace[0].v[2], outputFace.v[2]);

    manumesh_mesh_destroy(mesh);
}

TEST_F(CApiTest, SupportsDeepCopyIndexedAccessBoundsAndAtomicTransforms) {
    ManuMeshMeshHandle* source = manumesh_mesh_create(context);
    ManuMeshMeshHandle* copy = manumesh_mesh_create(context);
    ASSERT_NE(source, nullptr);
    ASSERT_NE(copy, nullptr);
    const ManuMeshVec3 vertices[] = {{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}};
    const ManuMeshFace faces[] = {{{0, 1, 2}}};
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_set_data(context, source, vertices, 3, faces, 1));
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_copy(context, source, copy));

    ManuMeshVec3 vertex = {9.0, 9.0, 9.0};
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_get_vertex(context, copy, 1, &vertex));
    EXPECT_DOUBLE_EQ(1.0, vertex.x);
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_set_vertex(context, copy, 1, ManuMeshVec3{2.0, 0.0, 0.0}));
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_get_vertex(context, source, 1, &vertex));
    EXPECT_DOUBLE_EQ(1.0, vertex.x);

    ManuMeshFace face = {{7, 8, 9}};
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_get_face(context, copy, 0, &face));
    EXPECT_EQ(0, face.v[0]);
    EXPECT_EQ(2, face.v[2]);
    const ManuMeshFace invalidFace = {{0, 1, 99}};
    EXPECT_EQ(MANUMESH_STATUS_INVALID_MESH, manumesh_mesh_set_face(context, copy, 0, invalidFace));
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_get_face(context, copy, 0, &face));
    EXPECT_EQ(2, face.v[2]);

    ManuMeshBounds bounds{};
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_get_bounds(context, copy, &bounds));
    EXPECT_EQ(1, bounds.valid);
    EXPECT_DOUBLE_EQ(2.0, bounds.max.x);
    EXPECT_DOUBLE_EQ(1.0, bounds.max.y);

    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_translate(context, copy, ManuMeshVec3{1.0, 2.0, 3.0}));
    const double transform[16] = {
        2.0,
        0.0,
        0.0,
        0.0,
        0.0,
        3.0,
        0.0,
        0.0,
        0.0,
        0.0,
        4.0,
        0.0,
        0.0,
        0.0,
        0.0,
        1.0,
    };
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_transform(context, copy, transform));
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_get_vertex(context, copy, 0, &vertex));
    EXPECT_DOUBLE_EQ(2.0, vertex.x);
    EXPECT_DOUBLE_EQ(6.0, vertex.y);
    EXPECT_DOUBLE_EQ(12.0, vertex.z);

    double invalidTransform[16] = {};
    EXPECT_EQ(MANUMESH_STATUS_INVALID_MESH, manumesh_mesh_transform(context, copy, invalidTransform));
    ManuMeshVec3 afterFailure{};
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_get_vertex(context, copy, 0, &afterFailure));
    EXPECT_DOUBLE_EQ(vertex.x, afterFailure.x);
    EXPECT_DOUBLE_EQ(vertex.y, afterFailure.y);
    EXPECT_DOUBLE_EQ(vertex.z, afterFailure.z);

    const double nan = std::numeric_limits<double>::quiet_NaN();
    EXPECT_EQ(MANUMESH_STATUS_INVALID_ARGUMENT, manumesh_mesh_set_vertex(context, copy, 0, ManuMeshVec3{nan, 0, 0}));
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_get_vertex(context, copy, 0, &afterFailure));
    EXPECT_DOUBLE_EQ(vertex.x, afterFailure.x);

    manumesh_mesh_destroy(copy);
    manumesh_mesh_destroy(source);
}

TEST_F(CApiTest, ComputesAreasCentroidsNormalsAndClassifiedUniqueEdgesWithoutPartialWrites) {
    ManuMeshMeshHandle* mesh = manumesh_mesh_create(context);
    ASSERT_NE(mesh, nullptr);
    const ManuMeshVec3 vertices[] = {
        {0.0, 0.0, 0.0},
        {1.0, 0.0, 0.0},
        {0.0, 1.0, 0.0},
        {1.0, 1.0, 0.0},
    };
    const ManuMeshFace faces[] = {{{0, 1, 2}}, {{1, 3, 2}}};
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_set_data(context, mesh, vertices, 4, faces, 2));

    size_t written = 0;
    EXPECT_EQ(MANUMESH_STATUS_BUFFER_TOO_SMALL, manumesh_mesh_copy_face_areas(context, mesh, nullptr, 0, &written));
    ASSERT_EQ(2u, written);
    double sentinelArea = 123.0;
    EXPECT_EQ(
        MANUMESH_STATUS_BUFFER_TOO_SMALL, manumesh_mesh_copy_face_areas(context, mesh, &sentinelArea, 1, &written)
    );
    EXPECT_DOUBLE_EQ(123.0, sentinelArea);
    std::array<double, 2> areas{};
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_copy_face_areas(context, mesh, areas.data(), areas.size(), &written));
    EXPECT_DOUBLE_EQ(0.5, areas[0]);
    EXPECT_DOUBLE_EQ(0.5, areas[1]);
    double surfaceArea = -1.0;
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_get_surface_area(context, mesh, &surfaceArea));
    EXPECT_DOUBLE_EQ(1.0, surfaceArea);
    ManuMeshVec3 surfaceCentroid{-1.0, -1.0, -1.0};
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_get_surface_centroid(context, mesh, &surfaceCentroid));
    EXPECT_DOUBLE_EQ(0.5, surfaceCentroid.x);
    EXPECT_DOUBLE_EQ(0.5, surfaceCentroid.y);
    EXPECT_DOUBLE_EQ(0.0, surfaceCentroid.z);

    std::array<ManuMeshVec3, 2> centroids{};
    ASSERT_EQ(
        MANUMESH_STATUS_OK,
        manumesh_mesh_copy_face_centroids(context, mesh, centroids.data(), centroids.size(), &written)
    );
    EXPECT_NEAR(1.0 / 3.0, centroids[0].x, 1e-12);
    EXPECT_NEAR(1.0 / 3.0, centroids[0].y, 1e-12);

    std::array<ManuMeshVec3, 2> faceNormals{};
    ASSERT_EQ(
        MANUMESH_STATUS_OK,
        manumesh_mesh_copy_face_normals(context, mesh, faceNormals.data(), faceNormals.size(), &written)
    );
    for (const ManuMeshVec3& normal : faceNormals) {
        EXPECT_DOUBLE_EQ(0.0, normal.x);
        EXPECT_DOUBLE_EQ(0.0, normal.y);
        EXPECT_DOUBLE_EQ(1.0, normal.z);
    }

    std::array<ManuMeshVec3, 4> vertexNormals{};
    ASSERT_EQ(
        MANUMESH_STATUS_OK,
        manumesh_mesh_copy_vertex_normals(context, mesh, vertexNormals.data(), vertexNormals.size(), &written)
    );
    for (const ManuMeshVec3& normal : vertexNormals) {
        EXPECT_DOUBLE_EQ(1.0, normal.z);
    }

    EXPECT_EQ(MANUMESH_STATUS_BUFFER_TOO_SMALL, manumesh_mesh_copy_unique_edges(context, mesh, nullptr, 0, &written));
    ASSERT_EQ(5u, written);
    ManuMeshEdge sentinelEdge = {9, 10, 11, 12, 13};
    EXPECT_EQ(
        MANUMESH_STATUS_BUFFER_TOO_SMALL, manumesh_mesh_copy_unique_edges(context, mesh, &sentinelEdge, 1, &written)
    );
    EXPECT_EQ(9, sentinelEdge.a);
    EXPECT_EQ(11u, sentinelEdge.face_count);
    std::vector<ManuMeshEdge> edges(written);
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_copy_unique_edges(context, mesh, edges.data(), edges.size(), &written));
    int boundaryEdges = 0;
    int interiorEdges = 0;
    for (const ManuMeshEdge& edge : edges) {
        boundaryEdges += edge.boundary;
        interiorEdges += edge.face_count == 2 ? 1 : 0;
        EXPECT_EQ(0, edge.non_manifold);
        EXPECT_LT(edge.a, edge.b);
    }
    EXPECT_EQ(4, boundaryEdges);
    EXPECT_EQ(1, interiorEdges);

    manumesh_mesh_destroy(mesh);
}

TEST_F(CApiTest, AreaAndNormalQueriesAgreeOnSubToleranceDegenerateFaces) {
    ManuMeshMeshHandle* mesh = manumesh_mesh_create(context);
    ASSERT_NE(mesh, nullptr);
    const ManuMeshVec3 vertices[] = {
        {0.0, 0.0, 0.0},
        {1e-13, 0.0, 0.0},
        {0.0, 1e-13, 0.0},
    };
    const ManuMeshFace face[] = {{{0, 1, 2}}};
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_set_data(context, mesh, vertices, 3, face, 1));

    size_t written = 0;
    double area = -1.0;
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_copy_face_areas(context, mesh, &area, 1, &written));
    EXPECT_EQ(1u, written);
    EXPECT_DOUBLE_EQ(0.0, area);
    area = -1.0;
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_get_surface_area(context, mesh, &area));
    EXPECT_DOUBLE_EQ(0.0, area);

    ManuMeshVec3 normal{1.0, 1.0, 1.0};
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_copy_face_normals(context, mesh, &normal, 1, &written));
    EXPECT_DOUBLE_EQ(0.0, normal.x);
    EXPECT_DOUBLE_EQ(0.0, normal.y);
    EXPECT_DOUBLE_EQ(0.0, normal.z);

    manumesh_mesh_destroy(mesh);
}

TEST_F(CApiTest, SurfaceQueriesPreserveAreaForExtremeFiniteAspectRatios) {
    ManuMeshMeshHandle* mesh = manumesh_mesh_create(context);
    ASSERT_NE(mesh, nullptr);
    const ManuMeshVec3 vertices[] = {
        {0.0, 0.0, 0.0},
        {1e100, 0.0, 0.0},
        {0.0, 1e-100, 0.0},
    };
    const ManuMeshFace face[] = {{{0, 1, 2}}};
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_set_data(context, mesh, vertices, 3, face, 1));
    EXPECT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_validate(context, mesh, 1, nullptr));

    double surfaceArea = 0.0;
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_get_surface_area(context, mesh, &surfaceArea));
    EXPECT_DOUBLE_EQ(0.5, surfaceArea);
    size_t written = 0;
    ManuMeshVec3 normal{};
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_copy_face_normals(context, mesh, &normal, 1, &written));
    EXPECT_DOUBLE_EQ(1.0, normal.z);

    manumesh_mesh_destroy(mesh);
}

TEST_F(CApiTest, ComputesStableVertexNormalsForLargeFiniteCoordinates) {
    ManuMeshMeshHandle* mesh = manumesh_mesh_create(context);
    ASSERT_NE(mesh, nullptr);
    const double scale = 1e150;
    const ManuMeshVec3 vertices[] = {{0.0, 0.0, 0.0}, {scale, 0.0, 0.0}, {0.0, scale, 0.0}};
    const ManuMeshFace faces[] = {{{0, 1, 2}}};
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_set_data(context, mesh, vertices, 3, faces, 1));

    std::array<ManuMeshVec3, 3> normals{};
    size_t written = 0;
    ASSERT_EQ(
        MANUMESH_STATUS_OK, manumesh_mesh_copy_vertex_normals(context, mesh, normals.data(), normals.size(), &written)
    );
    ASSERT_EQ(normals.size(), written);
    for (const ManuMeshVec3& normal : normals) {
        EXPECT_DOUBLE_EQ(0.0, normal.x);
        EXPECT_DOUBLE_EQ(0.0, normal.y);
        EXPECT_DOUBLE_EQ(1.0, normal.z);
    }

    manumesh_mesh_destroy(mesh);
}

TEST_F(CApiTest, RejectsNonFiniteAggregateStatsWithoutOverwritingOutput) {
    ManuMeshMeshHandle* mesh = manumesh_mesh_create(context);
    ASSERT_NE(mesh, nullptr);

    // Every triangle area is finite and passes the supported coordinate-range
    // check, while their aggregate area exceeds double's finite range.
    const double extent = 8e152;
    const ManuMeshVec3 vertices[] = {
        {0.0, 0.0, 0.0},
        {extent, 0.0, 0.0},
        {0.0, extent, 0.0},
    };
    std::vector<ManuMeshFace> faces(600, ManuMeshFace{{0, 1, 2}});
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_set_data(context, mesh, vertices, 3, faces.data(), faces.size()));

    double surfaceArea = 123.0;
    EXPECT_EQ(MANUMESH_STATUS_INVALID_MESH, manumesh_mesh_get_surface_area(context, mesh, &surfaceArea));
    EXPECT_DOUBLE_EQ(123.0, surfaceArea);

    ManuMeshMeshStats stats{};
    stats.faces = -17;
    stats.area = 456.0;
    EXPECT_EQ(
        MANUMESH_STATUS_INVALID_MESH, manumesh_compute_mesh_stats_with_size(context, mesh, &stats, sizeof(stats))
    );
    EXPECT_EQ(-17, stats.faces);
    EXPECT_DOUBLE_EQ(456.0, stats.area);

    manumesh_mesh_destroy(mesh);
}

TEST_F(CApiTest, RoundTripsPerFaceTextureCoordinatesAndReversesCornersWithWinding) {
    ManuMeshMeshHandle* mesh = manumesh_mesh_create(context);
    ASSERT_NE(mesh, nullptr);
    const ManuMeshVec3 vertices[] = {{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}};
    const ManuMeshFace faces[] = {{{0, 1, 2}}};
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_set_data(context, mesh, vertices, 3, faces, 1));

    int hasTexcoords = 7;
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_has_texture_coordinates(context, mesh, &hasTexcoords));
    EXPECT_EQ(0, hasTexcoords);
    ManuMeshFaceTexCoords read{};
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_get_face_texcoords(context, mesh, 0, &read));
    EXPECT_EQ(0, read.valid);
    EXPECT_DOUBLE_EQ(0.0, read.uv[2].u);

    ManuMeshFaceTexCoords input{};
    input.valid = 1;
    input.uv[0] = ManuMeshVec2{0.0, 0.0};
    input.uv[1] = ManuMeshVec2{1.0, 0.0};
    input.uv[2] = ManuMeshVec2{0.0, 1.0};
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_set_face_texcoords(context, mesh, 0, &input));
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_has_texture_coordinates(context, mesh, &hasTexcoords));
    EXPECT_EQ(1, hasTexcoords);

    const double nan = std::numeric_limits<double>::quiet_NaN();
    ManuMeshFaceTexCoords invalid = input;
    invalid.uv[1].u = nan;
    EXPECT_EQ(MANUMESH_STATUS_INVALID_ARGUMENT, manumesh_mesh_set_face_texcoords(context, mesh, 0, &invalid));
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_get_face_texcoords(context, mesh, 0, &read));
    EXPECT_DOUBLE_EQ(1.0, read.uv[1].u);

    size_t written = 0;
    ManuMeshFaceTexCoords sentinel{};
    sentinel.valid = 9;
    EXPECT_EQ(
        MANUMESH_STATUS_BUFFER_TOO_SMALL, manumesh_mesh_copy_face_texcoords(context, mesh, &sentinel, 0, &written)
    );
    EXPECT_EQ(9, sentinel.valid);
    EXPECT_EQ(1u, written);

    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_reverse_winding(context, mesh));
    ManuMeshFace face{};
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_get_face(context, mesh, 0, &face));
    EXPECT_EQ(0, face.v[0]);
    EXPECT_EQ(2, face.v[1]);
    EXPECT_EQ(1, face.v[2]);
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_get_face_texcoords(context, mesh, 0, &read));
    EXPECT_DOUBLE_EQ(0.0, read.uv[1].u);
    EXPECT_DOUBLE_EQ(1.0, read.uv[1].v);
    EXPECT_DOUBLE_EQ(1.0, read.uv[2].u);
    EXPECT_DOUBLE_EQ(0.0, read.uv[2].v);

    input.valid = 0;
    input.uv[0] = ManuMeshVec2{99.0, 99.0};
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_set_face_texcoords(context, mesh, 0, &input));
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_get_face_texcoords(context, mesh, 0, &read));
    EXPECT_EQ(0, read.valid);
    EXPECT_DOUBLE_EQ(0.0, read.uv[0].u);
    EXPECT_DOUBLE_EQ(0.0, read.uv[0].v);

    manumesh_mesh_destroy(mesh);
}

TEST_F(CApiTest, StrictValidationReportsDegenerateCountAndCleanupCompactsMesh) {
    ManuMeshMeshHandle* mesh = manumesh_mesh_create(context);
    ASSERT_NE(mesh, nullptr);
    const ManuMeshVec3 vertices[] = {
        {0.0, 0.0, 0.0},
        {1.0, 0.0, 0.0},
        {2.0, 0.0, 0.0},
        {9.0, 9.0, 9.0},
    };
    const ManuMeshFace faces[] = {{{0, 1, 2}}};
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_set_data(context, mesh, vertices, 4, faces, 1));

    size_t degenerateCount = 99;
    EXPECT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_validate(context, mesh, 0, &degenerateCount));
    EXPECT_EQ(1u, degenerateCount);
    degenerateCount = 99;
    EXPECT_EQ(MANUMESH_STATUS_INVALID_MESH, manumesh_mesh_validate(context, mesh, 1, &degenerateCount));
    EXPECT_EQ(1u, degenerateCount);

    size_t removed = 99;
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_remove_degenerate_faces(context, mesh, &removed));
    EXPECT_EQ(1u, removed);
    size_t vertexCount = 99;
    size_t faceCount = 99;
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_get_counts(context, mesh, &vertexCount, &faceCount));
    EXPECT_EQ(0u, vertexCount);
    EXPECT_EQ(0u, faceCount);

    const ManuMeshVec3 compactVertices[] = {
        {0.0, 0.0, 0.0},
        {1.0, 0.0, 0.0},
        {0.0, 1.0, 0.0},
        {9.0, 9.0, 9.0},
    };
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_set_data(context, mesh, compactVertices, 4, faces, 1));
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_compact(context, mesh));
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_get_counts(context, mesh, &vertexCount, &faceCount));
    EXPECT_EQ(3u, vertexCount);
    EXPECT_EQ(1u, faceCount);

    manumesh_mesh_destroy(mesh);
}

TEST_F(CApiTest, AppendsMeshesWithIndexOffsetsTextureAlignmentAndSelfAppend) {
    ManuMeshMeshHandle* destination = manumesh_mesh_create(context);
    ManuMeshMeshHandle* source = manumesh_mesh_create(context);
    ASSERT_NE(destination, nullptr);
    ASSERT_NE(source, nullptr);
    const ManuMeshVec3 destinationVertices[] = {{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}};
    const ManuMeshVec3 sourceVertices[] = {{2.0, 0.0, 0.0}, {3.0, 0.0, 0.0}, {2.0, 1.0, 0.0}};
    const ManuMeshFace face[] = {{{0, 1, 2}}};
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_set_data(context, destination, destinationVertices, 3, face, 1));
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_set_data(context, source, sourceVertices, 3, face, 1));
    ManuMeshFaceTexCoords uv{};
    uv.valid = 1;
    uv.uv[2] = ManuMeshVec2{0.25, 0.75};
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_set_face_texcoords(context, source, 0, &uv));

    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_append(context, destination, source));
    size_t vertexCount = 0;
    size_t faceCount = 0;
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_get_counts(context, destination, &vertexCount, &faceCount));
    EXPECT_EQ(6u, vertexCount);
    EXPECT_EQ(2u, faceCount);
    ManuMeshFace appendedFace{};
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_get_face(context, destination, 1, &appendedFace));
    EXPECT_EQ(3, appendedFace.v[0]);
    EXPECT_EQ(4, appendedFace.v[1]);
    EXPECT_EQ(5, appendedFace.v[2]);
    std::array<ManuMeshFaceTexCoords, 2> texcoords{};
    size_t written = 0;
    ASSERT_EQ(
        MANUMESH_STATUS_OK,
        manumesh_mesh_copy_face_texcoords(context, destination, texcoords.data(), texcoords.size(), &written)
    );
    EXPECT_EQ(0, texcoords[0].valid);
    EXPECT_EQ(1, texcoords[1].valid);
    EXPECT_DOUBLE_EQ(0.75, texcoords[1].uv[2].v);

    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_append(context, destination, destination));
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_get_counts(context, destination, &vertexCount, &faceCount));
    EXPECT_EQ(12u, vertexCount);
    EXPECT_EQ(4u, faceCount);
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_get_face(context, destination, 3, &appendedFace));
    EXPECT_EQ(9, appendedFace.v[0]);
    EXPECT_EQ(10, appendedFace.v[1]);
    EXPECT_EQ(11, appendedFace.v[2]);

    manumesh_mesh_destroy(source);
    manumesh_mesh_destroy(destination);
}

TEST_F(CApiTest, SerializesConcurrentReadsAndWritesOnOneMeshHandle) {
    ManuMeshMeshHandle* mesh = manumesh_mesh_create(context);
    ASSERT_NE(mesh, nullptr);
    const ManuMeshVec3 vertices[] = {{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}};
    const ManuMeshFace faces[] = {{{0, 1, 2}}};
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_set_data(context, mesh, vertices, 3, faces, 1));

    std::atomic<bool> ok{true};
    std::thread writer([&] {
        for (int i = 0; i < 200; ++i) {
            if (manumesh_mesh_translate(nullptr, mesh, ManuMeshVec3{0.001, 0.0, 0.0}) != MANUMESH_STATUS_OK) {
                ok.store(false);
            }
        }
    });
    auto reader = [&] {
        for (int i = 0; i < 200; ++i) {
            size_t vertexCount = 0;
            size_t faceCount = 0;
            if (manumesh_mesh_get_counts(nullptr, mesh, &vertexCount, &faceCount) != MANUMESH_STATUS_OK ||
                vertexCount != 3 || faceCount != 1) {
                ok.store(false);
            }
            std::array<ManuMeshVec3, 3> copied{};
            size_t written = 0;
            if (manumesh_mesh_copy_vertices(nullptr, mesh, copied.data(), copied.size(), &written) !=
                    MANUMESH_STATUS_OK ||
                written != 3) {
                ok.store(false);
            }
        }
    };
    std::thread reader1(reader);
    std::thread reader2(reader);
    writer.join();
    reader1.join();
    reader2.join();
    EXPECT_TRUE(ok.load());
    EXPECT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_validate(context, mesh, 0, nullptr));

    manumesh_mesh_destroy(mesh);
}

TEST_F(CApiTest, LocksReciprocalTwoHandleAppendsWithoutDeadlockOrTornSnapshots) {
    ManuMeshMeshHandle* first = manumesh_mesh_create(context);
    ManuMeshMeshHandle* second = manumesh_mesh_create(context);
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    const ManuMeshVec3 firstVertices[] = {{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}};
    const ManuMeshVec3 secondVertices[] = {{2.0, 0.0, 0.0}, {3.0, 0.0, 0.0}, {2.0, 1.0, 0.0}};
    const ManuMeshFace face[] = {{{0, 1, 2}}};
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_set_data(context, first, firstVertices, 3, face, 1));
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_set_data(context, second, secondVertices, 3, face, 1));

    std::atomic<int> ready{0};
    std::atomic<bool> start{false};
    std::atomic<bool> ok{true};
    auto append = [&](ManuMeshMeshHandle* destination, const ManuMeshMeshHandle* source) {
        ready.fetch_add(1);
        while (!start.load()) {
            std::this_thread::yield();
        }
        if (manumesh_mesh_append(nullptr, destination, source) != MANUMESH_STATUS_OK) {
            ok.store(false);
        }
    };
    std::thread firstThread(append, first, second);
    std::thread secondThread(append, second, first);
    while (ready.load() != 2) {
        std::this_thread::yield();
    }
    start.store(true);
    firstThread.join();
    secondThread.join();
    ASSERT_TRUE(ok.load());

    size_t firstVerticesCount = 0;
    size_t firstFacesCount = 0;
    size_t secondVerticesCount = 0;
    size_t secondFacesCount = 0;
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_get_counts(context, first, &firstVerticesCount, &firstFacesCount));
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_get_counts(context, second, &secondVerticesCount, &secondFacesCount));
    EXPECT_EQ(15u, firstVerticesCount + secondVerticesCount);
    EXPECT_EQ(5u, firstFacesCount + secondFacesCount);
    EXPECT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_validate(context, first, 0, nullptr));
    EXPECT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_validate(context, second, 0, nullptr));

    manumesh_mesh_destroy(second);
    manumesh_mesh_destroy(first);
}

TEST_F(CApiTest, ClearsTextureCoordinatesAlongWithLoadedGeometry) {
    const manumesh::filesystem::path objPath =
        manumesh::filesystem::temp_directory_path() / "manumesh_c_api_clear_textured.obj";
    const manumesh::filesystem::path stlPath =
        manumesh::filesystem::temp_directory_path() / "manumesh_c_api_clear_empty.stl";
    manumesh::filesystem::remove(objPath);
    manumesh::filesystem::remove(stlPath);

    {
        std::ofstream obj(objPath);
        ASSERT_TRUE(obj);
        obj << "v 0 0 0\n"
               "v 1 0 0\n"
               "v 0 1 0\n"
               "vt 0 0\n"
               "vt 1 0\n"
               "vt 0 1\n"
               "f 1/1 2/2 3/3\n";
    }

    ManuMeshMeshHandle* mesh = manumesh_mesh_create(context);
    ASSERT_NE(mesh, nullptr);
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_load_mesh(context, objPath.string().c_str(), mesh, 1e-9));

    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_clear(context, mesh));
    size_t vertexCount = 1;
    size_t faceCount = 1;
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_get_counts(context, mesh, &vertexCount, &faceCount));
    EXPECT_EQ(0u, vertexCount);
    EXPECT_EQ(0u, faceCount);

    // 保存操作会检查完整的 Mesh 不变量：清空后的面数组不应再与陈旧的面级 UV
    // 数据错位，否则该调用会失败。
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_save_binary_stl(context, stlPath.string().c_str(), mesh));
    EXPECT_EQ(84u, manumesh::filesystem::file_size(stlPath));

    manumesh_mesh_destroy(mesh);
    manumesh::filesystem::remove(stlPath);
    manumesh::filesystem::remove(objPath);
}

TEST_F(CApiTest, ClassifiesUnsupportedLoadFormatWithoutDependingOnIoErrorText) {
    ManuMeshMeshHandle* mesh = manumesh_mesh_create(context);
    ASSERT_NE(mesh, nullptr);
    const manumesh::filesystem::path missingPath =
        manumesh::filesystem::temp_directory_path() / "manumesh_c_api_missing_load.STL";
    manumesh::filesystem::remove(missingPath);

    EXPECT_EQ(MANUMESH_STATUS_UNSUPPORTED_FORMAT, manumesh_load_mesh(context, "mesh.ply", mesh, 1e-9));
    EXPECT_NE(std::string::npos, std::string(manumesh_context_last_error(context)).find("Unsupported"));
    EXPECT_EQ(MANUMESH_STATUS_IO_ERROR, manumesh_load_mesh(context, missingPath.string().c_str(), mesh, 1e-9));

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
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_set_data(context, mesh, vertices, 3, faces, 1));

    const ManuMeshVec3 invalidVertices[] = {
        {0.0, 0.0, 0.0},
        {std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0},
        {0.0, 1.0, 0.0},
    };
    EXPECT_EQ(MANUMESH_STATUS_INVALID_ARGUMENT, manumesh_mesh_set_data(context, mesh, invalidVertices, 3, faces, 1));
    EXPECT_NE('\0', manumesh_context_last_error(context)[0]);

    size_t vertexCount = 0;
    size_t faceCount = 0;
    EXPECT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_get_counts(context, mesh, &vertexCount, &faceCount));
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
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_set_data(context, mesh, vertices, 3, validFaces, 1));

    const ManuMeshFace invalidFaces[] = {{{0, 1, 5}}};
    EXPECT_EQ(MANUMESH_STATUS_INVALID_ARGUMENT, manumesh_mesh_set_data(context, mesh, vertices, 3, invalidFaces, 1));
    EXPECT_NE('\0', manumesh_context_last_error(context)[0]);

    size_t vertexCount = 0;
    size_t faceCount = 0;
    EXPECT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_get_counts(context, mesh, &vertexCount, &faceCount));
    EXPECT_EQ(3u, vertexCount);
    EXPECT_EQ(1u, faceCount);

    manumesh_mesh_destroy(mesh);
}

TEST_F(CApiTest, RejectsFaceCountOutsideSupportedIndexRange) {
    ManuMeshMeshHandle* mesh = manumesh_mesh_create(context);
    ASSERT_NE(mesh, nullptr);

    const ManuMeshVec3 vertices[] = {
        {0.0, 0.0, 0.0},
        {1.0, 0.0, 0.0},
        {0.0, 1.0, 0.0},
    };
    const ManuMeshFace face = {{0, 1, 2}};
    const size_t tooManyFaces = static_cast<size_t>(std::numeric_limits<int>::max()) + 1u;

    EXPECT_EQ(
        MANUMESH_STATUS_INVALID_ARGUMENT, manumesh_mesh_set_data(context, mesh, vertices, 3, &face, tooManyFaces)
    );
    EXPECT_NE('\0', manumesh_context_last_error(context)[0]);

    size_t vertexCount = 99;
    size_t faceCount = 99;
    EXPECT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_get_counts(context, mesh, &vertexCount, &faceCount));
    EXPECT_EQ(0u, vertexCount);
    EXPECT_EQ(0u, faceCount);

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
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_set_data(context, mesh, vertices, 3, validFaces, 1));

    const ManuMeshFace repeatedVertexFaces[] = {{{0, 1, 1}}};
    EXPECT_EQ(
        MANUMESH_STATUS_INVALID_ARGUMENT, manumesh_mesh_set_data(context, mesh, vertices, 3, repeatedVertexFaces, 1)
    );
    EXPECT_NE('\0', manumesh_context_last_error(context)[0]);

    // 宽松校验变更后，ABI 边界允许零面积（共线）面；算法会将其作为计数中的
    // 退化面继续传递，而不是直接失败。
    const ManuMeshVec3 collinearVertices[] = {
        {0.0, 0.0, 0.0},
        {1.0, 0.0, 0.0},
        {2.0, 0.0, 0.0},
    };
    EXPECT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_set_data(context, mesh, collinearVertices, 3, validFaces, 1));

    size_t vertexCount = 0;
    size_t faceCount = 0;
    EXPECT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_get_counts(context, mesh, &vertexCount, &faceCount));
    EXPECT_EQ(3u, vertexCount);
    EXPECT_EQ(1u, faceCount);

    manumesh_mesh_destroy(mesh);
}

TEST_F(CApiTest, SavesBinaryStlWithStandardLayoutAndLoadsItBack) {
    ManuMeshMeshHandle* mesh = manumesh_mesh_create(context);
    ASSERT_NE(mesh, nullptr);

    const ManuMeshVec3 vertices[] = {
        {0.0, 0.0, 0.0},
        {1.0, 0.0, 0.0},
        {0.0, 1.0, 0.0},
    };
    const ManuMeshFace faces[] = {{{0, 1, 2}}};
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_set_data(context, mesh, vertices, 3, faces, 1));

    const manumesh::filesystem::path path =
        manumesh::filesystem::temp_directory_path() / "manumesh_c_api_binary_export.stl";
    manumesh::filesystem::remove(path);
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_save_binary_stl(context, path.string().c_str(), mesh));
    ASSERT_EQ(134u, manumesh::filesystem::file_size(path));

    std::ifstream in(path, std::ios::binary);
    ASSERT_TRUE(in);
    in.seekg(80, std::ios::beg);
    unsigned char countBytes[4] = {};
    in.read(reinterpret_cast<char*>(countBytes), sizeof(countBytes));
    ASSERT_EQ(static_cast<std::streamsize>(sizeof(countBytes)), in.gcount());
    const std::uint32_t triangleCount =
        static_cast<std::uint32_t>(countBytes[0]) | (static_cast<std::uint32_t>(countBytes[1]) << 8u) |
        (static_cast<std::uint32_t>(countBytes[2]) << 16u) | (static_cast<std::uint32_t>(countBytes[3]) << 24u);
    EXPECT_EQ(1u, triangleCount);
    in.close();

    ManuMeshMeshHandle* loaded = manumesh_mesh_create(context);
    ASSERT_NE(loaded, nullptr);
    EXPECT_EQ(MANUMESH_STATUS_OK, manumesh_load_mesh(context, path.string().c_str(), loaded, 1e-9));

    size_t vertexCount = 0;
    size_t faceCount = 0;
    EXPECT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_get_counts(context, loaded, &vertexCount, &faceCount));
    EXPECT_EQ(3u, vertexCount);
    EXPECT_EQ(1u, faceCount);

    manumesh::filesystem::remove(path);
    manumesh_mesh_destroy(loaded);
    manumesh_mesh_destroy(mesh);
}

TEST_F(CApiTest, ClassifiesBinaryStlNumericRepresentabilityFailuresAsInvalidMesh) {
    ManuMeshMeshHandle* mesh = manumesh_mesh_create(context);
    ASSERT_NE(mesh, nullptr);
    const ManuMeshFace face[] = {{{0, 1, 2}}};
    const manumesh::filesystem::path path =
        manumesh::filesystem::temp_directory_path() / "manumesh_c_api_unrepresentable_binary.stl";
    manumesh::filesystem::remove(path);

    const ManuMeshVec3 outsideFloatRange[] = {
        {0.0, 0.0, 0.0},
        {1e40, 0.0, 0.0},
        {0.0, 1e40, 0.0},
    };
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_set_data(context, mesh, outsideFloatRange, 3, face, 1));
    EXPECT_EQ(MANUMESH_STATUS_INVALID_MESH, manumesh_save_binary_stl(context, path.string().c_str(), mesh));
    EXPECT_NE(std::string::npos, std::string(manumesh_context_last_error(context)).find("float32 range"));
    EXPECT_FALSE(manumesh::filesystem::exists(path));

    const ManuMeshVec3 collapsedByFloat32[] = {
        {100000000.0, 0.0, 0.0},
        {100000001.0, 0.0, 0.0},
        {100000000.0, 1.0, 0.0},
    };
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_set_data(context, mesh, collapsedByFloat32, 3, face, 1));
    EXPECT_EQ(MANUMESH_STATUS_INVALID_MESH, manumesh_save_binary_stl(context, path.string().c_str(), mesh));
    EXPECT_NE(std::string::npos, std::string(manumesh_context_last_error(context)).find("becomes degenerate"));
    EXPECT_FALSE(manumesh::filesystem::exists(path));

    manumesh_mesh_destroy(mesh);
}

TEST_F(CApiTest, BatchTextureInputTopologyVolumeAndObjExportAreTransactional) {
    ManuMeshMeshHandle* mesh = manumesh_mesh_create(context);
    ManuMeshMeshHandle* loaded = manumesh_mesh_create(context);
    ASSERT_NE(mesh, nullptr);
    ASSERT_NE(loaded, nullptr);
    const ManuMeshVec3 vertices[] = {
        {0.0, 0.0, 0.0},
        {1.0, 0.0, 0.0},
        {0.0, 1.0, 0.0},
        {0.0, 0.0, 1.0},
    };
    const ManuMeshFace faces[] = {
        {{0, 2, 1}},
        {{0, 1, 3}},
        {{0, 3, 2}},
        {{1, 2, 3}},
    };
    std::array<ManuMeshFaceTexCoords, 4> texcoords{};
    texcoords[0].valid = 1;
    texcoords[0].uv[0] = ManuMeshVec2{0.0, 0.0};
    texcoords[0].uv[1] = ManuMeshVec2{0.0, 1.0};
    texcoords[0].uv[2] = ManuMeshVec2{1.0, 0.0};
    ASSERT_EQ(
        MANUMESH_STATUS_OK,
        manumesh_mesh_set_data_with_texcoords(context, mesh, vertices, 4, faces, 4, texcoords.data())
    );

    ManuMeshTopologySummary summary{};
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_get_topology_summary(context, mesh, &summary));
    EXPECT_EQ(1u, summary.connected_face_components);
    EXPECT_EQ(6u, summary.unique_edges);
    EXPECT_EQ(0u, summary.boundary_edges);
    EXPECT_EQ(0u, summary.non_manifold_edges);
    EXPECT_EQ(1, summary.closed_manifold);
    EXPECT_EQ(1, summary.consistently_oriented);

    double volume = 0.0;
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_get_signed_volume(context, mesh, &volume));
    EXPECT_NEAR(1.0 / 6.0, volume, 1e-15);

    const manumesh::filesystem::path objPath =
        manumesh::filesystem::temp_directory_path() / "manumesh_c_api_uv_roundtrip.obj";
    manumesh::filesystem::remove(objPath);
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_save_obj(context, objPath.string().c_str(), mesh));
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_load_mesh(context, objPath.string().c_str(), loaded, 1e-9));
    ManuMeshFaceTexCoords loadedTexcoords{};
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_get_face_texcoords(context, loaded, 0, &loadedTexcoords));
    EXPECT_EQ(1, loadedTexcoords.valid);
    EXPECT_DOUBLE_EQ(1.0, loadedTexcoords.uv[2].u);

    std::array<ManuMeshFaceTexCoords, 4> invalidTexcoords = texcoords;
    invalidTexcoords[1].valid = 1;
    invalidTexcoords[1].uv[0].u = std::numeric_limits<double>::quiet_NaN();
    EXPECT_EQ(
        MANUMESH_STATUS_INVALID_ARGUMENT,
        manumesh_mesh_set_data_with_texcoords(context, mesh, vertices, 4, faces, 4, invalidTexcoords.data())
    );
    size_t vertexCount = 0;
    size_t faceCount = 0;
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_get_counts(context, mesh, &vertexCount, &faceCount));
    EXPECT_EQ(4u, vertexCount);
    EXPECT_EQ(4u, faceCount);

    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_reverse_winding(context, mesh));
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_get_signed_volume(context, mesh, &volume));
    EXPECT_NEAR(-1.0 / 6.0, volume, 1e-15);

    const ManuMeshFace openFace[] = {{{0, 1, 2}}};
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_set_data(context, mesh, vertices, 4, openFace, 1));
    volume = 99.0;
    EXPECT_EQ(MANUMESH_STATUS_INVALID_MESH, manumesh_mesh_get_signed_volume(context, mesh, &volume));
    EXPECT_DOUBLE_EQ(99.0, volume);
    ASSERT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_get_topology_summary(context, mesh, &summary));
    EXPECT_EQ(0, summary.closed_manifold);
    EXPECT_EQ(1u, summary.connected_face_components);

    manumesh::filesystem::remove(objPath);
    manumesh_mesh_destroy(loaded);
    manumesh_mesh_destroy(mesh);
}
