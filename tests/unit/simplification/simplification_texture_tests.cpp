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

TEST(TextureQem, SeamToleranceFlipsChartGroupingAroundPerturbationScale) {
    // textureSeamTolerance is relative to the UV bounding-box diagonal
    // (TextureProtection ctor: uvTolerance = tol * uvDiagonal). texturedGrid()
    // maps UV = XY of generatePlaneGrid(2, 1.0), so the UV diagonal is
    // sqrt(2) and a corner perturbation of delta = 1e-3 sits at relative
    // scale delta / sqrt(2) ~= 7.07e-4. Tolerances at 2e-4 (~0.28x) and 2e-3
    // (~2.8x) bracket it on both sides, so the same input must flip between
    // seam (ChartMismatch reject) and same-chart (allowed) handling.
    constexpr double kUvPerturbation = 1e-3;
    const auto evaluateWithTolerance = [&](double tolerance) {
        TextureFixture fixture(texturedGrid());
        // Perturb only face 2's corner UV at interior edge endpoint vertex 1;
        // faces 0 and 1 keep the exact shared UV, so vertex 1 has two UV
        // clusters iff the perturbation exceeds the tolerance.
        for (int corner = 0; corner < 3; ++corner) {
            if (fixture.faces[2].v[corner] == 1) {
                fixture.mesh.faceTexCoords[2].uv[corner] += Vec2(kUvPerturbation, 0.0);
            }
        }
        SimplifyOptions options;
        options.preserveTexture = true;
        options.textureSeamTolerance = tolerance;
        TextureProtection protection(fixture.mesh, options);
        const manumesh::Vec3 midpoint = 0.5 * (fixture.vertices[1].p + fixture.vertices[4].p);
        return protection.evaluate(
            {1, 4}, midpoint, fixture.faces, fixture.vertices, fixture.topology, fixture.mesh.faceTexCoords
        );
    };

    const auto sameChart = evaluateWithTolerance(2e-3);
    EXPECT_TRUE(sameChart.allowed());
    EXPECT_GT(sameChart.cost, 0.0);

    // Below the perturbation the corner splits into its own chart. Vertex 4
    // (the remove endpoint) has a single chart, so the one-sided seam cannot
    // pair its charts across the collapse and must be rejected.
    const auto seam = evaluateWithTolerance(2e-4);
    EXPECT_EQ(TextureCollapseRejectReason::ChartMismatch, seam.rejectReason);
    EXPECT_FALSE(seam.allowed());
}

TEST(TextureQem, MinTextureAreaRatioFlipsAroundCompressedUvArea) {
    // Collapse (keep = 1, remove = 4) at the geometric midpoint, with vertex
    // 1's UV moved off the geometric map to (-0.9, -0.5) on all of its faces
    // (0, 2, 3). The midpoint maps to the t = 0.5 UV blend, so the merged UV
    // is 0.5 * ((-0.9, -0.5) + (0, 0)) = (-0.45, -0.25). Surviving face 1
    // {0, 4, 3} has its fixed UV edge on the line u = -0.5, and its third
    // corner (vertex 4) moves from u = 0 to u = -0.45: the UV area scales by
    // the horizontal offset ratio 0.05 / 0.5 = 0.1 exactly (every other
    // surviving face keeps at least 0.6x, so face 1 alone decides the flip).
    // minTextureAreaRatio = 0.05 (below 0.1) must therefore accept and 0.2
    // (above 0.1, below 0.6) must reject with TriangleFlip.
    const auto evaluateWithRatio = [&](double minAreaRatio) {
        TextureFixture fixture(texturedGrid());
        for (int face : {0, 2, 3}) {
            for (int corner = 0; corner < 3; ++corner) {
                if (fixture.faces[face].v[corner] == 1) {
                    fixture.mesh.faceTexCoords[face].uv[corner] = Vec2(-0.9, -0.5);
                }
            }
        }
        SimplifyOptions options;
        options.preserveTexture = true;
        options.minTextureAreaRatio = minAreaRatio;
        TextureProtection protection(fixture.mesh, options);
        const manumesh::Vec3 midpoint = 0.5 * (fixture.vertices[1].p + fixture.vertices[4].p);
        return protection.evaluate(
            {1, 4}, midpoint, fixture.faces, fixture.vertices, fixture.topology, fixture.mesh.faceTexCoords
        );
    };

    const auto accepted = evaluateWithRatio(0.05);
    EXPECT_TRUE(accepted.allowed());
    EXPECT_GT(accepted.cost, 0.0);

    const auto rejected = evaluateWithRatio(0.2);
    EXPECT_EQ(TextureCollapseRejectReason::TriangleFlip, rejected.rejectReason);
    EXPECT_FALSE(rejected.allowed());
}

TEST(TextureQem, MinTextureAreaRatioRejectionsReachTextureRejectedCounter) {
    // End-to-end coverage that a strict minTextureAreaRatio really routes
    // rejected candidates through CollapseAttemptStatus::TextureRejected into
    // SimplifyReport::textureRejectedCollapses. The UV map is deliberately
    // non-affine (u = x + 0.8 x^2), so nearly every collapse changes local UV
    // triangle areas; with minTextureAreaRatio close to 1 those changes
    // exceed the budget. An affine map would never trigger the check because
    // interpolated merged UVs keep areas exact.
    const auto runWithRatio = [](double minAreaRatio) {
        Mesh mesh = manumesh::generatePlaneGrid(6, 1.0, false);
        mesh.faceTexCoords.resize(mesh.faces.size());
        for (int face = 0; face < static_cast<int>(mesh.faces.size()); ++face) {
            mesh.faceTexCoords[face].valid = true;
            for (int corner = 0; corner < 3; ++corner) {
                const manumesh::Vec3& point = mesh.vertices[mesh.faces[face].v[corner]];
                mesh.faceTexCoords[face].uv[corner] =
                    Vec2(point.x() + 0.8 * point.x() * point.x(), point.y() + 0.8 * point.y() * point.y());
            }
        }
        SimplifyOptions options;
        options.targetFaces = 2;
        options.preserveTexture = true;
        options.minTextureAreaRatio = minAreaRatio;
        manumesh::simplification::SimplifyReport report;
        const Mesh output = manumesh::simplification::simplifyMesh(mesh, options, &report);
        EXPECT_TRUE(manumesh::validateMeshGeometry(output));
        return report;
    };

    // Default-like tolerance: nothing hits the area check.
    const auto permissive = runWithRatio(1e-8);
    EXPECT_EQ(0, permissive.textureRejectedCollapses);

    // Strict tolerance: measured on this deterministic fixture the run
    // records 3 texture-rejected collapses (plus topology rejections once
    // the area budget freezes the remaining candidates). Assert the counter
    // becomes positive rather than pinning the exact count, which depends on
    // queue order.
    const auto strict = runWithRatio(0.95);
    EXPECT_GT(strict.textureRejectedCollapses, 0);
    EXPECT_GE(strict.rejectedCollapses, strict.textureRejectedCollapses);
    // The strict run must also end with more faces than the permissive one:
    // the area budget blocks collapses the permissive run performed.
    EXPECT_GE(strict.finalFaces, permissive.finalFaces);
}

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
