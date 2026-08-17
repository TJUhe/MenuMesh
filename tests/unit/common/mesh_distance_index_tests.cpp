/**
 * @file tests/unit/common/mesh_distance_index_tests.cpp
 * @brief 验证网格距离索引的最近三角形查询和退化面处理。
 * @ingroup manumesh_tests
 */

#include "../../../src/common/detail/MeshDistanceIndex.h"

#include "algorithms/analysis/MeshAnalysis.h"
#include "core/MeshGenerators.h"

#include <cmath>
#include <gtest/gtest.h>
#include <limits>
#include <string>
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

TEST(ManuMesh, MeshDistanceIndexSkipsNonFiniteTrianglesAndQueries) {
    manumesh::Mesh mesh = twoTriangleMesh();
    mesh.vertices.push_back(manumesh::Vec3(std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0));
    mesh.faces.push_back(manumesh::Face{{0, 1, 6}});

    const manumesh::common::MeshDistanceIndex index(mesh);

    ASSERT_FALSE(index.empty());
    EXPECT_EQ(1, index.skippedFaceCount());
    EXPECT_TRUE(std::isinf(index.distanceSquared(manumesh::Vec3(std::numeric_limits<double>::infinity(), 0.0, 0.0))));
}

TEST(ManuMesh, BuiltInGeneratorsRejectInvalidNumericAndResourceParameters) {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();
    EXPECT_TRUE(manumesh::generatePlaneGrid(std::numeric_limits<int>::max(), 1.0, false).empty());
    EXPECT_TRUE(manumesh::generatePlaneGrid(8, nan, false).empty());
    EXPECT_TRUE(manumesh::generateRidgeGrid(8, 0.0, 1.0).empty());
    EXPECT_TRUE(manumesh::generateNoisyPlaneGrid(8, 1.0, -1.0).empty());
    EXPECT_TRUE(manumesh::generateCylinderGrid(16, 4, infinity, 1.0).empty());
    EXPECT_TRUE(manumesh::generateTorusGrid(16, std::numeric_limits<int>::max(), 2.0, 0.5).empty());
    EXPECT_TRUE(manumesh::generateClosedCubeGrid(std::numeric_limits<int>::max(), 1.0).empty());

    manumesh::Mesh output = twoTriangleMesh();
    const std::size_t originalVertices = output.vertices.size();
    const std::size_t originalFaces = output.faces.size();
    std::string error = "stale";
    EXPECT_FALSE(manumesh::generateMeshByName("plane", std::numeric_limits<int>::max(), output, &error));
    EXPECT_FALSE(error.empty());
    EXPECT_EQ(originalVertices, output.vertices.size());
    EXPECT_EQ(originalFaces, output.faces.size());

    ASSERT_TRUE(manumesh::generateMeshByName("plane", 8, output, &error)) << error;
    EXPECT_TRUE(error.empty());
}

TEST(ManuMesh, RidgeGeneratorKeepsPositiveSubnormalSizeFinite) {
    const manumesh::Mesh ridge = manumesh::generateRidgeGrid(4, std::numeric_limits<double>::denorm_min(), 1.0);
    ASSERT_FALSE(ridge.empty());
    for (const manumesh::Vec3& vertex : ridge.vertices) {
        EXPECT_TRUE(vertex.allFinite());
    }
    std::string error;
    EXPECT_TRUE(manumesh::validateMeshGeometryLenient(ridge, &error)) << error;
}

TEST(ManuMesh, SampledDistanceCoversSmallDisconnectedComponents) {
    manumesh::Mesh original;
    original.vertices = {
        manumesh::Vec3(0.0, 0.0, 0.0),
        manumesh::Vec3(10.0, 0.0, 0.0),
        manumesh::Vec3(0.0, 10.0, 0.0),
        manumesh::Vec3(0.0, 0.0, 10.0),
        manumesh::Vec3(0.01, 0.0, 10.0),
        manumesh::Vec3(0.0, 0.01, 10.0),
    };
    original.faces = {manumesh::Face{{0, 1, 2}}, manumesh::Face{{3, 4, 5}}};
    manumesh::Mesh simplified;
    simplified.vertices.assign(original.vertices.begin(), original.vertices.begin() + 3);
    simplified.faces = {manumesh::Face{{0, 1, 2}}};

    const manumesh::analysis::DistanceStats stats =
        manumesh::analysis::compareMeshesBySampledDistance(original, simplified, 2);

    EXPECT_GT(stats.maxOriginalToSimplified, 9.9);
    EXPECT_NEAR(0.0, stats.maxSimplifiedToOriginal, 1e-12);
}

TEST(ManuMesh, SampledDistanceClampsOversizedWorkBudgets) {
    const manumesh::Mesh mesh = manumesh::generatePlaneGrid(2, 1.0, false);
    manumesh::Mesh offset = mesh;
    for (manumesh::Vec3& vertex : offset.vertices) {
        vertex.z() += 1.0;
    }
    const manumesh::analysis::DistanceStats stats =
        manumesh::analysis::compareMeshesBySampledDistance(mesh, offset, std::numeric_limits<int>::max());
    EXPECT_NEAR(1.0, stats.meanOriginalToSimplified, 1e-12);
    EXPECT_NEAR(1.0, stats.maxOriginalToSimplified, 1e-12);
    EXPECT_NEAR(1.0, stats.meanSimplifiedToOriginal, 1e-12);
    EXPECT_NEAR(1.0, stats.maxSimplifiedToOriginal, 1e-12);
}
