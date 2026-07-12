#include "algorithms/simplification/QEMSimplifier.h"
#include "core/MeshGenerators.h"
#include "io/MeshIo.h"
#include "simplification/detail/TextureProtection.h"

#include <gtest/gtest.h>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <vector>

namespace {

using manumesh::FaceTexCoords;
using manumesh::Mat4;
using manumesh::Mesh;
using manumesh::Vec2;
using manumesh::simplification::DynamicTopology;
using manumesh::simplification::FaceState;
using manumesh::simplification::SimplifyOptions;
using manumesh::simplification::TextureCollapseRejectReason;
using manumesh::simplification::TextureProtection;
using manumesh::simplification::VertexState;

struct TextureFixture {
    Mesh mesh;
    std::vector<VertexState> vertices;
    std::vector<FaceState> faces;
    DynamicTopology topology;

    explicit TextureFixture(Mesh input)
        : mesh(std::move(input)),
          vertices(mesh.vertices.size()),
          faces(mesh.faces.size()),
          topology(makeFaces(), static_cast<int>(mesh.vertices.size())) {
        for (int vertex = 0; vertex < static_cast<int>(mesh.vertices.size()); ++vertex) {
            vertices[vertex].p = mesh.vertices[vertex];
        }
    }

private:
    std::vector<FaceState> makeFaces() {
        for (int face = 0; face < static_cast<int>(mesh.faces.size()); ++face) {
            faces[face].v = mesh.faces[face].v;
        }
        return faces;
    }
};

Mesh texturedGrid() {
    Mesh mesh = manumesh::generatePlaneGrid(2, 1.0, false);
    mesh.faceTexCoords.resize(mesh.faces.size());
    for (int face = 0; face < static_cast<int>(mesh.faces.size()); ++face) {
        mesh.faceTexCoords[face].valid = true;
        for (int corner = 0; corner < 3; ++corner) {
            const manumesh::Vec3& point = mesh.vertices[mesh.faces[face].v[corner]];
            mesh.faceTexCoords[face].uv[corner] = Vec2(point.x(), point.y());
        }
    }
    return mesh;
}

void offsetFaceUv(FaceTexCoords& texcoords, const Vec2& offset) {
    for (Vec2& uv : texcoords.uv) {
        uv += offset;
    }
}

} // namespace

TEST(TextureQem, GeometryQuadricRemainsFourByFour) {
    static_assert(Mat4::RowsAtCompileTime == 4);
    static_assert(Mat4::ColsAtCompileTime == 4);
    EXPECT_EQ(16, Mat4::SizeAtCompileTime);
    EXPECT_FALSE(SimplifyOptions{}.preserveTexture);
}

TEST(TextureQem, UsesScalarWeightWithoutChangingPlacementSpace) {
    TextureFixture fixture(texturedGrid());
    SimplifyOptions unweightedOptions;
    unweightedOptions.preserveTexture = true;
    unweightedOptions.textureWeight = 0.0;
    TextureProtection unweighted(fixture.mesh, unweightedOptions);
    SimplifyOptions weightedOptions = unweightedOptions;
    weightedOptions.textureWeight = 7.0;
    TextureProtection weighted(fixture.mesh, weightedOptions);

    const manumesh::Vec3 midpoint = 0.5 * (fixture.vertices[1].p + fixture.vertices[4].p);
    const auto zero = unweighted.evaluate(
        {1, 4}, midpoint, fixture.faces, fixture.vertices, fixture.topology, fixture.mesh.faceTexCoords
    );
    const auto scaled = weighted.evaluate(
        {1, 4}, midpoint, fixture.faces, fixture.vertices, fixture.topology, fixture.mesh.faceTexCoords
    );

    ASSERT_TRUE(zero.allowed());
    ASSERT_TRUE(scaled.allowed());
    EXPECT_DOUBLE_EQ(0.0, zero.cost);
    EXPECT_GT(scaled.cost, 0.0);
}

