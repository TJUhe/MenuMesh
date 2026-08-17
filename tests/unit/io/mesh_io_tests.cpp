/**
 * @file tests/unit/io/mesh_io_tests.cpp
 * @brief 验证 OBJ/STL 解析、三角化、纹理坐标和错误报告契约。
 * @ingroup manumesh_tests
 */

#include "core/Mesh.h"
#include "io/MeshIo.h"

#include "core/Filesystem.h"
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <gtest/gtest.h>
#include <limits>
#include <string>
#include <vector>
namespace {

manumesh::filesystem::path writeTempFile(const std::string& name, const std::string& contents) {
    const manumesh::filesystem::path path = manumesh::filesystem::temp_directory_path() / name;
    std::ofstream out(path, std::ios::binary);
    out << contents;
    return path;
}

std::string readFileBytes(const manumesh::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return {};
    }
    in.seekg(0, std::ios::end);
    const std::streamoff size = in.tellg();
    if (size < 0) {
        return {};
    }
    in.seekg(0, std::ios::beg);
    std::string data(static_cast<std::size_t>(size), '\0');
    if (size > 0) {
        in.read(&data[0], static_cast<std::streamsize>(size));
    }
    return data;
}

std::uint32_t readUint32LE(const char* bytes) {
    const auto* data = reinterpret_cast<const unsigned char*>(bytes);
    return static_cast<std::uint32_t>(data[0]) | (static_cast<std::uint32_t>(data[1]) << 8u) |
           (static_cast<std::uint32_t>(data[2]) << 16u) | (static_cast<std::uint32_t>(data[3]) << 24u);
}

void appendUint32LE(std::string& out, std::uint32_t value) {
    out.push_back(static_cast<char>(value & 0xffu));
    out.push_back(static_cast<char>((value >> 8u) & 0xffu));
    out.push_back(static_cast<char>((value >> 16u) & 0xffu));
    out.push_back(static_cast<char>((value >> 24u) & 0xffu));
}

void appendUint16LE(std::string& out, std::uint16_t value) {
    out.push_back(static_cast<char>(value & 0xffu));
    out.push_back(static_cast<char>((value >> 8u) & 0xffu));
}

void appendFloatLE(std::string& out, float value) {
    std::uint32_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value), "Binary STL tests require 32-bit floats.");
    std::memcpy(&bits, &value, sizeof(bits));
    appendUint32LE(out, bits);
}

