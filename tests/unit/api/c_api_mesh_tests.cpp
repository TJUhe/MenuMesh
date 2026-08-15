/**
 * @file tests/unit/api/c_api_mesh_tests.cpp
 * @brief 验证 ManuMesh 测试中的 C API 网格测试行为。
 * @ingroup manumesh_tests
 *
 * @details 测试夹具和断言记录可观察契约、数值容差、确定性要求以及已修复的回归问题。
 */

#include "CApiTestSupport.h"

#include "core/Filesystem.h"
#include <cstddef>
#include <cstdint>
#include <fstream>
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