TEST(TextureQem, ScalarCostIsInvariantToUniformUvScale) {
    TextureFixture fixture(texturedGrid());
    Mesh scaledMesh = fixture.mesh;
    for (FaceTexCoords& texcoords : scaledMesh.faceTexCoords) {
        for (Vec2& uv : texcoords.uv) {
            uv *= 1000.0;
        }
    }
    SimplifyOptions options;
    options.preserveTexture = true;
    TextureProtection original(fixture.mesh, options);
    TextureProtection scaled(scaledMesh, options);
    const manumesh::Vec3 midpoint = 0.5 * (fixture.vertices[1].p + fixture.vertices[4].p);

    const auto originalEvaluation = original.evaluate(
        {1, 4}, midpoint, fixture.faces, fixture.vertices, fixture.topology, fixture.mesh.faceTexCoords
    );
    const auto scaledEvaluation =
        scaled.evaluate({1, 4}, midpoint, fixture.faces, fixture.vertices, fixture.topology, scaledMesh.faceTexCoords);

    ASSERT_TRUE(originalEvaluation.allowed());
    ASSERT_TRUE(scaledEvaluation.allowed());
    EXPECT_NEAR(originalEvaluation.cost, scaledEvaluation.cost, std::abs(originalEvaluation.cost) * 1e-12);
}

TEST(TextureQem, RejectsCollapseThatCannotPairAllUvCharts) {
    TextureFixture fixture(texturedGrid());
    offsetFaceUv(fixture.mesh.faceTexCoords[1], Vec2(10.0, 0.0));
    SimplifyOptions options;
    options.preserveTexture = true;
    TextureProtection protection(fixture.mesh, options);

    const manumesh::Vec3 midpoint = 0.5 * (fixture.vertices[0].p + fixture.vertices[1].p);
    const auto evaluation = protection.evaluate(
        {0, 1}, midpoint, fixture.faces, fixture.vertices, fixture.topology, fixture.mesh.faceTexCoords
    );

    EXPECT_EQ(TextureCollapseRejectReason::ChartMismatch, evaluation.rejectReason);
}

TEST(TextureQem, AllowsCompatibleCollapseAlongTwoSidedUvSeam) {
    TextureFixture fixture(texturedGrid());
    offsetFaceUv(fixture.mesh.faceTexCoords[2], Vec2(10.0, 0.0));
    offsetFaceUv(fixture.mesh.faceTexCoords[3], Vec2(10.0, 0.0));
    offsetFaceUv(fixture.mesh.faceTexCoords[6], Vec2(10.0, 0.0));
    offsetFaceUv(fixture.mesh.faceTexCoords[7], Vec2(10.0, 0.0));
    SimplifyOptions options;
    options.preserveTexture = true;
    TextureProtection protection(fixture.mesh, options);

    const manumesh::Vec3 midpoint = 0.5 * (fixture.vertices[1].p + fixture.vertices[4].p);
    const auto evaluation = protection.evaluate(
        {1, 4}, midpoint, fixture.faces, fixture.vertices, fixture.topology, fixture.mesh.faceTexCoords
    );

    EXPECT_TRUE(evaluation.allowed());
    EXPECT_TRUE(std::isfinite(evaluation.cost));
}

TEST(TextureQem, RejectsUvTriangleDegeneration) {
    TextureFixture fixture(texturedGrid());
    const int survivingFace = 1;
    const int removeCorner = [&] {
        for (int corner = 0; corner < 3; ++corner) {
            if (fixture.faces[survivingFace].v[corner] == 4) {
                return corner;
            }
        }
        return -1;
    }();
    ASSERT_GE(removeCorner, 0);
    const int nextCorner = (removeCorner + 1) % 3;
    fixture.mesh.faceTexCoords[survivingFace].uv[nextCorner] = fixture.mesh.faceTexCoords[0].uv[1];
    SimplifyOptions options;
    options.preserveTexture = true;
    TextureProtection protection(fixture.mesh, options);

    const auto evaluation = protection.evaluate(
        {1, 4}, fixture.vertices[1].p, fixture.faces, fixture.vertices, fixture.topology, fixture.mesh.faceTexCoords
    );

    EXPECT_EQ(TextureCollapseRejectReason::TriangleFlip, evaluation.rejectReason);
}

