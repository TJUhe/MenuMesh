/**
 * @file tests/unit/common/mesh_distance_index_tests.cpp
 * @brief Verifies mesh distance index tests behavior in the ManuMesh tests.
 * @ingroup manumesh_tests
 *
 * @details The fixture and assertions document observable contracts, numeric tolerances, determinism requirements, and previously fixed regressions.
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

} // namespace

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
