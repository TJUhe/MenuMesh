#include "core/Mesh.h"
#include "io/MeshIo.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <limits>
#include <string>
#include <vector>

namespace {

std::filesystem::path writeTempFile(const std::string& name, const std::string& contents) {
    const std::filesystem::path path = std::filesystem::temp_directory_path() / name;
    std::ofstream out(path, std::ios::binary);
    out << contents;
    return path;
}

void appendBytes(std::string& out, const void* data, std::size_t size) {
    out.append(static_cast<const char*>(data), size);
}

void appendUint32LE(std::string& out, std::uint32_t value) { appendBytes(out, &value, sizeof(value)); }

void appendUint16LE(std::string& out, std::uint16_t value) { appendBytes(out, &value, sizeof(value)); }

void appendFloatLE(std::string& out, float value) { appendBytes(out, &value, sizeof(value)); }

/// Builds a binary STL blob with an all-zero header (so it does not start
/// with "solid"), the given declared triangle count, the actual triangle
/// records, and optional trailing padding bytes.
std::string makeBinaryStl(
    std::uint32_t declaredTriangles, const std::vector<std::array<float, 9>>& triangles, std::size_t trailingPadding
) {
    std::string data(80, '\0');
    appendUint32LE(data, declaredTriangles);
    for (const std::array<float, 9>& tri : triangles) {
        for (int k = 0; k < 3; ++k) {
            appendFloatLE(data, 0.0f); // normal
        }
        for (float coordinate : tri) {
            appendFloatLE(data, coordinate);
        }
        appendUint16LE(data, 0); // attribute byte count
    }
    data.append(trailingPadding, '\0');
    return data;
}

std::string singleAsciiStlTriangle() {
    return "solid tri\n"
           "  facet normal 0 0 1\n"
           "    outer loop\n"
           "      vertex 0 0 0\n"
           "      vertex 1 0 0\n"
           "      vertex 0 1 0\n"
           "    endloop\n"
           "  endfacet\n"
           "endsolid tri\n";
}

} // namespace

TEST(MeshIo, MalformedObjVertexLineIsAnErrorWithLineNumber) {
    const std::filesystem::path path = writeTempFile(
        "manumesh_io_bad_vertex.obj",
        "v 0 0 0\n"
        "v 1 0\n"
        "v 0 1 0\n"
        "f 1 2 3\n"
    );

    manumesh::Mesh mesh;
    std::string error;
    EXPECT_FALSE(manumesh::loadObj(path.string(), mesh, &error));
    EXPECT_NE(error.find("line 2"), std::string::npos) << error;
    std::filesystem::remove(path);
}

TEST(MeshIo, ObjAcceptsSingleComponentTextureCoordinate) {
    const std::filesystem::path path = writeTempFile(
        "manumesh_io_single_vt.obj",
        "v 0 0 0\n"
        "v 1 0 0\n"
        "v 0 1 0\n"
        "vt 0.25\n"
        "vt 0.5 0.75\n"
        "vt 1 0 0.5\n"
        "f 1/1 2/2 3/3\n"
    );

    manumesh::Mesh mesh;
    std::string error;
    ASSERT_TRUE(manumesh::loadObj(path.string(), mesh, &error)) << error;
    std::filesystem::remove(path);

    ASSERT_EQ(1u, mesh.faceTexCoords.size());
    ASSERT_TRUE(mesh.faceTexCoords[0].valid);
    EXPECT_DOUBLE_EQ(0.25, mesh.faceTexCoords[0].uv[0].x());
    EXPECT_DOUBLE_EQ(0.0, mesh.faceTexCoords[0].uv[0].y());
    EXPECT_DOUBLE_EQ(0.5, mesh.faceTexCoords[0].uv[1].x());
    EXPECT_DOUBLE_EQ(0.75, mesh.faceTexCoords[0].uv[1].y());
    EXPECT_DOUBLE_EQ(1.0, mesh.faceTexCoords[0].uv[2].x());
    EXPECT_DOUBLE_EQ(0.0, mesh.faceTexCoords[0].uv[2].y());
}

