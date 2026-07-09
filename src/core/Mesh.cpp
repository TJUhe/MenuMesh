#include "core/Mesh.h"

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
#include <stdexcept>
#include <system_error>
#include <unordered_map>
#include <unordered_set>

namespace manumesh {
namespace {

struct QuantizedKey {
  long long x = 0;
  long long y = 0;
  long long z = 0;

  bool operator==(const QuantizedKey& other) const {
    return x == other.x && y == other.y && z == other.z;
  }
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
  return {static_cast<long long>(std::llround(p.x() / eps)),
          static_cast<long long>(std::llround(p.y() / eps)),
          static_cast<long long>(std::llround(p.z() / eps))};
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
  const auto expectedSize = static_cast<std::uintmax_t>(84) +
                            static_cast<std::uintmax_t>(triangleCount) * 50u;
  return fileSize == expectedSize;
}

bool finitePoint(const Vec3& p) {
  return std::isfinite(p.x()) && std::isfinite(p.y()) && std::isfinite(p.z());
}

bool readBinaryTriangles(const std::string& path,
                         std::vector<std::array<Vec3, 3>>& triangles,
                         std::string* error) {
  uint32_t triangleCount = 0;
  if (!isBinaryStl(path, triangleCount)) {
    return false;
  }

  std::ifstream in(path, std::ios::binary);
  if (!in) {
    if (error) *error = "Failed to open binary STL.";
    return false;
  }

  in.seekg(84, std::ios::beg);
  triangles.clear();
  triangles.reserve(triangleCount);

  std::array<char, 50> record{};
  for (uint32_t i = 0; i < triangleCount; ++i) {
    in.read(record.data(), static_cast<std::streamsize>(record.size()));
    if (!in) {
      if (error) *error = "Unexpected end of binary STL.";
      return false;
    }

    std::array<Vec3, 3> tri{};
    for (int k = 0; k < 3; ++k) {
      const int offset = 12 + k * 12;
      tri[k] = Vec3(readFloatLE(record.data() + offset + 0),
                    readFloatLE(record.data() + offset + 4),
                    readFloatLE(record.data() + offset + 8));
      if (!finitePoint(tri[k])) {
        if (error) *error = "Binary STL contains a non-finite vertex coordinate.";
        return false;
      }
    }
    triangles.push_back(tri);
  }

  return true;
}

bool readAsciiTriangles(const std::string& path,
                        std::vector<std::array<Vec3, 3>>& triangles,
                        std::string* error) {
  std::ifstream in(path);
  if (!in) {
    if (error) *error = "Failed to open ASCII STL.";
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
        if (error) *error = "Malformed ASCII STL vertex record.";
        return false;
      }
      if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) {
        if (error) *error = "ASCII STL contains a non-finite vertex coordinate.";
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
    if (error) *error = "No triangles found in ASCII STL.";
    return false;
  }
  if (!pending.empty()) {
    if (error) *error = "ASCII STL ended with an incomplete triangle.";
    return false;
  }
  return true;
}

void mergeDuplicateTriangleVertices(const std::vector<std::array<Vec3, 3>>& triangles,
                                    Mesh& mesh, double mergeRelativeEpsilon) {
  mesh.vertices.clear();
  mesh.faces.clear();

  Vec3 lo(std::numeric_limits<double>::infinity(),
          std::numeric_limits<double>::infinity(),
          std::numeric_limits<double>::infinity());
  Vec3 hi(-std::numeric_limits<double>::infinity(),
          -std::numeric_limits<double>::infinity(),
          -std::numeric_limits<double>::infinity());

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

struct FaceKey {
  std::array<int, 3> v{};

  bool operator==(const FaceKey& other) const { return v == other.v; }
};

struct FaceKeyHash {
  std::size_t operator()(const FaceKey& key) const {
    return static_cast<std::size_t>(key.v[0]) * 73856093u ^
           static_cast<std::size_t>(key.v[1]) * 19349663u ^
           static_cast<std::size_t>(key.v[2]) * 83492791u;
  }
};

} // namespace

bool Mesh::empty() const {
  return vertices.empty() || faces.empty();
}

Vec3 Mesh::bboxMin() const {
  Vec3 lo(std::numeric_limits<double>::infinity(),
          std::numeric_limits<double>::infinity(),
          std::numeric_limits<double>::infinity());
  for (const Vec3& p : vertices) {
    lo = lo.cwiseMin(p);
  }
  return lo;
}

Vec3 Mesh::bboxMax() const {
  Vec3 hi(-std::numeric_limits<double>::infinity(),
          -std::numeric_limits<double>::infinity(),
          -std::numeric_limits<double>::infinity());
  for (const Vec3& p : vertices) {
    hi = hi.cwiseMax(p);
  }
  return hi;
}

double Mesh::bboxDiag() const {
  if (vertices.empty()) {
    return 0.0;
  }
  return (bboxMax() - bboxMin()).norm();
}

void Mesh::removeUnusedVertices() {
  std::vector<int> remap(vertices.size(), -1);
  std::vector<Vec3> newVertices;
  newVertices.reserve(vertices.size());
  std::vector<char> validFace(faces.size(), 1);

  for (std::size_t faceIndex = 0; faceIndex < faces.size(); ++faceIndex) {
    const Face& face = faces[faceIndex];
    for (int id : face.v) {
      if (id < 0 || id >= static_cast<int>(vertices.size())) {
        validFace[faceIndex] = 0;
        continue;
      }
      if (remap[id] < 0) {
        remap[id] = static_cast<int>(newVertices.size());
        newVertices.push_back(vertices[id]);
      }
    }
  }

  for (std::size_t faceIndex = 0; faceIndex < faces.size(); ++faceIndex) {
    if (!validFace[faceIndex]) {
      continue;
    }
    Face& face = faces[faceIndex];
    for (int& id : face.v) {
      id = remap[id];
    }
  }
  faces.erase(std::remove_if(faces.begin(), faces.end(),
                             [&](const Face& face) {
                               for (int id : face.v) {
                                 if (id < 0 || id >= static_cast<int>(remap.size())) {
                                   return true;
                                 }
                               }
                               return false;
                             }),
              faces.end());
  vertices.swap(newVertices);
}

bool loadStl(const std::string& path, Mesh& mesh, std::string* error,
             double mergeRelativeEpsilon) {
  if (!std::isfinite(mergeRelativeEpsilon) || mergeRelativeEpsilon < 0.0) {
    if (error) *error = "mergeRelativeEpsilon must be finite and non-negative.";
    return false;
  }
  std::vector<std::array<Vec3, 3>> triangles;
  std::string localError;
  if (!readBinaryTriangles(path, triangles, &localError)) {
    triangles.clear();
    if (!readAsciiTriangles(path, triangles, &localError)) {
      if (error) *error = localError;
      return false;
    }
  }

  mergeDuplicateTriangleVertices(triangles, mesh, mergeRelativeEpsilon);
  mesh.removeUnusedVertices();
  return !mesh.empty();
}

int parseObjVertexIndex(const std::string& token, int vertexCount) {
  const std::size_t slash = token.find('/');
  const std::string indexText =
      slash == std::string::npos ? token : token.substr(0, slash);
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
    return vertexCount + raw;
  }
  return -1;
}

