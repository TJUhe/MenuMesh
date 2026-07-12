#include "io/MeshIo.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <system_error>
#include <unordered_map>
#include <vector>

namespace manumesh {
namespace {

struct QuantizedKey {
    long long x = 0;
    long long y = 0;
    long long z = 0;

    bool operator==(const QuantizedKey& other) const { return x == other.x && y == other.y && z == other.z; }
};

struct QuantizedKeyHash {
    std::size_t operator()(const QuantizedKey& key) const {
        std::size_t h = 1469598103934665603ull;
        auto mix = [&](long long value) {
            std::size_t v = static_cast<std::size_t>(value);
            h ^= v + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
        };
        mix(key.x);
        mix(key.y);
        mix(key.z);
        return h;
    }
};

QuantizedKey makeKey(const Vec3& p, double eps) {
    return {
        static_cast<long long>(std::llround(p.x() / eps)),
        static_cast<long long>(std::llround(p.y() / eps)),
        static_cast<long long>(std::llround(p.z() / eps))
    };
}

uint32_t readUint32LE(const char* bytes) {
    uint32_t value = 0;
    std::memcpy(&value, bytes, sizeof(uint32_t));
    return value;
}

float readFloatLE(const char* bytes) {
    float value = 0.0f;
    std::memcpy(&value, bytes, sizeof(float));
    return value;
}

bool finitePoint(const Vec3& p) { return std::isfinite(p.x()) && std::isfinite(p.y()) && std::isfinite(p.z()); }

bool isBinaryStl(const std::string& path, uint32_t& triangleCount) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return false;
    }

    std::array<char, 84> header{};
    in.read(header.data(), static_cast<std::streamsize>(header.size()));
    if (in.gcount() != static_cast<std::streamsize>(header.size())) {
        return false;
    }

    triangleCount = readUint32LE(header.data() + 80);
    std::error_code ec;
    const auto fileSize = std::filesystem::file_size(path, ec);
    if (ec) {
        return false;
    }
    const auto expectedSize = static_cast<std::uintmax_t>(84) + static_cast<std::uintmax_t>(triangleCount) * 50u;
    return fileSize == expectedSize;
}

bool readBinaryTriangles(const std::string& path, std::vector<std::array<Vec3, 3>>& triangles, std::string* error) {
    uint32_t triangleCount = 0;
    if (!isBinaryStl(path, triangleCount)) {
        return false;
    }

    std::ifstream in(path, std::ios::binary);
    if (!in) {
        if (error)
            *error = "Failed to open binary STL.";
        return false;
    }

    in.seekg(84, std::ios::beg);
    triangles.clear();
    triangles.reserve(triangleCount);

    std::array<char, 50> record{};
    for (uint32_t i = 0; i < triangleCount; ++i) {
        in.read(record.data(), static_cast<std::streamsize>(record.size()));
        if (!in) {
            if (error)
                *error = "Unexpected end of binary STL.";
            return false;
        }

        std::array<Vec3, 3> tri{};
        for (int k = 0; k < 3; ++k) {
            const int offset = 12 + k * 12;
            tri[k] = Vec3(
                readFloatLE(record.data() + offset + 0),
                readFloatLE(record.data() + offset + 4),
                readFloatLE(record.data() + offset + 8)
            );
            if (!finitePoint(tri[k])) {
                if (error)
                    *error = "Binary STL contains a non-finite vertex coordinate.";
                return false;
            }
        }
        triangles.push_back(tri);
    }

    return true;
}

bool readAsciiTriangles(const std::string& path, std::vector<std::array<Vec3, 3>>& triangles, std::string* error) {
    std::ifstream in(path);
    if (!in) {
        if (error)
            *error = "Failed to open ASCII STL.";
        return false;
    }

    triangles.clear();
    std::vector<Vec3> pending;
    std::string token;
    while (in >> token) {
        if (token == "vertex") {
            double x = 0.0;
            double y = 0.0;
            double z = 0.0;
            if (!(in >> x >> y >> z)) {
                if (error)
                    *error = "Malformed ASCII STL vertex record.";
                return false;
            }
            if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) {
                if (error)
                    *error = "ASCII STL contains a non-finite vertex coordinate.";
                return false;
            }
            pending.emplace_back(x, y, z);
            if (pending.size() == 3) {
                triangles.push_back({pending[0], pending[1], pending[2]});
                pending.clear();
            }
        }
    }

    if (triangles.empty()) {
        if (error)
            *error = "No triangles found in ASCII STL.";
        return false;
    }
    if (!pending.empty()) {
        if (error)
            *error = "ASCII STL ended with an incomplete triangle.";
        return false;
    }
    return true;
}