TEST(MeshIo, ObjResolvesNegativeIndices) {
    const std::filesystem::path path = writeTempFile(
        "manumesh_io_negative_indices.obj",
        "v 0 0 0\n"
        "v 1 0 0\n"
        "v 0 1 0\n"
        "f -3 -2 -1\n"
    );

    manumesh::Mesh mesh;
    std::string error;
    ASSERT_TRUE(manumesh::loadObj(path.string(), mesh, &error)) << error;
    std::filesystem::remove(path);

    ASSERT_EQ(1u, mesh.faces.size());
    EXPECT_EQ(0, mesh.faces[0].v[0]);
    EXPECT_EQ(1, mesh.faces[0].v[1]);
    EXPECT_EQ(2, mesh.faces[0].v[2]);
    EXPECT_TRUE(mesh.faceTexCoords.empty());
}

TEST(MeshIo, ObjParsesAllFaceCornerFormats) {
    const std::filesystem::path path = writeTempFile(
        "manumesh_io_corner_formats.obj",
        "# a comment line\n"
        "v 0 0 0\n"
        "v 1 0 0\n"
        "v 0 1 0\n"
        "v 1 1 0\n"
        "vt 0 0\n"
        "vt 1 0\n"
        "vt 0 1\n"
        "vt 1 1\n"
        "vn 0 0 1\n"
        "g quad\n"
        "usemtl none\n"
        "f 1 2 3\n"
        "f 1/1 2/2 4/4\n"
        "f 1//1 2//1 3//1\n"
        "f 2/2/1 4/4/1 3/3/1\n"
    );

    manumesh::Mesh mesh;
    std::string error;
    ASSERT_TRUE(manumesh::loadObj(path.string(), mesh, &error)) << error;
    std::filesystem::remove(path);

    ASSERT_EQ(4u, mesh.faces.size());
    ASSERT_EQ(4u, mesh.faceTexCoords.size());
    EXPECT_FALSE(mesh.faceTexCoords[0].valid);
    EXPECT_TRUE(mesh.faceTexCoords[1].valid);
    EXPECT_FALSE(mesh.faceTexCoords[2].valid);
    EXPECT_TRUE(mesh.faceTexCoords[3].valid);
    EXPECT_DOUBLE_EQ(1.0, mesh.faceTexCoords[1].uv[2].x());
    EXPECT_DOUBLE_EQ(1.0, mesh.faceTexCoords[1].uv[2].y());
}

TEST(MeshIo, ObjFanTriangulatesConvexPolygons) {
    const std::filesystem::path path = writeTempFile(
        "manumesh_io_polygon_fan.obj",
        "v 0 0 0\n"
        "v 2 0 0\n"
        "v 3 1 0\n"
        "v 1.5 2.5 0\n"
        "v 0 1 0\n"
        "f 1 2 3 4 5\n"
    );

    manumesh::Mesh mesh;
    std::string error;
    ASSERT_TRUE(manumesh::loadObj(path.string(), mesh, &error)) << error;
    std::filesystem::remove(path);

    ASSERT_EQ(3u, mesh.faces.size());
    EXPECT_EQ(5u, mesh.vertices.size());
    const std::array<std::array<int, 3>, 3> expected{{{0, 1, 2}, {0, 2, 3}, {0, 3, 4}}};
    for (std::size_t face = 0; face < expected.size(); ++face) {
        for (std::size_t corner = 0; corner < 3; ++corner) {
            EXPECT_EQ(expected[face][corner], mesh.faces[face].v[corner]) << face << "/" << corner;
        }
    }
}

