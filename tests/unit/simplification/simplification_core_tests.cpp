/**
 * @file tests/unit/simplification/simplification_core_tests.cpp
 * @brief 验证 ManuMesh 测试中的简化 核心测试行为。
 * @ingroup manumesh_tests
 *
 * @details 测试夹具和断言记录可观察契约、数值容差、确定性要求以及已修复的回归问题。
 */

#include "algorithms/simplification/QEMSimplifier.h"
#include "core/MeshGenerators.h"

#include <cmath>
#include <gtest/gtest.h>
#include <limits>
#include <stdexcept>

// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。

TEST(ManuMesh, SimplifierRejectsInvalidOptionsAndMeshes) {
    const manumesh::Mesh input = manumesh::generatePlaneGrid(4, 1.0, false);

    manumesh::simplification::SimplifyOptions options;
    options.targetRatio = 0.0;
    EXPECT_THROW(manumesh::simplification::simplifyMesh(input, options), std::invalid_argument);

    manumesh::Mesh invalid;
    invalid.vertices = {
        manumesh::Vec3(0.0, 0.0, 0.0),
        manumesh::Vec3(1.0, 0.0, 0.0),
        manumesh::Vec3(0.0, 1.0, 0.0),
    };
    invalid.faces = {{{0, 1, 5}}};
    EXPECT_THROW(
        manumesh::simplification::simplifyMesh(invalid, manumesh::simplification::SimplifyOptions{}),
        std::invalid_argument
    );

    manumesh::Mesh facesWithoutVertices;
    facesWithoutVertices.faces = {{{0, 1, 2}}};
    EXPECT_THROW(
        manumesh::simplification::simplifyMesh(facesWithoutVertices, manumesh::simplification::SimplifyOptions{}),
        std::invalid_argument
    );

    manumesh::Mesh nonFinite = input;
    nonFinite.vertices[0].x() = std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(
        manumesh::simplification::simplifyMesh(nonFinite, manumesh::simplification::SimplifyOptions{}),
        std::invalid_argument
    );
}

namespace {

// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
// 检查该步骤的边界条件，并确保结果保持确定性。
manumesh::Mesh makePlaneGridWithDegenerateTriangle(double collinearScale) {
    manumesh::Mesh mesh = manumesh::generatePlaneGrid(6, 1.0, false);
    const manumesh::Vec3 base = mesh.vertices[0];
    const manumesh::Vec3 along = mesh.vertices[1] - mesh.vertices[0];
    const manumesh::Vec3 ortho = manumesh::Vec3(-along.y(), along.x(), 0.0);
    const int apex = static_cast<int>(mesh.vertices.size());
    if (collinearScale < 0.0) {
        mesh.vertices.push_back(base);
    } else {
        mesh.vertices.push_back(base + 0.5 * along + collinearScale * ortho);
    }
    mesh.faces.push_back({{0, apex, 1}});
    return mesh;
}

void expectFiniteMesh(const manumesh::Mesh& mesh) {
    for (const manumesh::Vec3& p : mesh.vertices) {
        EXPECT_TRUE(p.allFinite());
    }
}

void expectDegenerateInputSimplifiesSafely(double collinearScale) {
    const manumesh::Mesh dirty = makePlaneGridWithDegenerateTriangle(collinearScale);
    const manumesh::Mesh clean = manumesh::generatePlaneGrid(6, 1.0, false);

    manumesh::simplification::SimplifyOptions options;
    options.targetRatio = 0.5;
    options.preserveBoundary = true;

    // 检查该步骤的边界条件，并确保结果保持确定性。
    // 检查该步骤的边界条件，并确保结果保持确定性。
    manumesh::simplification::SimplifyReport dirtyReport;
    manumesh::Mesh dirtyResult;
    ASSERT_NO_THROW(dirtyResult = manumesh::simplification::simplifyMesh(dirty, options, &dirtyReport));
    EXPECT_EQ(1, dirtyReport.degenerateInputFaces);
    EXPECT_FALSE(dirtyResult.empty());
    expectFiniteMesh(dirtyResult);
    EXPECT_TRUE(manumesh::validateMeshGeometryLenient(dirtyResult));

    // 检查该步骤的边界条件，并确保结果保持确定性。
    // 检查该步骤的边界条件，并确保结果保持确定性。
    manumesh::simplification::SimplifyReport cleanReport;
    const manumesh::Mesh cleanResult = manumesh::simplification::simplifyMesh(clean, options, &cleanReport);
    EXPECT_EQ(0, cleanReport.degenerateInputFaces);
    EXPECT_GT(dirtyReport.collapsedEdges, 0);
    EXPECT_LE(
        std::abs(static_cast<int>(dirtyResult.faces.size()) - static_cast<int>(cleanResult.faces.size())),
        static_cast<int>(clean.faces.size()) / 4
    );
}

} // 命名空间

TEST(ManuMesh, SimplifierToleratesZeroAreaTriangleFromDuplicateVertex) { expectDegenerateInputSimplifiesSafely(-1.0); }

TEST(ManuMesh, SimplifierToleratesCollinearSliverTriangle) { expectDegenerateInputSimplifiesSafely(1e-30); }