void mergeDuplicateTriangleVertices(
    const std::vector<std::array<Vec3, 3>>& triangles, Mesh& mesh, double mergeRelativeEpsilon
) {
    mesh.vertices.clear();
    mesh.faces.clear();

    Vec3 lo(
        std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity()
    );
    Vec3 hi(
        -std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity()
    );

    for (const auto& tri : triangles) {
        for (const Vec3& p : tri) {
            lo = lo.cwiseMin(p);
            hi = hi.cwiseMax(p);
        }
    }

    const double diag = (hi - lo).norm();
    const double eps = std::max({diag * mergeRelativeEpsilon, 1e-12});

    std::unordered_map<QuantizedKey, int, QuantizedKeyHash> indexOf;
    indexOf.reserve(triangles.size() * 3);

    auto addVertex = [&](const Vec3& p) {
        const QuantizedKey key = makeKey(p, eps);
        auto it = indexOf.find(key);
        if (it != indexOf.end()) {
            return it->second;
        }
        const int id = static_cast<int>(mesh.vertices.size());
        mesh.vertices.push_back(p);
        indexOf.emplace(key, id);
        return id;
    };

    for (const auto& tri : triangles) {
        Face face;
        face.v = {addVertex(tri[0]), addVertex(tri[1]), addVertex(tri[2])};
        if (face.v[0] != face.v[1] && face.v[1] != face.v[2] && face.v[0] != face.v[2]) {
            mesh.faces.push_back(face);
        }
    }
}

int parseObjIndex(const std::string& indexText, int valueCount) {
    if (indexText.empty()) {
        return -1;
    }
    int raw = 0;
    try {
        raw = std::stoi(indexText);
    } catch (const std::exception&) {
        return -1;
    }
    if (raw > 0) {
        return raw - 1;
    }
    if (raw < 0) {
        return valueCount + raw;
    }
    return -1;
}

struct ObjCorner {
    int vertex = -1;
    int texcoord = -1;
};

bool parseObjCorner(const std::string& token, int vertexCount, int texcoordCount, ObjCorner& corner) {
    const std::size_t firstSlash = token.find('/');
    const std::string vertexText = firstSlash == std::string::npos ? token : token.substr(0, firstSlash);
    corner.vertex = parseObjIndex(vertexText, vertexCount);
    if (corner.vertex < 0 || corner.vertex >= vertexCount) {
        return false;
    }
    if (firstSlash == std::string::npos) {
        return true;
    }
    const std::size_t secondSlash = token.find('/', firstSlash + 1);
    const std::string texcoordText = secondSlash == std::string::npos
                                         ? token.substr(firstSlash + 1)
                                         : token.substr(firstSlash + 1, secondSlash - firstSlash - 1);
    if (texcoordText.empty()) {
        return true;
    }
    corner.texcoord = parseObjIndex(texcoordText, texcoordCount);
    return corner.texcoord >= 0 && corner.texcoord < texcoordCount;
}

} // namespace

bool loadStl(const std::string& path, Mesh& mesh, std::string* error, double mergeRelativeEpsilon) {
    if (!std::isfinite(mergeRelativeEpsilon) || mergeRelativeEpsilon < 0.0) {
        if (error)
            *error = "mergeRelativeEpsilon must be finite and non-negative.";
        return false;
    }
    std::vector<std::array<Vec3, 3>> triangles;
    std::string localError;
    if (!readBinaryTriangles(path, triangles, &localError)) {
        triangles.clear();
        if (!readAsciiTriangles(path, triangles, &localError)) {
            if (error)
                *error = localError;
            return false;
        }
    }

    mergeDuplicateTriangleVertices(triangles, mesh, mergeRelativeEpsilon);
    mesh.removeUnusedVertices();
    if (mesh.empty()) {
        if (error) {
            *error = "STL contains no non-degenerate triangles after vertex merging.";
        }
        return false;
    }
    return true;
}

