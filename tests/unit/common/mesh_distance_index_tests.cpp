/**
 * @file tests/unit/common/mesh_distance_index_tests.cpp
 * @brief 验证网格距离索引的最近三角形查询和退化面处理。
 * @ingroup manumesh_tests
 */

#include "../../../src/common/detail/MeshDistanceIndex.h"

#include "core/MeshGenerators.h"

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

TEST(ManuMesh, MeshDistanceIndexBuildsLargeTreesWithoutInvalidatingParentNodes) {
    const manumesh::Mesh mesh = manumesh::generatePlaneGrid(64, 2.0, false);

    for (int build = 0; build < 8; ++build) {
        const manumesh::common::MeshDistanceIndex index(mesh);
        ASSERT_FALSE(index.empty());

        for (int y = -3; y <= 3; ++y) {
            for (int x = -3; x <= 3; ++x) {
                const double px = static_cast<double>(x) * 0.25;
                const double py = static_cast<double>(y) * 0.25;
                EXPECT_NEAR(0.25, index.distanceSquared(manumesh::Vec3(px, py, 0.5)), 1e-12);
                EXPECT_NEAR(4.0, index.distanceSquared(manumesh::Vec3(px, py, -2.0)), 1e-12);
            }
        }
    }
}
