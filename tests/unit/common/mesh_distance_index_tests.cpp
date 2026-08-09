/**
 * @file tests/unit/common/mesh_distance_index_tests.cpp
 * @brief 验证 ManuMesh 测试中的网格距离索引测试行为。
 * @ingroup manumesh_tests
 *
 * @details 测试夹具和断言记录可观察契约、数值容差、确定性要求以及已修复的回归问题。
 */

#include "../../../src/common/detail/MeshDistanceIndex.h"

#include <cmath>
#include <gtest/gtest.h>

namespace {

manumesh::Mesh twoTriangleMesh() {
    manumesh::Mesh mesh;
    mesh.vertices = {
        manumesh::Vec3(0.0, 0.0, 0.0),
        manumesh::Vec3(1.0, 0.0, 0.0),
        manumesh::Vec3(0.0, 1.0, 0.0),
        manumesh::Vec3(0.0, 0.0, 5.0),
        manumesh::Vec3(1.0, 0.0, 5.0),
        manumesh::Vec3(0.0, 1.0, 5.0),
    };
    mesh.faces = {manumesh::Face{{0, 1, 2}}, manumesh::Face{{3, 4, 5}}};
    return mesh;
}

} // 命名空间

TEST(ManuMesh, MeshDistanceIndexFindsNearestTriangleDistance) {
    const manumesh::Mesh mesh = twoTriangleMesh();
    const manumesh::common::MeshDistanceIndex index(mesh);

    ASSERT_FALSE(index.empty());
    EXPECT_NEAR(4.0, index.distanceSquared(manumesh::Vec3(0.25, 0.25, 2.0)), 1e-12);
    EXPECT_NEAR(1.0, index.distanceSquared(manumesh::Vec3(0.25, 0.25, 4.0)), 1e-12);
}

TEST(ManuMesh, MeshDistanceIndexIgnoresDegenerateTriangles) {
    manumesh::Mesh mesh;
    mesh.vertices = {
        manumesh::Vec3(0.0, 0.0, 0.0),
        manumesh::Vec3(1.0, 0.0, 0.0),
        manumesh::Vec3(2.0, 0.0, 0.0),
    };
    mesh.faces = {manumesh::Face{{0, 1, 2}}};

    const manumesh::common::MeshDistanceIndex index(mesh);

    EXPECT_TRUE(index.empty());
    EXPECT_TRUE(std::isinf(index.distanceSquared(manumesh::Vec3(0.0, 0.0, 1.0))));
}