TEST(MeshIo, ObjEarClipsConcavePolygonsWithoutOverlappingFanTriangles) {
    const std::filesystem::path path = writeTempFile(
        "manumesh_io_concave_polygon.obj",
        "v 0 0 0\n"
        "v 2 0 0\n"
        "v 2 2 0\n"
        "v 1 1 0\n"
        "v 0 2 0\n"
        "f 1 2 3 4 5\n"
    );

    manumesh::Mesh mesh;
    std::string error;
    ASSERT_TRUE(manumesh::loadObj(path.string(), mesh, &error)) << error;
    std::filesystem::remove(path);

    ASSERT_EQ(3u, mesh.faces.size());
    double triangleArea = 0.0;
    for (const manumesh::Face& face : mesh.faces) {
        const manumesh::Vec3& a = mesh.vertices[static_cast<std::size_t>(face.v[0])];
        const manumesh::Vec3& b = mesh.vertices[static_cast<std::size_t>(face.v[1])];
        const manumesh::Vec3& c = mesh.vertices[static_cast<std::size_t>(face.v[2])];
        const double signedAreaTwice = (b - a).cross(c - a).z();
        EXPECT_GT(signedAreaTwice, 0.0);
        triangleArea += 0.5 * signedAreaTwice;
    }
    EXPECT_NEAR(3.0, triangleArea, 1e-12);
}

TEST(MeshIo, ObjPreservesPerCornerTextureCoordinatesThroughConcaveEarClipping) {
    const std::filesystem::path path = writeTempFile(
        "manumesh_io_concave_polygon_uv.obj",
        "v 0 0 0\n"
        "v 2 0 0\n"
        "v 2 2 0\n"
        "v 1 1 0\n"
        "v 0 2 0\n"
        "vt 0 0\n"
        "vt 2 0\n"
        "vt 22 0\n"
        "vt 11 0\n"
        "vt 20 0\n"
        "f 1/1 2/2 3/3 4/4 5/5\n"
    );

    manumesh::Mesh mesh;
    std::string error;
    ASSERT_TRUE(manumesh::loadObj(path.string(), mesh, &error)) << error;
    std::filesystem::remove(path);

    ASSERT_EQ(3u, mesh.faces.size());
    ASSERT_EQ(mesh.faces.size(), mesh.faceTexCoords.size());
    for (std::size_t face = 0; face < mesh.faces.size(); ++face) {
        ASSERT_TRUE(mesh.faceTexCoords[face].valid);
        for (std::size_t corner = 0; corner < 3; ++corner) {
            const manumesh::Vec3& point = mesh.vertices[static_cast<std::size_t>(mesh.faces[face].v[corner])];
            EXPECT_DOUBLE_EQ(point.x() + 10.0 * point.y(), mesh.faceTexCoords[face].uv[corner].x());
        }
    }
}

TEST(MeshIo, ObjRejectsSelfIntersectingAndRepeatedPolygonFaces) {
    const std::filesystem::path selfIntersecting = writeTempFile(
        "manumesh_io_self_intersecting_polygon.obj",
        "v 0 0 0\n"
        "v 2 2 0\n"
        "v 0 2 0\n"
        "v 2 0 0\n"
        "f 1 2 3 4\n"
    );
    manumesh::Mesh mesh;
    std::string error;
    EXPECT_FALSE(manumesh::loadObj(selfIntersecting.string(), mesh, &error));
    EXPECT_NE(std::string::npos, error.find("self-intersecting"));
    std::filesystem::remove(selfIntersecting);

    const std::filesystem::path repeated = writeTempFile(
        "manumesh_io_repeated_polygon_corner.obj",
        "v 0 0 0\n"
        "v 2 0 0\n"
        "v 2 2 0\n"
        "v 0 2 0\n"
        "f 1 2 3 2 4\n"
    );
    error.clear();
    EXPECT_FALSE(manumesh::loadObj(repeated.string(), mesh, &error));
    EXPECT_NE(std::string::npos, error.find("repeats a corner position"));
    std::filesystem::remove(repeated);
}