/// 构造二进制 STL 数据：包含全零头部（因此不会以 "solid" 开头）、声明的三角形数量、
/// 实际三角形记录，以及可选的尾部填充字节。
std::string makeBinaryStl(
    std::uint32_t declaredTriangles, const std::vector<std::array<float, 9>>& triangles, std::size_t trailingPadding
) {
    std::string data(80, '\0');
    appendUint32LE(data, declaredTriangles);
    for (const std::array<float, 9>& tri : triangles) {
        for (int k = 0; k < 3; ++k) {
            appendFloatLE(data, 0.0f); // 法向量
        }
        for (float coordinate : tri) {
            appendFloatLE(data, coordinate);
        }
        appendUint16LE(data, 0); // 属性字节数
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

} // 命名空间

TEST(MeshIo, SuccessfulLoadsClearStaleError) {
    const manumesh::filesystem::path objPath = writeTempFile(
        "mesh_io_clear_stale_error.obj",
        "v 0 0 0\n"
        "v 1 0 0\n"
        "v 0 1 0\n"
        "f 1 2 3\n"
    );
    const manumesh::filesystem::path stlPath = writeTempFile("mesh_io_clear_stale_error.stl", singleAsciiStlTriangle());

    manumesh::Mesh mesh;
    std::string error = "stale error";
    EXPECT_TRUE(manumesh::loadObj(objPath.string(), mesh, &error));
    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(mesh.faceTexCoords.empty());

    error = "stale error";
    EXPECT_TRUE(manumesh::loadStl(stlPath.string(), mesh, &error));
    EXPECT_TRUE(error.empty());

    error = "stale error";
    EXPECT_TRUE(manumesh::loadMesh(objPath.string(), mesh, &error));
    EXPECT_TRUE(error.empty());

    manumesh::filesystem::remove(objPath);
    manumesh::filesystem::remove(stlPath);
}

TEST(MeshIo, FailedLoadsPreserveExistingMeshAndObjAcceptsInlineComments) {
    const manumesh::filesystem::path validPath = writeTempFile(
        "manumesh_io_inline_comment.obj",
        "v 0 0 0 # origin\n"
        "v 1 0 0\n"
        "v 0 1 0\n"
        "f 1 2 3 # triangle\n"
    );
    const manumesh::filesystem::path invalidPath = writeTempFile(
        "manumesh_io_preserve_on_failure.obj",
        "v 0 0 0\n"
        "v 1 0 0\n"
        "v 0 1 0\n"
        "f 1/ 2 3\n"
    );

    manumesh::Mesh mesh;
    mesh.vertices = {manumesh::Vec3(7.0, 8.0, 9.0), manumesh::Vec3(8.0, 8.0, 9.0), manumesh::Vec3(7.0, 9.0, 9.0)};
    mesh.faces = {{{0, 1, 2}}};
    const manumesh::Mesh original = mesh;
    std::string error;
    EXPECT_FALSE(manumesh::loadObj(invalidPath.string(), mesh, &error));
    ASSERT_EQ(original.vertices.size(), mesh.vertices.size());
    for (std::size_t i = 0; i < original.vertices.size(); ++i) {
        EXPECT_DOUBLE_EQ(original.vertices[i].x(), mesh.vertices[i].x());
        EXPECT_DOUBLE_EQ(original.vertices[i].y(), mesh.vertices[i].y());
        EXPECT_DOUBLE_EQ(original.vertices[i].z(), mesh.vertices[i].z());
    }
    EXPECT_EQ(original.faces.size(), mesh.faces.size());
    EXPECT_EQ(original.faces[0].v, mesh.faces[0].v);

    ASSERT_TRUE(manumesh::loadObj(validPath.string(), mesh, &error)) << error;
    EXPECT_EQ(3u, mesh.vertices.size());
    EXPECT_EQ(1u, mesh.faces.size());
    manumesh::filesystem::remove(validPath);
    manumesh::filesystem::remove(invalidPath);
}

TEST(MeshIo, ObjRejectsMalformedNormalAndExtraSlashReferences) {
    const std::vector<std::string> invalidFaces = {"f 1// 2//1 3//1\n", "f 1/1/1/1 2/1/1 3/1/1\n"};
    for (std::size_t i = 0; i < invalidFaces.size(); ++i) {
        const manumesh::filesystem::path path = writeTempFile(
            "manumesh_io_bad_face_slashes_" + std::to_string(i) + ".obj",
            "v 0 0 0\n"
            "v 1 0 0\n"
            "v 0 1 0\n"
            "vt 0 0\n"
            "vn 0 0 1\n" +
                invalidFaces[i]
        );
        manumesh::Mesh mesh;
        std::string error;
        EXPECT_FALSE(manumesh::loadObj(path.string(), mesh, &error)) << invalidFaces[i];
        EXPECT_NE(std::string::npos, error.find("line 6"));
        manumesh::filesystem::remove(path);
    }
}

TEST(MeshIo, AsciiStlRejectsVertexTrailingDataAndPreservesTarget) {
    const manumesh::filesystem::path path = writeTempFile(
        "manumesh_io_ascii_vertex_trailing_data.stl",
        "solid bad\n"
        "  facet normal 0 0 1\n"
        "    outer loop\n"
        "      vertex 0 0 0 unexpected\n"
        "      vertex 1 0 0\n"
        "      vertex 0 1 0\n"
        "    endloop\n"
        "  endfacet\n"
        "endsolid bad\n"
    );
    manumesh::Mesh mesh;
    mesh.vertices = {
        manumesh::Vec3(7.0, 8.0, 9.0),
        manumesh::Vec3(8.0, 8.0, 9.0),
        manumesh::Vec3(7.0, 9.0, 9.0),
    };
    mesh.faces = {{{0, 1, 2}}};
    const manumesh::Mesh original = mesh;
    std::string error;
    EXPECT_FALSE(manumesh::loadStl(path.string(), mesh, &error));
    EXPECT_FALSE(error.empty());
    EXPECT_EQ(original.vertices.size(), mesh.vertices.size());
    EXPECT_EQ(original.faces.size(), mesh.faces.size());
    EXPECT_EQ(original.faces[0].v, mesh.faces[0].v);
    manumesh::filesystem::remove(path);
}

TEST(MeshIo, AsciiStlRejectsIncompleteFacetStructure) {
    const manumesh::filesystem::path path = writeTempFile(
        "manumesh_io_ascii_missing_endfacet.stl",
        "solid malformed\n"
        "  facet normal 0 0 1\n"
        "    outer loop\n"
        "      vertex 0 0 0\n"
        "      vertex 1 0 0\n"
        "      vertex 0 1 0\n"
        "    endloop\n"
        "endsolid malformed\n"
    );

    manumesh::Mesh mesh;
    std::string error;
    EXPECT_FALSE(manumesh::loadStl(path.string(), mesh, &error));
    EXPECT_NE(std::string::npos, error.find("endfacet")) << error;
    manumesh::filesystem::remove(path);
}

TEST(MeshIo, AsciiStlAcceptsConcatenatedSolidBlocks) {
    const manumesh::filesystem::path path = writeTempFile(
        "manumesh_io_concatenated_solids.stl",
        "solid lower\n"
        "  facet normal 0 0 1\n"
        "    outer loop\n"
        "      vertex 0 0 0\n"
        "      vertex 1 0 0\n"
        "      vertex 0 1 0\n"
        "    endloop\n"
        "  endfacet\n"
        "endsolid lower\n"
        "solid upper\n"
        "  facet normal 0 0 1\n"
        "    outer loop\n"
        "      vertex 0 0 1\n"
        "      vertex 1 0 1\n"
        "      vertex 0 1 1\n"
        "    endloop\n"
        "  endfacet\n"
        "endsolid upper\n"
    );

    manumesh::Mesh mesh;
    std::string error;
    ASSERT_TRUE(manumesh::loadStl(path.string(), mesh, &error)) << error;
    EXPECT_EQ(2u, mesh.faces.size());
    EXPECT_EQ(6u, mesh.vertices.size());
    manumesh::filesystem::remove(path);
}

TEST(MeshIo, StlRejectsCollinearFacetAndPreservesTarget) {
    const manumesh::filesystem::path path = writeTempFile(
        "manumesh_io_collinear_facet.stl",
        "solid collinear\n"
        "  facet normal 0 0 1\n"
        "    outer loop\n"
        "      vertex 0 0 0\n"
        "      vertex 1 0 0\n"
        "      vertex 2 0 0\n"
        "    endloop\n"
        "  endfacet\n"
        "endsolid collinear\n"
    );
    manumesh::Mesh mesh;
    mesh.vertices = {manumesh::Vec3(0.0, 0.0, 0.0), manumesh::Vec3(1.0, 0.0, 0.0), manumesh::Vec3(0.0, 1.0, 0.0)};
    mesh.faces = {{{0, 1, 2}}};
    const manumesh::Mesh original = mesh;

    std::string error;
    EXPECT_FALSE(manumesh::loadStl(path.string(), mesh, &error));
    EXPECT_NE(std::string::npos, error.find("zero area")) << error;
    EXPECT_EQ(original.vertices.size(), mesh.vertices.size());
    ASSERT_EQ(original.faces.size(), mesh.faces.size());
    EXPECT_EQ(original.faces[0].v, mesh.faces[0].v);
    manumesh::filesystem::remove(path);
}

TEST(MeshIo, BinaryStlWriteRejectsFloatQuantizationDegeneracyWithoutOverwritingOutput) {
    const manumesh::filesystem::path path = writeTempFile("manumesh_io_existing_output.stl", "preserve me");
    manumesh::Mesh mesh;
    mesh.vertices = {
        manumesh::Vec3(100000000.0, 0.0, 0.0),
        manumesh::Vec3(100000001.0, 0.0, 0.0),
        manumesh::Vec3(100000000.0, 1.0, 0.0),
    };
    mesh.faces = {{{0, 1, 2}}};
    std::string error;
    EXPECT_FALSE(manumesh::saveBinaryStl(path.string(), mesh, &error));
    EXPECT_NE(std::string::npos, error.find("float32"));
    EXPECT_EQ("preserve me", readFileBytes(path));
    manumesh::filesystem::remove(path);
}

TEST(MeshIo, BinaryStlWriteRejectsSubthresholdFloatQuantizationWithoutOverwritingOutput) {
    const manumesh::filesystem::path path = writeTempFile("manumesh_io_subthreshold_output.stl", "preserve me");
    manumesh::Mesh mesh;
    mesh.vertices = {
        manumesh::Vec3(0.0, 0.0, 0.0),
        manumesh::Vec3(1e-10, 1e-10, 0.0),
        manumesh::Vec3(2e-10, 2e-10 + 2.000001e-14, 0.0),
    };
    mesh.faces = {{{0, 1, 2}}};
    ASSERT_GT(manumesh::triangleArea(mesh.vertices[0], mesh.vertices[1], mesh.vertices[2]), 1e-24);

    std::string error;
    EXPECT_FALSE(manumesh::saveBinaryStl(path.string(), mesh, &error));
    EXPECT_NE(std::string::npos, error.find("degenerate after binary STL float32 conversion")) << error;
    EXPECT_EQ("preserve me", readFileBytes(path));
    manumesh::filesystem::remove(path);
}

TEST(MeshIo, MalformedObjVertexLineIsAnErrorWithLineNumber) {
    const manumesh::filesystem::path path = writeTempFile(
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
    manumesh::filesystem::remove(path);
}

TEST(MeshIo, ObjRejectsNonDecimalOrPartiallyParsedCoordinates) {
    const std::vector<std::string> invalidCoordinates = {
        "0x1p0",
        "+0x1p0",
        "-0x1p0",
        "1x",
        "1e9999",
        "1e-9999",
    };

    for (std::size_t i = 0; i < invalidCoordinates.size(); ++i) {
        const manumesh::filesystem::path path = writeTempFile(
            "manumesh_io_invalid_coordinate_" + std::to_string(i) + ".obj",
            "v " + invalidCoordinates[i] + " 0 0\n" + "v 1 0 0\n" + "v 0 1 0\n" + "f 1 2 3\n"
        );

        manumesh::Mesh mesh;
        std::string error;
        EXPECT_FALSE(manumesh::loadObj(path.string(), mesh, &error)) << invalidCoordinates[i];
        EXPECT_NE(std::string::npos, error.find("line 1")) << error;
        manumesh::filesystem::remove(path);
    }
}

TEST(MeshIo, ObjAppliesHomogeneousVertexCoordinate) {
    const manumesh::filesystem::path path = writeTempFile(
        "manumesh_io_homogeneous_vertex.obj",
        "v 2 0 0 2\n"
        "v 0 2 0 2\n"
        "v 0 0 0 2\n"
        "f 1 2 3\n"
    );

    manumesh::Mesh mesh;
    std::string error;
    ASSERT_TRUE(manumesh::loadObj(path.string(), mesh, &error)) << error;
    manumesh::filesystem::remove(path);

    ASSERT_EQ(3u, mesh.vertices.size());
    EXPECT_DOUBLE_EQ(1.0, mesh.vertices[0].x());
    EXPECT_DOUBLE_EQ(1.0, mesh.vertices[1].y());
    EXPECT_DOUBLE_EQ(0.5, manumesh::triangleArea(mesh.vertices[0], mesh.vertices[1], mesh.vertices[2]));
}

TEST(MeshIo, ObjRejectsZeroHomogeneousCoordinate) {
    const manumesh::filesystem::path path = writeTempFile(
        "manumesh_io_zero_homogeneous_vertex.obj",
        "v 1 0 0 0\n"
        "v 0 1 0\n"
        "v 0 0 0\n"
        "f 1 2 3\n"
    );

    manumesh::Mesh mesh;
    std::string error;
    EXPECT_FALSE(manumesh::loadObj(path.string(), mesh, &error));
    EXPECT_NE(std::string::npos, error.find("homogeneous")) << error;
    manumesh::filesystem::remove(path);
}

TEST(MeshIo, ObjAcceptsSingleComponentTextureCoordinate) {
    const manumesh::filesystem::path path = writeTempFile(
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
    manumesh::filesystem::remove(path);

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
    const manumesh::filesystem::path path = writeTempFile(
        "manumesh_io_negative_indices.obj",
        "v 0 0 0\n"
        "v 1 0 0\n"
        "v 0 1 0\n"
        "f -3 -2 -1\n"
    );

    manumesh::Mesh mesh;
    std::string error;
    ASSERT_TRUE(manumesh::loadObj(path.string(), mesh, &error)) << error;
    manumesh::filesystem::remove(path);

    ASSERT_EQ(1u, mesh.faces.size());
    EXPECT_EQ(0, mesh.faces[0].v[0]);
    EXPECT_EQ(1, mesh.faces[0].v[1]);
    EXPECT_EQ(2, mesh.faces[0].v[2]);
    EXPECT_TRUE(mesh.faceTexCoords.empty());
}

TEST(MeshIo, ObjParsesAllFaceCornerFormats) {
    const manumesh::filesystem::path path = writeTempFile(
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
    manumesh::filesystem::remove(path);

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
    const manumesh::filesystem::path path = writeTempFile(
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
    manumesh::filesystem::remove(path);

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
    const manumesh::filesystem::path path = writeTempFile(
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
    manumesh::filesystem::remove(path);

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
    const manumesh::filesystem::path path = writeTempFile(
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
    manumesh::filesystem::remove(path);

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
    const manumesh::filesystem::path selfIntersecting = writeTempFile(
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
    manumesh::filesystem::remove(selfIntersecting);

    const manumesh::filesystem::path repeated = writeTempFile(
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
    manumesh::filesystem::remove(repeated);
}

TEST(MeshIo, ObjPreservesTextureCoordinatesThroughFanTriangulation) {
    const manumesh::filesystem::path path = writeTempFile(
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
    manumesh::filesystem::remove(path);

    ASSERT_EQ(2u, mesh.faces.size());
    ASSERT_EQ(2u, mesh.faceTexCoords.size());
    ASSERT_TRUE(mesh.faceTexCoords[0].valid);
    ASSERT_TRUE(mesh.faceTexCoords[1].valid);
    EXPECT_TRUE(mesh.hasTextureCoordinates());
    // 第一个扇形三角形携带 vt 1、2、3；第二个携带 vt 1、3、4。
    EXPECT_DOUBLE_EQ(0.0, mesh.faceTexCoords[0].uv[0].x());
    EXPECT_DOUBLE_EQ(1.0, mesh.faceTexCoords[0].uv[1].x());
    EXPECT_DOUBLE_EQ(1.0, mesh.faceTexCoords[0].uv[2].x());
    EXPECT_DOUBLE_EQ(1.0, mesh.faceTexCoords[0].uv[2].y());
    EXPECT_DOUBLE_EQ(1.0, mesh.faceTexCoords[1].uv[1].x());
    EXPECT_DOUBLE_EQ(1.0, mesh.faceTexCoords[1].uv[1].y());
    EXPECT_DOUBLE_EQ(0.0, mesh.faceTexCoords[1].uv[2].x());
    EXPECT_DOUBLE_EQ(1.0, mesh.faceTexCoords[1].uv[2].y());
}

TEST(MeshIo, ObjKeepsTextureCoordinateAlignmentAfterLateTexturedFace) {
    const manumesh::filesystem::path path = writeTempFile(
        "manumesh_io_late_texture_coordinates.obj",
        "v 0 0 0\n"
        "v 1 0 0\n"
        "v 0 1 0\n"
        "v 2 0 0\n"
        "v 3 0 0\n"
        "v 2 1 0\n"
        "vt 0 0\n"
        "vt 1 0\n"
        "vt 0 1\n"
        "f 1 2 3\n"
        "f 4/1 5/2 6/3\n"
    );

    manumesh::Mesh mesh;
    std::string error;
    ASSERT_TRUE(manumesh::loadObj(path.string(), mesh, &error)) << error;
    manumesh::filesystem::remove(path);

    ASSERT_EQ(2u, mesh.faces.size());
    ASSERT_EQ(2u, mesh.faceTexCoords.size());
    EXPECT_FALSE(mesh.faceTexCoords[0].valid);
    EXPECT_TRUE(mesh.faceTexCoords[1].valid);
    EXPECT_DOUBLE_EQ(1.0, mesh.faceTexCoords[1].uv[1].x());
    EXPECT_DOUBLE_EQ(1.0, mesh.faceTexCoords[1].uv[2].y());
}

TEST(MeshIo, LoadStlClearsStaleTextureCoordinates) {
    const manumesh::filesystem::path path = writeTempFile("manumesh_io_stale_uv.stl", singleAsciiStlTriangle());

    manumesh::Mesh mesh;
    manumesh::FaceTexCoords stale;
    stale.valid = true;
    mesh.faceTexCoords.push_back(stale);

    std::string error;
    ASSERT_TRUE(manumesh::loadStl(path.string(), mesh, &error)) << error;
    manumesh::filesystem::remove(path);

    EXPECT_EQ(1u, mesh.faces.size());
    EXPECT_TRUE(mesh.faceTexCoords.empty());
    EXPECT_FALSE(mesh.hasTextureCoordinates());
}

TEST(MeshIo, TruncatedBinaryStlReportsBinaryError) {
    const std::vector<std::array<float, 9>> triangles = {{0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f}};
    const manumesh::filesystem::path path = writeTempFile("manumesh_io_truncated.stl", makeBinaryStl(2, triangles, 0));

    manumesh::Mesh mesh;
    std::string error;
    EXPECT_FALSE(manumesh::loadStl(path.string(), mesh, &error));
    EXPECT_NE(error.find("binary"), std::string::npos) << error;
    manumesh::filesystem::remove(path);
}

TEST(MeshIo, BinaryStlWithTrailingPaddingLoads) {
    const std::vector<std::array<float, 9>> triangles = {{0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f}};
    const manumesh::filesystem::path path = writeTempFile("manumesh_io_padded.stl", makeBinaryStl(1, triangles, 6));

    manumesh::Mesh mesh;
    std::string error;
    ASSERT_TRUE(manumesh::loadStl(path.string(), mesh, &error)) << error;
    manumesh::filesystem::remove(path);

    EXPECT_EQ(1u, mesh.faces.size());
    EXPECT_EQ(3u, mesh.vertices.size());
}

TEST(MeshIo, BinaryStlWithSolidHeaderAndTrailingPaddingLoads) {
    const std::vector<std::array<float, 9>> triangles = {{0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f}};
    std::string bytes = makeBinaryStl(1, triangles, 6);
    const char solidHeader[] = "solid padded binary";
    std::memcpy(&bytes[0], solidHeader, sizeof(solidHeader) - 1);
    const manumesh::filesystem::path path = writeTempFile("manumesh_io_solid_padded.stl", bytes);

    manumesh::Mesh mesh;
    std::string error;
    ASSERT_TRUE(manumesh::loadStl(path.string(), mesh, &error)) << error;
    manumesh::filesystem::remove(path);

    EXPECT_EQ(1u, mesh.faces.size());
    EXPECT_EQ(3u, mesh.vertices.size());
}

TEST(MeshIo, BinaryStlRoundTripUsesStandardLayout) {
    manumesh::Mesh mesh;
    mesh.vertices = {
        manumesh::Vec3(0.0, 0.0, 0.0),
        manumesh::Vec3(1.0, 0.0, 0.0),
        manumesh::Vec3(0.0, 1.0, 0.0),
        manumesh::Vec3(1.0, 1.0, 0.0),
    };
    mesh.faces = {{{0, 1, 2}}, {{1, 3, 2}}};

    const manumesh::filesystem::path path = manumesh::filesystem::temp_directory_path() / "manumesh_io_roundtrip.stl";
    std::string error;
    ASSERT_TRUE(manumesh::saveBinaryStl(path.string(), mesh, &error)) << error;

    const std::string bytes = readFileBytes(path);
    ASSERT_EQ(84u + 50u * mesh.faces.size(), bytes.size());
    EXPECT_EQ("ManuMesh binary STL", bytes.substr(0, 19));
    EXPECT_EQ(mesh.faces.size(), readUint32LE(bytes.data() + 80));
    EXPECT_EQ('\0', bytes[84 + 48]);
    EXPECT_EQ('\0', bytes[84 + 49]);

    manumesh::Mesh loaded;
    ASSERT_TRUE(manumesh::loadStl(path.string(), loaded, &error)) << error;
    manumesh::filesystem::remove(path);

    EXPECT_EQ(mesh.faces.size(), loaded.faces.size());
    EXPECT_EQ(mesh.vertices.size(), loaded.vertices.size());
    EXPECT_NEAR(0.0, (loaded.bboxMin() - mesh.bboxMin()).norm(), 1e-12);
    EXPECT_NEAR(0.0, (loaded.bboxMax() - mesh.bboxMax()).norm(), 1e-12);
}

TEST(MeshIo, BinaryStlRoundTripsThroughUtf8WindowsPath) {
    manumesh::Mesh mesh;
    mesh.vertices = {
        manumesh::Vec3(0.0, 0.0, 0.0),
        manumesh::Vec3(1.0, 0.0, 0.0),
        manumesh::Vec3(0.0, 1.0, 0.0),
    };
    mesh.faces = {{{0, 1, 2}}};

    const manumesh::filesystem::path path =
        manumesh::filesystem::temp_directory_path() / manumesh::filesystem::u8path("manumesh_路径_🚀.stl");
    const std::string utf8Path = path.u8string();
    manumesh::filesystem::remove(path);

    std::string error;
    ASSERT_TRUE(manumesh::saveBinaryStl(utf8Path, mesh, &error)) << error;
    ASSERT_TRUE(manumesh::filesystem::exists(path));

    manumesh::Mesh loaded;
    ASSERT_TRUE(manumesh::loadStl(utf8Path, loaded, &error)) << error;
    EXPECT_EQ(mesh.faces.size(), loaded.faces.size());
    EXPECT_EQ(mesh.vertices.size(), loaded.vertices.size());
}

TEST(MeshIo, BinaryStlRejectsCoordinatesOutsideFloat32RangeBeforeOpeningFile) {
    manumesh::Mesh mesh;
    const double outsideFloatRange = static_cast<double>(std::numeric_limits<float>::max()) * 2.0;
    mesh.vertices = {
        manumesh::Vec3(0.0, 0.0, 0.0),
        manumesh::Vec3(outsideFloatRange, 0.0, 0.0),
        manumesh::Vec3(0.0, 1.0, 0.0),
    };
    mesh.faces = {{{0, 1, 2}}};

    const manumesh::filesystem::path path =
        manumesh::filesystem::temp_directory_path() / "manumesh_io_outside_float_range.stl";
    manumesh::filesystem::remove(path);
    std::string error;
    EXPECT_FALSE(manumesh::saveBinaryStl(path.string(), mesh, &error));
    EXPECT_NE(std::string::npos, error.find("float32")) << error;
    EXPECT_FALSE(manumesh::filesystem::exists(path));
}

TEST(MeshIo, LargeCoordinatesDoNotOverflowVertexMerging) {
    const manumesh::filesystem::path path = writeTempFile(
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
    // 相对 epsilon 为零时会触发绝对 1e-12 量化下限；过去这会使坐标/epsilon
    // 的比值远超 long long 可表示范围。
    ASSERT_TRUE(manumesh::loadStl(path.string(), mesh, &error, 0.0)) << error;
    manumesh::filesystem::remove(path);

    EXPECT_EQ(2u, mesh.faces.size());
    EXPECT_EQ(4u, mesh.vertices.size());
}

TEST(MeshIo, CoordinatesOutsideSupportedNumericRangeAreRejectedTransactionally) {
    const manumesh::filesystem::path path = writeTempFile(
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
    mesh.vertices = {
        manumesh::Vec3(0.0, 0.0, 0.0),
        manumesh::Vec3(1.0, 0.0, 0.0),
        manumesh::Vec3(0.0, 1.0, 0.0),
    };
    mesh.faces = {{{0, 1, 2}}};
    const manumesh::Mesh original = mesh;
    std::string error;
    EXPECT_FALSE(manumesh::loadStl(path.string(), mesh, &error));
    manumesh::filesystem::remove(path);

    EXPECT_NE(std::string::npos, error.find("numeric coordinate range"));
    EXPECT_EQ(original.faces.size(), mesh.faces.size());
    EXPECT_EQ(original.vertices.size(), mesh.vertices.size());
    EXPECT_TRUE(original.vertices[1].isApprox(mesh.vertices[1]));
}

TEST(MeshIo, StlVertexMergingSearchesAdjacentQuantizationBuckets) {
    const manumesh::filesystem::path path = writeTempFile(
        "manumesh_io_adjacent_merge_buckets.stl",
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
    manumesh::filesystem::remove(path);

    EXPECT_EQ(2u, mesh.faces.size());
    EXPECT_EQ(4u, mesh.vertices.size());
    EXPECT_EQ(mesh.faces[0].v[0], mesh.faces[1].v[0]);
}

TEST(MeshIo, StlVertexMergingIsInvariantToLargeTranslation) {
    const manumesh::filesystem::path originPath = writeTempFile(
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
    const manumesh::filesystem::path translatedPath = writeTempFile(
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

    manumesh::filesystem::remove(originPath);
    manumesh::filesystem::remove(translatedPath);
}

TEST(MeshIo, SaveObjRoundTripsMixedPerFaceTextureCoordinates) {
    manumesh::Mesh original;
    original.vertices = {
        manumesh::Vec3(0.0, 0.0, 0.0),
        manumesh::Vec3(1.0, 0.0, 0.0),
        manumesh::Vec3(0.0, 1.0, 0.0),
        manumesh::Vec3(1.0, 1.0, 0.0),
    };
    original.faces = {
        manumesh::Face{{0, 1, 2}},
        manumesh::Face{{1, 3, 2}},
    };
    original.faceTexCoords.resize(2);
    original.faceTexCoords[0].valid = true;
    original.faceTexCoords[0].uv = {
        manumesh::Vec2(std::numeric_limits<double>::denorm_min(), -std::numeric_limits<double>::denorm_min()),
        manumesh::Vec2(1.0, 0.0),
        manumesh::Vec2(0.0, 1.0),
    };

    const manumesh::filesystem::path path =
        manumesh::filesystem::temp_directory_path() / "manumesh_io_uv_export_roundtrip.obj";
    manumesh::filesystem::remove(path);
    std::string error;
    ASSERT_TRUE(manumesh::saveObj(path.string(), original, &error)) << error;

    manumesh::Mesh loaded;
    ASSERT_TRUE(manumesh::loadObj(path.string(), loaded, &error)) << error;
    manumesh::filesystem::remove(path);
    EXPECT_EQ(original.vertices.size(), loaded.vertices.size());
    ASSERT_EQ(original.faces.size(), loaded.faces.size());
    EXPECT_EQ(original.faces[0].v, loaded.faces[0].v);
    EXPECT_EQ(original.faces[1].v, loaded.faces[1].v);
    ASSERT_EQ(2u, loaded.faceTexCoords.size());
    EXPECT_TRUE(loaded.faceTexCoords[0].valid);
    EXPECT_FALSE(loaded.faceTexCoords[1].valid);
    EXPECT_DOUBLE_EQ(std::numeric_limits<double>::denorm_min(), loaded.faceTexCoords[0].uv[0].x());
    EXPECT_DOUBLE_EQ(-std::numeric_limits<double>::denorm_min(), loaded.faceTexCoords[0].uv[0].y());
    EXPECT_TRUE(loaded.faceTexCoords[0].uv[1].isApprox(manumesh::Vec2(1.0, 0.0), 1e-15));
    EXPECT_TRUE(loaded.faceTexCoords[0].uv[2].isApprox(manumesh::Vec2(0.0, 1.0), 1e-15));
}

TEST(MeshIo, SaveObjRoundTripsExtremeFiniteTriangleAspectRatio) {
    manumesh::Mesh original;
    original.vertices = {
        manumesh::Vec3(0.0, 0.0, 0.0),
        manumesh::Vec3(1e100, 0.0, 0.0),
        manumesh::Vec3(0.0, 1e-100, 0.0),
    };
    original.faces = {manumesh::Face{{0, 1, 2}}};
    ASSERT_DOUBLE_EQ(0.5, manumesh::triangleArea(original.vertices[0], original.vertices[1], original.vertices[2]));

    const manumesh::filesystem::path path =
        manumesh::filesystem::temp_directory_path() / "manumesh_io_extreme_aspect_roundtrip.obj";
    manumesh::filesystem::remove(path);
    std::string error;
    ASSERT_TRUE(manumesh::saveObj(path.string(), original, &error)) << error;

    manumesh::Mesh loaded;
    ASSERT_TRUE(manumesh::loadObj(path.string(), loaded, &error)) << error;
    manumesh::filesystem::remove(path);
    ASSERT_EQ(1u, loaded.faces.size());
    EXPECT_EQ(original.faces[0].v, loaded.faces[0].v);
    EXPECT_DOUBLE_EQ(0.5, manumesh::triangleArea(loaded.vertices[0], loaded.vertices[1], loaded.vertices[2]));
}

TEST(ManuMesh, MeshUtilitiesRejectMalformedInputWithoutThrowing) {
    const manumesh::filesystem::path objPath = manumesh::filesystem::temp_directory_path() / "mesh_io_bad_face.obj";
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
    manumesh::filesystem::remove(objPath);

    const manumesh::filesystem::path degenerateStlPath =
        manumesh::filesystem::temp_directory_path() / "mesh_io_degenerate_only.stl";
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
    manumesh::filesystem::remove(degenerateStlPath);

    manumesh::Mesh invalid;
    invalid.vertices = {manumesh::Vec3(0.0, 0.0, 0.0)};
    invalid.faces = {{{0, 1, 2}}};
    error.clear();
    const manumesh::filesystem::path stlPath =
        manumesh::filesystem::temp_directory_path() / "mesh_io_invalid_indices.stl";
    EXPECT_FALSE(manumesh::saveBinaryStl(stlPath.string(), invalid, &error));
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
    EXPECT_FALSE(manumesh::saveBinaryStl(stlPath.string(), zeroArea, &error));
    EXPECT_FALSE(error.empty());
}
