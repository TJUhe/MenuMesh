#include "algorithms/simplification/QEMSimplifier.h"
#include "core/MeshGenerators.h"

#include <cmath>
#include <gtest/gtest.h>
#include <limits>
#include <stdexcept>

// The generic core/topology/io cases that used to live here moved to
// tests/unit/core/core_tests.cpp and tests/unit/io/mesh_io_tests.cpp; this
// file keeps only tests that exercise the simplification entry points.

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

    manumesh::Mesh nonFinite = input;
    nonFinite.vertices[0].x() = std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(
        manumesh::simplification::simplifyMesh(nonFinite, manumesh::simplification::SimplifyOptions{}),
        std::invalid_argument
    );
}

namespace {

// Plane grid with one extra zero-area triangle. With a negative
// `collinearScale` the apex exactly duplicates the position of grid vertex 0
// (distinct index, coincident coordinates), so face {0, apex, 1} spans no
// area. A non-negative scale instead places the apex on the vertex0->vertex1
// segment offset orthogonally by `collinearScale`, producing a sliver whose
// area is ~1e-32 for a 1e-30 scale.
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

    // Before the lenient-validation change this threw std::invalid_argument
    // ("has zero area") from validateSimplifierInput.
    manumesh::simplification::SimplifyReport dirtyReport;
    manumesh::Mesh dirtyResult;
    ASSERT_NO_THROW(dirtyResult = manumesh::simplification::simplifyMesh(dirty, options, &dirtyReport));
    EXPECT_EQ(1, dirtyReport.degenerateInputFaces);
    EXPECT_FALSE(dirtyResult.empty());
    expectFiniteMesh(dirtyResult);
    EXPECT_TRUE(manumesh::validateMeshGeometryLenient(dirtyResult));

    // The rest of the plane still simplifies to a face count comparable to
    // the clean baseline (same budget, one extra input face).
    manumesh::simplification::SimplifyReport cleanReport;
    const manumesh::Mesh cleanResult = manumesh::simplification::simplifyMesh(clean, options, &cleanReport);
    EXPECT_EQ(0, cleanReport.degenerateInputFaces);
    EXPECT_GT(dirtyReport.collapsedEdges, 0);
    EXPECT_LE(
        std::abs(static_cast<int>(dirtyResult.faces.size()) - static_cast<int>(cleanResult.faces.size())),
        static_cast<int>(clean.faces.size()) / 4
    );
}

} // namespace

TEST(ManuMesh, SimplifierToleratesZeroAreaTriangleFromDuplicateVertex) {
    expectDegenerateInputSimplifiesSafely(-1.0);
}

TEST(ManuMesh, SimplifierToleratesCollinearSliverTriangle) {
    expectDegenerateInputSimplifiesSafely(1e-30);
}