TEST(MeshIo, ObjPreservesTextureCoordinatesThroughFanTriangulation) {
    const std::filesystem::path path = writeTempFile(
        "manumesh_io_uv_roundtrip.obj",
        "v 0 0 0\n"
        "v 1 0 0\n"
        "v 1 1 0\n"
        "v 0 1 0\n"
        "vt 0 0\n"
        "vt 1 0\n"
        "vt 1 1\n"
        "vt 0 1\n"
        "f 1/1 2/2 3/3 4/4\n"
    );

    manumesh::Mesh mesh;
    std::string error;
    ASSERT_TRUE(manumesh::loadObj(path.string(), mesh, &error)) << error;
    std::filesystem::remove(path);

    ASSERT_EQ(2u, mesh.faces.size());
    ASSERT_EQ(2u, mesh.faceTexCoords.size());
    ASSERT_TRUE(mesh.faceTexCoords[0].valid);
    ASSERT_TRUE(mesh.faceTexCoords[1].valid);
    EXPECT_TRUE(mesh.hasTextureCoordinates());
    // First fan triangle carries vt 1, 2, 3; second carries vt 1, 3, 4.
    EXPECT_DOUBLE_EQ(0.0, mesh.faceTexCoords[0].uv[0].x());
    EXPECT_DOUBLE_EQ(1.0, mesh.faceTexCoords[0].uv[1].x());
    EXPECT_DOUBLE_EQ(1.0, mesh.faceTexCoords[0].uv[2].x());
    EXPECT_DOUBLE_EQ(1.0, mesh.faceTexCoords[0].uv[2].y());
    EXPECT_DOUBLE_EQ(1.0, mesh.faceTexCoords[1].uv[1].x());
    EXPECT_DOUBLE_EQ(1.0, mesh.faceTexCoords[1].uv[1].y());
    EXPECT_DOUBLE_EQ(0.0, mesh.faceTexCoords[1].uv[2].x());
    EXPECT_DOUBLE_EQ(1.0, mesh.faceTexCoords[1].uv[2].y());
}

TEST(MeshIo, LoadStlClearsStaleTextureCoordinates) {
    const std::filesystem::path path = writeTempFile("manumesh_io_stale_uv.stl", singleAsciiStlTriangle());

    manumesh::Mesh mesh;
    manumesh::FaceTexCoords stale;
    stale.valid = true;
    mesh.faceTexCoords.push_back(stale);

    std::string error;
    ASSERT_TRUE(manumesh::loadStl(path.string(), mesh, &error)) << error;
    std::filesystem::remove(path);

    EXPECT_EQ(1u, mesh.faces.size());
    EXPECT_TRUE(mesh.faceTexCoords.empty());
    EXPECT_FALSE(mesh.hasTextureCoordinates());
}

TEST(MeshIo, TruncatedBinaryStlReportsBinaryError) {
    const std::vector<std::array<float, 9>> triangles = {{0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f}};
    const std::filesystem::path path = writeTempFile("manumesh_io_truncated.stl", makeBinaryStl(2, triangles, 0));

    manumesh::Mesh mesh;
    std::string error;
    EXPECT_FALSE(manumesh::loadStl(path.string(), mesh, &error));
    EXPECT_NE(error.find("binary"), std::string::npos) << error;
    std::filesystem::remove(path);
}

TEST(MeshIo, BinaryStlWithTrailingPaddingLoads) {
    const std::vector<std::array<float, 9>> triangles = {{0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f}};
    const std::filesystem::path path = writeTempFile("manumesh_io_padded.stl", makeBinaryStl(1, triangles, 6));

    manumesh::Mesh mesh;
    std::string error;
    ASSERT_TRUE(manumesh::loadStl(path.string(), mesh, &error)) << error;
    std::filesystem::remove(path);

    EXPECT_EQ(1u, mesh.faces.size());
    EXPECT_EQ(3u, mesh.vertices.size());
}