bool loadObj(const std::string& path, Mesh& mesh, std::string* error) {
    std::ifstream in(path);
    if (!in) {
        if (error)
            *error = "Failed to open OBJ.";
        return false;
    }

    std::vector<Vec3> positions;
    std::vector<Vec2> texcoords;
    mesh.vertices.clear();
    mesh.faces.clear();
    mesh.faceTexCoords.clear();
    bool sawTextureReference = false;

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }
        std::istringstream ss(line);
        std::string tag;
        ss >> tag;
        if (tag == "v") {
            double x = 0.0;
            double y = 0.0;
            double z = 0.0;
            if (ss >> x >> y >> z) {
                if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) {
                    if (error)
                        *error = "OBJ contains a non-finite vertex coordinate.";
                    return false;
                }
                positions.emplace_back(x, y, z);
            }
        } else if (tag == "vt") {
            double u = 0.0;
            double v = 0.0;
            if (!(ss >> u >> v) || !std::isfinite(u) || !std::isfinite(v)) {
                if (error)
                    *error = "OBJ contains a malformed or non-finite texture coordinate.";
                return false;
            }
            texcoords.emplace_back(u, v);
        } else if (tag == "f") {
            std::vector<ObjCorner> corners;
            std::string token;
            while (ss >> token) {
                ObjCorner corner;
                if (!parseObjCorner(
                        token, static_cast<int>(positions.size()), static_cast<int>(texcoords.size()), corner
                    )) {
                    if (error)
                        *error = "OBJ face references an invalid vertex or texture-coordinate index.";
                    return false;
                }
                sawTextureReference = sawTextureReference || corner.texcoord >= 0;
                corners.push_back(corner);
            }
            for (int i = 1; i + 1 < static_cast<int>(corners.size()); ++i) {
                Face face;
                face.v = {corners[0].vertex, corners[i].vertex, corners[i + 1].vertex};
                if (face.v[0] != face.v[1] && face.v[1] != face.v[2] && face.v[0] != face.v[2]) {
                    mesh.faces.push_back(face);
                    FaceTexCoords faceUv;
                    const std::array<int, 3> textureIds{
                        corners[0].texcoord, corners[i].texcoord, corners[i + 1].texcoord
                    };
                    const bool allTextured = textureIds[0] >= 0 && textureIds[1] >= 0 && textureIds[2] >= 0;
                    const bool noneTextured = textureIds[0] < 0 && textureIds[1] < 0 && textureIds[2] < 0;
                    if (!allTextured && !noneTextured) {
                        if (error)
                            *error = "OBJ face mixes textured and untextured corners.";
                        return false;
                    }
                    faceUv.valid = allTextured;
                    if (allTextured) {
                        for (int corner = 0; corner < 3; ++corner) {
                            faceUv.uv[corner] = texcoords[textureIds[corner]];
                        }
                    }
                    mesh.faceTexCoords.push_back(faceUv);
                }
            }
        }
    }

    mesh.vertices = std::move(positions);
    if (!sawTextureReference) {
        mesh.faceTexCoords.clear();
    }
    mesh.removeUnusedVertices();
    if (mesh.empty()) {
        if (error)
            *error = "No triangles found in OBJ.";
        return false;
    }
    return true;
}

bool loadMesh(const std::string& path, Mesh& mesh, std::string* error, double mergeRelativeEpsilon) {
    std::string extension = std::filesystem::path(path).extension().string();
    for (char& ch : extension) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }

    if (extension == ".stl") {
        return loadStl(path, mesh, error, mergeRelativeEpsilon);
    }
    if (extension == ".obj") {
        return loadObj(path, mesh, error);
    }
    if (error) {
        *error = "Unsupported mesh extension. Use .stl or .obj.";
    }
    return false;
}

bool saveAsciiStl(const std::string& path, const Mesh& mesh, const std::string& solidName, std::string* error) {
    if (!validateMeshGeometry(mesh, error)) {
        return false;
    }
    const std::filesystem::path outputPath(path);
    if (outputPath.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(outputPath.parent_path(), ec);
        if (ec) {
            if (error)
                *error = "Failed to create output directory: " + ec.message();
            return false;
        }
    }
    std::ofstream out(path);
    if (!out) {
        if (error)
            *error = "Failed to open output STL.";
        return false;
    }

    out << std::setprecision(17);
    out << "solid " << solidName << "\n";
    for (const Face& face : mesh.faces) {
        const Vec3& a = mesh.vertices[face.v[0]];
        const Vec3& b = mesh.vertices[face.v[1]];
        const Vec3& c = mesh.vertices[face.v[2]];
        const Vec3 n = triangleNormal(a, b, c);
        out << "  facet normal " << n.x() << " " << n.y() << " " << n.z() << "\n";
        out << "    outer loop\n";
        out << "      vertex " << a.x() << " " << a.y() << " " << a.z() << "\n";
        out << "      vertex " << b.x() << " " << b.y() << " " << b.z() << "\n";
        out << "      vertex " << c.x() << " " << c.y() << " " << c.z() << "\n";
        out << "    endloop\n";
        out << "  endfacet\n";
    }
    out << "endsolid " << solidName << "\n";
    return true;
}

} // namespace manumesh