bool loadObj(const std::string& path, Mesh& mesh, std::string* error) {
  std::ifstream in(path);
  if (!in) {
    if (error) *error = "Failed to open OBJ.";
    return false;
  }

  std::vector<Vec3> positions;
  mesh.vertices.clear();
  mesh.faces.clear();

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
          if (error) *error = "OBJ contains a non-finite vertex coordinate.";
          return false;
        }
        positions.emplace_back(x, y, z);
      }
    } else if (tag == "f") {
      std::vector<int> ids;
      std::string token;
      while (ss >> token) {
        const int id = parseObjVertexIndex(token, static_cast<int>(positions.size()));
        if (id < 0 || id >= static_cast<int>(positions.size())) {
          if (error) *error = "OBJ face references an invalid vertex index.";
          return false;
        }
        ids.push_back(id);
      }
      for (int i = 1; i + 1 < static_cast<int>(ids.size()); ++i) {
        Face face;
        face.v = {ids[0], ids[i], ids[i + 1]};
        if (face.v[0] != face.v[1] && face.v[1] != face.v[2] &&
            face.v[0] != face.v[2]) {
          mesh.faces.push_back(face);
        }
      }
    }
  }

  mesh.vertices = std::move(positions);
  mesh.removeUnusedVertices();
  if (mesh.empty()) {
    if (error) *error = "No triangles found in OBJ.";
    return false;
  }
  return true;
}

