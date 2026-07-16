/**
 * @file tests/unit/api/c_api_mesh_tests.cpp
 * @brief Verifies c api mesh tests behavior in the ManuMesh tests.
 * @ingroup manumesh_tests
 *
 * @details The fixture and assertions document observable contracts, numeric tolerances, determinism requirements, and previously fixed regressions.
 */

#include "CApiTestSupport.h"

#include <cstddef>
#include <gtest/gtest.h>
#include <limits>
#include <string>
#include <vector>

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

    std::vector<ManuMeshVec3> copiedVertices(required);
    EXPECT_EQ(
        MANUMESH_STATUS_OK,
        manumesh_mesh_copy_vertices(context, mesh, copiedVertices.data(), copiedVertices.size(), &required)
    );
    EXPECT_EQ(1.0, copiedVertices[1].x);

    size_t copiedFaces = 0;
    ManuMeshFace copiedFace;
    EXPECT_EQ(MANUMESH_STATUS_OK, manumesh_mesh_copy_faces(context, mesh, &copiedFace, 1, &copiedFaces));
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

    // Zero-area (collinear) faces are tolerated at the ABI boundary since the
    // lenient validation change: algorithms carry them as counted degenerate
    // faces instead of failing hard.
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