TEST(TextureQem, ObjLoaderPreservesPerCornerTextureSeams) {
    const std::filesystem::path path = std::filesystem::temp_directory_path() / "manumesh_texture_seam_test.obj";
    {
        std::ofstream out(path);
        out << "v 0 0 0\n"
            << "v 1 0 0\n"
            << "v 0 1 0\n"
            << "v 1 1 0\n"
            << "vt 0 0\n"
            << "vt 1 0\n"
            << "vt 0 1\n"
            << "vt 10 0\n"
            << "vt 11 1\n"
            << "vt 10 1\n"
            << "f 1/1 2/2 3/3\n"
            << "f 2/4 4/5 3/6\n";
    }

    Mesh mesh;
    std::string error;
    ASSERT_TRUE(manumesh::loadObj(path.string(), mesh, &error)) << error;
    std::filesystem::remove(path);
    ASSERT_EQ(2u, mesh.faceTexCoords.size());
    ASSERT_TRUE(mesh.faceTexCoords[0].valid);
    ASSERT_TRUE(mesh.faceTexCoords[1].valid);
    EXPECT_NE(mesh.faceTexCoords[0].uv[1], mesh.faceTexCoords[1].uv[0]);
}

TEST(TextureQem, SimplifierReturnsFaceAlignedTextureCoordinates) {
    Mesh mesh = texturedGrid();
    offsetFaceUv(mesh.faceTexCoords[2], Vec2(10.0, 0.0));
    offsetFaceUv(mesh.faceTexCoords[3], Vec2(10.0, 0.0));
    offsetFaceUv(mesh.faceTexCoords[6], Vec2(10.0, 0.0));
    offsetFaceUv(mesh.faceTexCoords[7], Vec2(10.0, 0.0));
    SimplifyOptions options;
    options.targetRatio = 0.5;
    options.useLineQuadrics = false;
    options.preserveTexture = true;
    manumesh::simplification::SimplifyReport report;

    const Mesh output = manumesh::simplification::simplifyMesh(mesh, options, &report);

    EXPECT_EQ(output.faces.size(), output.faceTexCoords.size());
    EXPECT_TRUE(output.hasTextureCoordinates());
    EXPECT_GT(report.textureProtectedEdges, 0);
    EXPECT_TRUE(manumesh::validateMeshGeometry(output));
}

TEST(TextureQem, DefaultDisabledProtectionPreservesLegacyGeometryResult) {
    const Mesh untextured = manumesh::generatePlaneGrid(5, 1.0, false);
    Mesh textured = untextured;
    textured.faceTexCoords.resize(textured.faces.size());
    for (int face = 0; face < static_cast<int>(textured.faces.size()); ++face) {
        textured.faceTexCoords[face].valid = true;
        for (int corner = 0; corner < 3; ++corner) {
            const manumesh::Vec3& point = textured.vertices[textured.faces[face].v[corner]];
            textured.faceTexCoords[face].uv[corner] = Vec2(point.x(), point.y());
        }
    }
    SimplifyOptions options;
    options.targetRatio = 0.4;

    const Mesh baseline = manumesh::simplification::simplifyMesh(untextured, options);
    const Mesh disabled = manumesh::simplification::simplifyMesh(textured, options);

    ASSERT_EQ(baseline.vertices.size(), disabled.vertices.size());
    ASSERT_EQ(baseline.faces.size(), disabled.faces.size());
    for (int vertex = 0; vertex < static_cast<int>(baseline.vertices.size()); ++vertex) {
        EXPECT_EQ(baseline.vertices[vertex], disabled.vertices[vertex]);
    }
    for (int face = 0; face < static_cast<int>(baseline.faces.size()); ++face) {
        EXPECT_EQ(baseline.faces[face].v, disabled.faces[face].v);
    }
}