TEST(MeshIo, AsciiStlRoundTrip) {
    manumesh::Mesh mesh;
    mesh.vertices = {
        manumesh::Vec3(0.0, 0.0, 0.0),
        manumesh::Vec3(1.0, 0.0, 0.0),
        manumesh::Vec3(0.0, 1.0, 0.0),
        manumesh::Vec3(1.0, 1.0, 0.0),
    };
    mesh.faces = {{{0, 1, 2}}, {{1, 3, 2}}};

    const std::filesystem::path path = std::filesystem::temp_directory_path() / "manumesh_io_roundtrip.stl";
    std::string error;
    ASSERT_TRUE(manumesh::saveAsciiStl(path.string(), mesh, "roundtrip", &error)) << error;

    manumesh::Mesh loaded;
    ASSERT_TRUE(manumesh::loadStl(path.string(), loaded, &error)) << error;
    std::filesystem::remove(path);

    EXPECT_EQ(mesh.faces.size(), loaded.faces.size());
    EXPECT_EQ(mesh.vertices.size(), loaded.vertices.size());
    EXPECT_NEAR(0.0, (loaded.bboxMin() - mesh.bboxMin()).norm(), 1e-12);
    EXPECT_NEAR(0.0, (loaded.bboxMax() - mesh.bboxMax()).norm(), 1e-12);
}

TEST(MeshIo, LargeCoordinatesDoNotOverflowVertexMerging) {
    const std::filesystem::path path = writeTempFile(
        "manumesh_io_large_coords.stl",
        "solid big\n"
        "  facet normal 0 0 1\n"
        "    outer loop\n"
        "      vertex 0 0 0\n"
        "      vertex 1e30 0 0\n"
        "      vertex 0 1e30 0\n"
        "    endloop\n"
        "  endfacet\n"
        "  facet normal 0 0 1\n"
        "    outer loop\n"
        "      vertex 1e30 0 0\n"
        "      vertex 2e30 0 0\n"
        "      vertex 0 1e30 0\n"
        "    endloop\n"
        "  endfacet\n"
        "endsolid big\n"
    );

    manumesh::Mesh mesh;
    std::string error;
    // A zero relative epsilon forces the absolute 1e-12 quantization floor,
    // which used to push coordinate / epsilon far beyond the long long range.
    ASSERT_TRUE(manumesh::loadStl(path.string(), mesh, &error, 0.0)) << error;
    std::filesystem::remove(path);

    EXPECT_EQ(2u, mesh.faces.size());
    EXPECT_EQ(4u, mesh.vertices.size());
}

TEST(MeshIo, FiniteCoordinatesRemainDistinctWhenBoundingBoxNormOverflows) {
    const std::filesystem::path path = writeTempFile(
        "manumesh_io_overflowing_bbox_norm.stl",
        "solid huge\n"
        "  facet normal 0 0 1\n"
        "    outer loop\n"
        "      vertex 0 0 0\n"
        "      vertex 1e308 0 0\n"
        "      vertex 0 1e308 0\n"
        "    endloop\n"
        "  endfacet\n"
        "endsolid huge\n"
    );

    manumesh::Mesh mesh;
    std::string error;
    ASSERT_TRUE(manumesh::loadStl(path.string(), mesh, &error)) << error;
    std::filesystem::remove(path);

    EXPECT_EQ(1u, mesh.faces.size());
    EXPECT_EQ(3u, mesh.vertices.size());
}

TEST(MeshIo, StlVertexMergingSearchesAdjacentQuantizationBuckets) {
    const std::filesystem::path path = writeTempFile(
        "manumesh_io_adjacent_weld_buckets.stl",
        "solid adjacent\n"
        "  facet normal 0 0 1\n"
        "    outer loop\n"
        "      vertex 0.07 0 0\n"
        "      vertex 10 0 0\n"
        "      vertex 0 10 0\n"
        "    endloop\n"
        "  endfacet\n"
        "  facet normal 0 0 1\n"
        "    outer loop\n"
        "      vertex 0.15 0 0\n"
        "      vertex 10 0 0\n"
        "      vertex 10 10 0\n"
        "    endloop\n"
        "  endfacet\n"
        "endsolid adjacent\n"
    );

    manumesh::Mesh mesh;
    std::string error;
    ASSERT_TRUE(manumesh::loadStl(path.string(), mesh, &error, 0.01)) << error;
    std::filesystem::remove(path);

    EXPECT_EQ(2u, mesh.faces.size());
    EXPECT_EQ(4u, mesh.vertices.size());
    EXPECT_EQ(mesh.faces[0].v[0], mesh.faces[1].v[0]);
}