bool loadMesh(const std::string& path, Mesh& mesh, std::string* error,
              double mergeRelativeEpsilon) {
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

bool validateMeshIndices(const Mesh& mesh, std::string* error) {
  if (mesh.vertices.size() >
      static_cast<std::size_t>(std::numeric_limits<int>::max())) {
    if (error) *error = "Vertex count exceeds the supported int-index range.";
    return false;
  }
  for (std::size_t faceIndex = 0; faceIndex < mesh.faces.size(); ++faceIndex) {
    const Face& face = mesh.faces[faceIndex];
    for (int id : face.v) {
      if (id < 0 || id >= static_cast<int>(mesh.vertices.size())) {
        if (error) {
          *error = "Mesh face " + std::to_string(faceIndex) +
                   " references an invalid vertex index.";
        }
        return false;
      }
    }
  }
  return true;
}

bool validateMeshGeometry(const Mesh& mesh, std::string* error) {
  if (!validateMeshIndices(mesh, error)) {
    return false;
  }
  for (std::size_t vertexIndex = 0; vertexIndex < mesh.vertices.size(); ++vertexIndex) {
    if (!finitePoint(mesh.vertices[vertexIndex])) {
      if (error) {
        *error = "Mesh vertex " + std::to_string(vertexIndex) +
                 " contains a non-finite coordinate.";
      }
      return false;
    }
  }
  for (std::size_t faceIndex = 0; faceIndex < mesh.faces.size(); ++faceIndex) {
    const Face& face = mesh.faces[faceIndex];
    if (face.v[0] == face.v[1] || face.v[1] == face.v[2] || face.v[0] == face.v[2]) {
      if (error) {
        *error = "Mesh face " + std::to_string(faceIndex) + " is degenerate.";
      }
      return false;
    }
    const Vec3& a = mesh.vertices[face.v[0]];
    const Vec3& b = mesh.vertices[face.v[1]];
    const Vec3& c = mesh.vertices[face.v[2]];
    if ((b - a).cross(c - a).squaredNorm() <= 0.0) {
      if (error) {
        *error = "Mesh face " + std::to_string(faceIndex) + " has zero area.";
      }
      return false;
    }
  }
  return true;
}

bool saveAsciiStl(const std::string& path, const Mesh& mesh,
                  const std::string& solidName, std::string* error) {
  if (!validateMeshGeometry(mesh, error)) {
    return false;
  }
  const std::filesystem::path outputPath(path);
  if (outputPath.has_parent_path()) {
    std::error_code ec;
    std::filesystem::create_directories(outputPath.parent_path(), ec);
    if (ec) {
      if (error) *error = "Failed to create output directory: " + ec.message();
      return false;
    }
  }
  std::ofstream out(path);
  if (!out) {
    if (error) *error = "Failed to open output STL.";
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

double triangleArea(const Vec3& a, const Vec3& b, const Vec3& c) {
  return 0.5 * (b - a).cross(c - a).norm();
}

Vec3 triangleNormal(const Vec3& a, const Vec3& b, const Vec3& c) {
  Vec3 n = (b - a).cross(c - a);
  const double len = n.norm();
  if (len <= 1e-30) {
    return Vec3(0.0, 0.0, 0.0);
  }
  return n / len;
}

std::vector<std::pair<int, int>> uniqueEdges(const Mesh& mesh) {
  std::unordered_set<std::uint64_t> seen;
  std::vector<std::pair<int, int>> edges;
  edges.reserve(mesh.faces.size() * 3 / 2);

  auto pack = [](int a, int b) {
    if (a > b) std::swap(a, b);
    return (static_cast<std::uint64_t>(static_cast<uint32_t>(a)) << 32u) |
           static_cast<uint32_t>(b);
  };

  for (const Face& face : mesh.faces) {
    for (int e = 0; e < 3; ++e) {
      int a = face.v[e];
      int b = face.v[(e + 1) % 3];
      if (a == b) continue;
      const auto key = pack(a, b);
      if (seen.insert(key).second) {
        if (a > b) std::swap(a, b);
        edges.emplace_back(a, b);
      }
    }
  }
  return edges;
}

} // namespace manumesh