TEST(MeshIo, StlVertexMergingIsInvariantToLargeTranslation) {
    const std::filesystem::path originPath = writeTempFile(
        "manumesh_io_translation_origin.stl",
        "solid square\n"
        "  facet normal 0 0 1\n"
        "    outer loop\n"
        "      vertex 0 0 0\n"
        "      vertex 1 0 0\n"
        "      vertex 0 1 0\n"
        "    endloop\n"
        "  endfacet\n"
        "  facet normal 0 0 1\n"
        "    outer loop\n"
        "      vertex 1 0 0\n"
        "      vertex 1 1 0\n"
        "      vertex 0 1 0\n"
        "    endloop\n"
        "  endfacet\n"
        "endsolid square\n"
    );
    const std::filesystem::path translatedPath = writeTempFile(
        "manumesh_io_translation_1e12.stl",
        "solid square\n"
        "  facet normal 0 0 1\n"
        "    outer loop\n"
        "      vertex 1000000000000 1000000000000 1000000000000\n"
        "      vertex 1000000000001 1000000000000 1000000000000\n"
        "      vertex 1000000000000 1000000000001 1000000000000\n"
        "    endloop\n"
        "  endfacet\n"
        "  facet normal 0 0 1\n"
        "    outer loop\n"
        "      vertex 1000000000001 1000000000000 1000000000000\n"
        "      vertex 1000000000001 1000000000001 1000000000000\n"
        "      vertex 1000000000000 1000000000001 1000000000000\n"
        "    endloop\n"
        "  endfacet\n"
        "endsolid square\n"
    );

    for (int epsilonCase = 0; epsilonCase < 2; ++epsilonCase) {
        manumesh::Mesh origin;
        manumesh::Mesh translated;
        std::string error;
        const bool originLoaded = epsilonCase == 0 ? manumesh::loadStl(originPath.string(), origin, &error)
                                                   : manumesh::loadStl(originPath.string(), origin, &error, 0.0);
        ASSERT_TRUE(originLoaded) << "epsilon case " << epsilonCase << ": " << error;
        error.clear();
        const bool translatedLoaded = epsilonCase == 0
                                          ? manumesh::loadStl(translatedPath.string(), translated, &error)
                                          : manumesh::loadStl(translatedPath.string(), translated, &error, 0.0);
        ASSERT_TRUE(translatedLoaded) << "epsilon case " << epsilonCase << ": " << error;

        ASSERT_EQ(origin.vertices.size(), translated.vertices.size()) << "epsilon case " << epsilonCase;
        ASSERT_EQ(origin.faces.size(), translated.faces.size()) << "epsilon case " << epsilonCase;
        EXPECT_EQ(4u, translated.vertices.size()) << "epsilon case " << epsilonCase;
        EXPECT_EQ(2u, translated.faces.size()) << "epsilon case " << epsilonCase;
        const manumesh::Vec3 originLo = origin.bboxMin();
        const manumesh::Vec3 translatedLo = translated.bboxMin();
        for (std::size_t vertex = 0; vertex < origin.vertices.size(); ++vertex) {
            EXPECT_DOUBLE_EQ(
                0.0, ((origin.vertices[vertex] - originLo) - (translated.vertices[vertex] - translatedLo)).norm()
            ) << "epsilon case "
              << epsilonCase << ", vertex " << vertex;
        }
        for (std::size_t face = 0; face < origin.faces.size(); ++face) {
            EXPECT_EQ(origin.faces[face].v, translated.faces[face].v) << "epsilon case " << epsilonCase;
        }
    }

    std::filesystem::remove(originPath);
    std::filesystem::remove(translatedPath);
}

TEST(ManuMesh, MeshUtilitiesRejectMalformedInputWithoutThrowing) {
    const std::filesystem::path objPath = std::filesystem::temp_directory_path() / "mesh_io_bad_face.obj";
    {
        std::ofstream out(objPath);
        out << "v 0 0 0\n";
        out << "v 1 0 0\n";
        out << "v 0 1 0\n";
        out << "f nope 2 3\n";
    }

    manumesh::Mesh mesh;
    std::string error;
    EXPECT_FALSE(manumesh::loadObj(objPath.string(), mesh, &error));
    EXPECT_FALSE(error.empty());
    std::filesystem::remove(objPath);

    const std::filesystem::path degenerateStlPath =
        std::filesystem::temp_directory_path() / "mesh_io_degenerate_only.stl";
    {
        std::ofstream out(degenerateStlPath);
        out << "solid degenerate\n";
        out << "  facet normal 0 0 1\n";
        out << "    outer loop\n";
        out << "      vertex 0 0 0\n";
        out << "      vertex 0 0 0\n";
        out << "      vertex 1 0 0\n";
        out << "    endloop\n";
        out << "  endfacet\n";
        out << "endsolid degenerate\n";
    }
    error.clear();
    EXPECT_FALSE(manumesh::loadStl(degenerateStlPath.string(), mesh, &error));
    EXPECT_FALSE(error.empty());
    std::filesystem::remove(degenerateStlPath);

    manumesh::Mesh invalid;
    invalid.vertices = {manumesh::Vec3(0.0, 0.0, 0.0)};
    invalid.faces = {{{0, 1, 2}}};
    error.clear();
    const std::filesystem::path stlPath = std::filesystem::temp_directory_path() / "mesh_io_invalid_indices.stl";
    EXPECT_FALSE(manumesh::saveAsciiStl(stlPath.string(), invalid, "invalid", &error));
    EXPECT_FALSE(error.empty());

    manumesh::Mesh nonFinite;
    nonFinite.vertices = {
        manumesh::Vec3(0.0, 0.0, 0.0),
        manumesh::Vec3(std::numeric_limits<double>::infinity(), 0.0, 0.0),
        manumesh::Vec3(0.0, 1.0, 0.0),
    };
    nonFinite.faces = {{{0, 1, 2}}};
    error.clear();
    EXPECT_FALSE(manumesh::validateMeshGeometry(nonFinite, &error));
    EXPECT_FALSE(error.empty());

    manumesh::Mesh repeatedVertexFace;
    repeatedVertexFace.vertices = {
        manumesh::Vec3(0.0, 0.0, 0.0),
        manumesh::Vec3(1.0, 0.0, 0.0),
        manumesh::Vec3(0.0, 1.0, 0.0),
    };
    repeatedVertexFace.faces = {{{0, 1, 1}}};
    error.clear();
    EXPECT_FALSE(manumesh::validateMeshGeometry(repeatedVertexFace, &error));
    EXPECT_FALSE(error.empty());

    manumesh::Mesh zeroArea;
    zeroArea.vertices = {
        manumesh::Vec3(0.0, 0.0, 0.0),
        manumesh::Vec3(1.0, 0.0, 0.0),
        manumesh::Vec3(2.0, 0.0, 0.0),
    };
    zeroArea.faces = {{{0, 1, 2}}};
    error.clear();
    EXPECT_FALSE(manumesh::validateMeshGeometry(zeroArea, &error));
    EXPECT_FALSE(error.empty());
    error.clear();
    EXPECT_FALSE(manumesh::saveAsciiStl(stlPath.string(), zeroArea, "zero_area", &error));
    EXPECT_FALSE(error.empty());
}
