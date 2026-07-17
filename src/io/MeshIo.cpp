/**
 * @file src/io/MeshIo.cpp
 * @brief Implements mesh io facilities for ManuMesh's mesh-I/O module.
 * @ingroup manumesh_io
 *
 * @details Implements deterministic STL/OBJ parsing and ASCII/binary STL serialization.
 * @algorithm Binary STL is recognized by record layout, while ASCII STL uses
 * token parsing. Coincident STL positions are merged with scale-aware quantization. OBJ
 * polygons are projected to their dominant plane; convex faces use stable fan
 * triangulation and concave faces use validated ear clipping.
 * @failuremodes Repeated, degenerate, self-intersecting, non-finite, or
 * truncated input is rejected with a diagnostic rather than partially emitted.
 */

#include "io/MeshIo.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <string>
#include <system_error>
#include <unordered_map>
#include <vector>

namespace manumesh {
namespace {

std::filesystem::path pathFromUtf8(const std::string& path) { return std::filesystem::u8path(path); }

/** @brief Quantized three-dimensional position used for STL vertex welding. */
struct QuantizedKey {
    long long x = 0;
    long long y = 0;
    long long z = 0;

    bool operator==(const QuantizedKey& other) const { return x == other.x && y == other.y && z == other.z; }
};

/** @brief Stable hash for quantized STL vertex positions. */
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

long long quantizeCoordinate(double value, double eps) {
    const double scaled = value / eps;
    // std::llround is undefined once the argument leaves the long long range,
    // so clamp huge quotients to the representable extremes first.
    constexpr double kMaxQuantized = 9.0e18; // safely below LLONG_MAX (~9.22e18)
    if (scaled >= kMaxQuantized) {
        return std::numeric_limits<long long>::max();
    }
    if (scaled <= -kMaxQuantized) {
        return std::numeric_limits<long long>::min();
    }
    return std::llround(scaled);
}

QuantizedKey makeKey(const Vec3& p, double eps) {
    return {quantizeCoordinate(p.x(), eps), quantizeCoordinate(p.y(), eps), quantizeCoordinate(p.z(), eps)};
}

bool offsetQuantizedCoordinate(long long value, int offset, long long& result) {
    if ((offset < 0 && value == std::numeric_limits<long long>::min()) ||
        (offset > 0 && value == std::numeric_limits<long long>::max())) {
        return false;
    }
    result = value + offset;
    return true;
}

uint32_t readUint32LE(const char* bytes) {
    uint32_t value = 0;
    std::memcpy(&value, bytes, sizeof(uint32_t));
    return value;
}

void writeUint32LE(char* bytes, uint32_t value) {
    bytes[0] = static_cast<char>(value & 0xffu);
    bytes[1] = static_cast<char>((value >> 8u) & 0xffu);
    bytes[2] = static_cast<char>((value >> 16u) & 0xffu);
    bytes[3] = static_cast<char>((value >> 24u) & 0xffu);
}

void writeFloatLE(char* bytes, float value) {
    uint32_t bits = 0;
    static_assert(std::numeric_limits<float>::is_iec559, "Binary STL requires IEEE-754 floats.");
    static_assert(sizeof(bits) == sizeof(value), "Binary STL requires 32-bit IEEE-754 floats.");
    std::memcpy(&bits, &value, sizeof(bits));
    writeUint32LE(bytes, bits);
}

float readFloatLE(const char* bytes) {
    float value = 0.0f;
    std::memcpy(&value, bytes, sizeof(float));
    return value;
}

bool finitePoint(const Vec3& p) { return std::isfinite(p.x()) && std::isfinite(p.y()) && std::isfinite(p.z()); }

bool isAsciiSpace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f'; }

const char* skipSpaces(const char* p, const char* end) {
    while (p != end && isAsciiSpace(*p)) {
        ++p;
    }
    return p;
}

const char* findTokenEnd(const char* p, const char* end) {
    while (p != end && !isAsciiSpace(*p)) {
        ++p;
    }
    return p;
}

/**
 * @brief Parses a double at `p`, tolerating an explicit leading '+'. Returns the
 * first unconsumed character, or nullptr when no double could be parsed.
 */
const char* parseDoubleAt(const char* p, const char* end, double& value) {
    if (p != end && *p == '+') {
        ++p;
        if (p != end && (*p == '+' || *p == '-')) {
            return nullptr;
        }
    }
    const std::from_chars_result result = std::from_chars(p, end, value);
    if (result.ec != std::errc()) {
        return nullptr;
    }
    return result.ptr;
}

/**
 * @brief Reads the whole file into `text`. Returns false when the file cannot be
 * opened or read completely.
 */
bool readFileToString(const std::string& path, std::string& text) {
    std::ifstream in(pathFromUtf8(path), std::ios::binary);
    if (!in) {
        return false;
    }
    in.seekg(0, std::ios::end);
    const std::streamoff size = in.tellg();
    if (size < 0) {
        return false;
    }
    in.seekg(0, std::ios::beg);
    text.resize(static_cast<std::size_t>(size));
    if (size > 0) {
        in.read(&text[0], static_cast<std::streamsize>(size));
        if (in.gcount() != static_cast<std::streamsize>(size)) {
            return false;
        }
    }
    return true;
}

enum class StlFormat { Binary, Ascii, Invalid };

/**
 * @brief Decides whether an STL file is binary or ASCII.
 *
 * A file whose size matches the exact binary layout (84 + 50 * n bytes) is
 * binary even when its header starts with "solid". A file that does not
 * start with "solid" is treated as binary as well; trailing padding bytes
 * after the last record are tolerated, while a shorter file is reported as a
 * truncated binary STL instead of falling through to the ASCII parser.
 */
StlFormat probeStlFormat(const std::string& path, uint32_t& triangleCount, std::string* error) {
    const std::filesystem::path inputPath = pathFromUtf8(path);
    std::ifstream in(inputPath, std::ios::binary);
    if (!in) {
        if (error)
            *error = "Failed to open STL.";
        return StlFormat::Invalid;
    }

    std::array<char, 84> header{};
    in.read(header.data(), static_cast<std::streamsize>(header.size()));
    const std::size_t headerBytes = static_cast<std::size_t>(in.gcount());

    std::size_t offset = 0;
    if (headerBytes >= 3 && std::memcmp(header.data(), "\xEF\xBB\xBF", 3) == 0) {
        offset = 3; // skip a UTF-8 byte-order mark
    }
    while (offset < headerBytes && isAsciiSpace(header[offset])) {
        ++offset;
    }
    bool startsWithSolid = headerBytes - offset >= 5;
    if (startsWithSolid) {
        const char expected[] = {'s', 'o', 'l', 'i', 'd'};
        for (std::size_t i = 0; i < sizeof(expected); ++i) {
            const unsigned char ch = static_cast<unsigned char>(header[offset + i]);
            if (static_cast<char>(std::tolower(ch)) != expected[i]) {
                startsWithSolid = false;
                break;
            }
        }
    }

    if (headerBytes < header.size()) {
        if (startsWithSolid) {
            return StlFormat::Ascii;
        }
        if (error)
            *error = "STL file is too small to hold a binary STL header.";
        return StlFormat::Invalid;
    }

    triangleCount = readUint32LE(header.data() + 80);
    std::error_code ec;
    const std::uintmax_t fileSize = std::filesystem::file_size(inputPath, ec);
    if (ec) {
        if (startsWithSolid) {
            return StlFormat::Ascii;
        }
        if (error)
            *error = "Failed to determine binary STL file size.";
        return StlFormat::Invalid;
    }

    const std::uintmax_t expectedSize =
        static_cast<std::uintmax_t>(84) + static_cast<std::uintmax_t>(triangleCount) * 50u;
    if (fileSize == expectedSize) {
        return StlFormat::Binary;
    }
    if (startsWithSolid) {
        return StlFormat::Ascii;
    }
    if (fileSize > expectedSize) {
        // Some exporters append padding after the last record; ignore it.
        return StlFormat::Binary;
    }
    if (error) {
        *error = "Truncated binary STL: the header declares " + std::to_string(triangleCount) +
                 " triangles but the file is smaller than the required " + std::to_string(expectedSize) + " bytes.";
    }
    return StlFormat::Invalid;
}

bool readBinaryTriangles(
    const std::string& path, uint32_t triangleCount, std::vector<std::array<Vec3, 3>>& triangles, std::string* error
) {
    std::ifstream in(pathFromUtf8(path), std::ios::binary);
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
    std::string text;
    if (!readFileToString(path, text)) {
        if (error)
            *error = "Failed to open ASCII STL.";
        return false;
    }

    triangles.clear();
    std::array<Vec3, 3> pending{};
    int pendingCount = 0;

    const char* p = text.data();
    const char* const end = p + text.size();
    while (true) {
        p = skipSpaces(p, end);
        if (p == end) {
            break;
        }
        const char* tokenEnd = findTokenEnd(p, end);
        if (tokenEnd - p == 6 && std::memcmp(p, "vertex", 6) == 0) {
            p = tokenEnd;
            double coords[3] = {0.0, 0.0, 0.0};
            for (double& coord : coords) {
                p = skipSpaces(p, end);
                const char* next = parseDoubleAt(p, end, coord);
                if (next == nullptr) {
                    if (error)
                        *error = "Malformed ASCII STL vertex record.";
                    return false;
                }
                p = next;
            }
            if (!std::isfinite(coords[0]) || !std::isfinite(coords[1]) || !std::isfinite(coords[2])) {
                if (error)
                    *error = "ASCII STL contains a non-finite vertex coordinate.";
                return false;
            }
            pending[static_cast<std::size_t>(pendingCount)] = Vec3(coords[0], coords[1], coords[2]);
            ++pendingCount;
            if (pendingCount == 3) {
                triangles.push_back(pending);
                pendingCount = 0;
            }
        } else {
            p = tokenEnd;
        }
    }

    if (triangles.empty()) {
        if (error)
            *error = "No triangles found in ASCII STL.";
        return false;
    }
    if (pendingCount != 0) {
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

    auto scaledSpan = [&](double low, double high) {
        const double span = high - low;
        if (std::isfinite(span)) {
            return std::abs(span * mergeRelativeEpsilon);
        }
        // Scale before subtracting when the finite endpoints span more than
        // DBL_MAX. This keeps the usual small relative epsilon representable.
        return std::abs(high * mergeRelativeEpsilon - low * mergeRelativeEpsilon);
    };
    double relativeEps = 0.0;
    if (mergeRelativeEpsilon > 0.0) {
        relativeEps = std::hypot(scaledSpan(lo.x(), hi.x()), scaledSpan(lo.y(), hi.y()), scaledSpan(lo.z(), hi.z()));
        if (!std::isfinite(relativeEps)) {
            relativeEps = std::numeric_limits<double>::max();
        }
    }
    const double eps = std::max(relativeEps, 1e-12);

    std::unordered_map<QuantizedKey, std::vector<int>, QuantizedKeyHash> indicesByKey;
    indicesByKey.reserve(triangles.size() * 3);

    auto addVertex = [&](const Vec3& p) {
        // Quantize relative to a stable local origin so translating a mesh far
        // from zero cannot saturate every coordinate into the same key.
        const QuantizedKey key = makeKey(p - lo, eps);
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dz = -1; dz <= 1; ++dz) {
                    QuantizedKey neighbor;
                    if (!offsetQuantizedCoordinate(key.x, dx, neighbor.x) ||
                        !offsetQuantizedCoordinate(key.y, dy, neighbor.y) ||
                        !offsetQuantizedCoordinate(key.z, dz, neighbor.z)) {
                        continue;
                    }
                    const auto bucket = indicesByKey.find(neighbor);
                    if (bucket == indicesByKey.end()) {
                        continue;
                    }
                    for (int candidate : bucket->second) {
                        const Vec3& existing = mesh.vertices[static_cast<std::size_t>(candidate)];
                        if ((existing - p).norm() <= eps) {
                            return candidate;
                        }
                    }
                }
            }
        }
        const int id = static_cast<int>(mesh.vertices.size());
        mesh.vertices.push_back(p);
        indicesByKey[key].push_back(id);
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

std::string objLineError(int lineNumber, const char* message) {
    return "OBJ line " + std::to_string(lineNumber) + ": " + message;
}

/**
 * @brief Parses one OBJ index segment (for example the "3" in "3/1/2"), resolving
 * negative indices relative to `valueCount`. The whole segment must be a
 * valid integer and the resolved zero-based index must be in range.
 */
bool parseObjIndexText(const char* begin, const char* end, int valueCount, int& indexOut) {
    if (begin != end && *begin == '+') {
        ++begin;
        if (begin != end && (*begin == '+' || *begin == '-')) {
            return false;
        }
    }
    int raw = 0;
    const std::from_chars_result result = std::from_chars(begin, end, raw);
    if (result.ec != std::errc() || result.ptr != end || raw == 0) {
        return false;
    }
    const int index = raw > 0 ? raw - 1 : valueCount + raw;
    if (index < 0 || index >= valueCount) {
        return false;
    }
    indexOut = index;
    return true;
}

/** @brief Resolved vertex and optional texture-coordinate indices of one OBJ corner. */
struct ObjCorner {
    int vertex = -1;
    int texcoord = -1;
};

using ObjTriangle = std::array<int, 3>;

constexpr double kObjPolygonEpsilon = 1e-12;

double orient2d(const Vec2& a, const Vec2& b, const Vec2& c) {
    const Vec2 ab = b - a;
    const Vec2 ac = c - a;
    return ab.x() * ac.y() - ab.y() * ac.x();
}

bool pointOnSegment2d(const Vec2& point, const Vec2& a, const Vec2& b) {
    if (std::abs(orient2d(a, b, point)) > kObjPolygonEpsilon) {
        return false;
    }
    return point.x() >= std::min(a.x(), b.x()) - kObjPolygonEpsilon &&
           point.x() <= std::max(a.x(), b.x()) + kObjPolygonEpsilon &&
           point.y() >= std::min(a.y(), b.y()) - kObjPolygonEpsilon &&
           point.y() <= std::max(a.y(), b.y()) + kObjPolygonEpsilon;
}

int orientationSign(double value) {
    if (value > kObjPolygonEpsilon) {
        return 1;
    }
    if (value < -kObjPolygonEpsilon) {
        return -1;
    }
    return 0;
}

bool segmentsIntersect2d(const Vec2& a, const Vec2& b, const Vec2& c, const Vec2& d) {
    if (std::max(a.x(), b.x()) + kObjPolygonEpsilon < std::min(c.x(), d.x()) ||
        std::max(c.x(), d.x()) + kObjPolygonEpsilon < std::min(a.x(), b.x()) ||
        std::max(a.y(), b.y()) + kObjPolygonEpsilon < std::min(c.y(), d.y()) ||
        std::max(c.y(), d.y()) + kObjPolygonEpsilon < std::min(a.y(), b.y())) {
        return false;
    }

    const int abc = orientationSign(orient2d(a, b, c));
    const int abd = orientationSign(orient2d(a, b, d));
    const int cda = orientationSign(orient2d(c, d, a));
    const int cdb = orientationSign(orient2d(c, d, b));
    if (abc * abd < 0 && cda * cdb < 0) {
        return true;
    }
    return (abc == 0 && pointOnSegment2d(c, a, b)) || (abd == 0 && pointOnSegment2d(d, a, b)) ||
           (cda == 0 && pointOnSegment2d(a, c, d)) || (cdb == 0 && pointOnSegment2d(b, c, d));
}

bool polygonSelfIntersects(const std::vector<Vec2>& polygon) {
    const int size = static_cast<int>(polygon.size());
    for (int first = 0; first < size; ++first) {
        const int firstNext = (first + 1) % size;
        for (int second = first + 1; second < size; ++second) {
            const int secondNext = (second + 1) % size;
            if (first == second || firstNext == second || secondNext == first) {
                continue;
            }
            if (segmentsIntersect2d(
                    polygon[static_cast<std::size_t>(first)],
                    polygon[static_cast<std::size_t>(firstNext)],
                    polygon[static_cast<std::size_t>(second)],
                    polygon[static_cast<std::size_t>(secondNext)]
                )) {
                return true;
            }
        }
    }
    return false;
}

bool pointInOrOnTriangle2d(const Vec2& point, const Vec2& a, const Vec2& b, const Vec2& c, int orientation) {
    return orientation * orient2d(a, b, point) >= -kObjPolygonEpsilon &&
           orientation * orient2d(b, c, point) >= -kObjPolygonEpsilon &&
           orientation * orient2d(c, a, point) >= -kObjPolygonEpsilon;
}

bool triangulateObjPolygon(
    const std::vector<ObjCorner>& corners,
    const std::vector<Vec3>& positions,
    std::vector<ObjTriangle>& triangles,
    const char*& failureReason
) {
    triangles.clear();
    if (corners.size() < 3) {
        failureReason = "face has fewer than three corners.";
        return false;
    }

    const Vec3 origin = positions[static_cast<std::size_t>(corners[0].vertex)];
    std::vector<Vec3> normalized;
    normalized.reserve(corners.size());
    double scale = 0.0;
    for (const ObjCorner& corner : corners) {
        const Vec3 delta = positions[static_cast<std::size_t>(corner.vertex)] - origin;
        scale = std::max(scale, delta.cwiseAbs().maxCoeff());
        normalized.push_back(delta);
    }
    if (!std::isfinite(scale) || scale <= 0.0) {
        failureReason = "face polygon is degenerate.";
        return false;
    }
    for (Vec3& point : normalized) {
        point /= scale;
    }
    for (std::size_t first = 0; first < normalized.size(); ++first) {
        for (std::size_t second = first + 1; second < normalized.size(); ++second) {
            if ((normalized[first] - normalized[second]).squaredNorm() <= kObjPolygonEpsilon * kObjPolygonEpsilon) {
                failureReason = "face polygon repeats a corner position.";
                return false;
            }
        }
    }

    Vec3 normal = Vec3::Zero();
    for (std::size_t i = 0; i < normalized.size(); ++i) {
        normal += normalized[i].cross(normalized[(i + 1) % normalized.size()]);
    }
    if (normal.squaredNorm() <= kObjPolygonEpsilon * kObjPolygonEpsilon) {
        for (std::size_t first = 1; first < normalized.size(); ++first) {
            for (std::size_t second = first + 1; second < normalized.size(); ++second) {
                const Vec3 candidate = normalized[first].cross(normalized[second]);
                if (candidate.squaredNorm() > normal.squaredNorm()) {
                    normal = candidate;
                }
            }
        }
        if (normal.squaredNorm() <= kObjPolygonEpsilon * kObjPolygonEpsilon) {
            failureReason = "face polygon is degenerate.";
            return false;
        }
    }

    int dropAxis = 0;
    normal = normal.cwiseAbs();
    if (normal.y() > normal.x()) {
        dropAxis = 1;
    }
    if (normal.z() > normal[dropAxis]) {
        dropAxis = 2;
    }

    std::vector<Vec2> projected;
    projected.reserve(normalized.size());
    for (const Vec3& point : normalized) {
        if (dropAxis == 0) {
            projected.emplace_back(point.y(), point.z());
        } else if (dropAxis == 1) {
            projected.emplace_back(point.x(), point.z());
        } else {
            projected.emplace_back(point.x(), point.y());
        }
    }

    double signedAreaTwice = 0.0;
    for (std::size_t i = 0; i < projected.size(); ++i) {
        const Vec2& a = projected[i];
        const Vec2& b = projected[(i + 1) % projected.size()];
        signedAreaTwice += a.x() * b.y() - a.y() * b.x();
    }
    if (polygonSelfIntersects(projected)) {
        failureReason = "face polygon is self-intersecting.";
        return false;
    }
    if (std::abs(signedAreaTwice) <= kObjPolygonEpsilon) {
        failureReason = "face polygon has zero projected area.";
        return false;
    }
    const int orientation = signedAreaTwice > 0.0 ? 1 : -1;

    bool strictlyConvex = true;
    for (std::size_t i = 0; i < projected.size(); ++i) {
        const Vec2& previous = projected[(i + projected.size() - 1) % projected.size()];
        const Vec2& current = projected[i];
        const Vec2& next = projected[(i + 1) % projected.size()];
        if (orientation * orient2d(previous, current, next) <= kObjPolygonEpsilon) {
            strictlyConvex = false;
            break;
        }
    }
    if (strictlyConvex) {
        triangles.reserve(corners.size() - 2);
        for (int i = 1; i + 1 < static_cast<int>(corners.size()); ++i) {
            triangles.push_back({0, i, i + 1});
        }
        return true;
    }

    std::vector<int> remaining;
    remaining.reserve(corners.size());
    for (int i = 0; i < static_cast<int>(corners.size()); ++i) {
        remaining.push_back(i);
    }
    triangles.reserve(corners.size() - 2);
    while (remaining.size() > 3) {
        bool clippedEar = false;
        for (std::size_t i = 0; i < remaining.size(); ++i) {
            const int previous = remaining[(i + remaining.size() - 1) % remaining.size()];
            const int current = remaining[i];
            const int next = remaining[(i + 1) % remaining.size()];
            const Vec2& a = projected[static_cast<std::size_t>(previous)];
            const Vec2& b = projected[static_cast<std::size_t>(current)];
            const Vec2& c = projected[static_cast<std::size_t>(next)];
            if (orientation * orient2d(a, b, c) <= kObjPolygonEpsilon) {
                continue;
            }

            bool containsOtherCorner = false;
            for (int candidate : remaining) {
                if (candidate == previous || candidate == current || candidate == next) {
                    continue;
                }
                if (pointInOrOnTriangle2d(projected[static_cast<std::size_t>(candidate)], a, b, c, orientation)) {
                    containsOtherCorner = true;
                    break;
                }
            }
            if (containsOtherCorner) {
                continue;
            }

            triangles.push_back({previous, current, next});
            remaining.erase(remaining.begin() + static_cast<std::ptrdiff_t>(i));
            clippedEar = true;
            break;
        }
        if (!clippedEar) {
            failureReason = "face polygon cannot be triangulated without degenerate or overlapping triangles.";
            return false;
        }
    }

    const int a = remaining[0];
    const int b = remaining[1];
    const int c = remaining[2];
    if (orientation * orient2d(
                          projected[static_cast<std::size_t>(a)],
                          projected[static_cast<std::size_t>(b)],
                          projected[static_cast<std::size_t>(c)]
                      ) <=
        kObjPolygonEpsilon) {
        failureReason = "face polygon produces a degenerate final triangle.";
        return false;
    }
    triangles.push_back({a, b, c});
    return true;
}

/**
 * @brief Parses one face corner token: `v`, `v/vt`, `v//vn`, or `v/vt/vn`.
 * The normal index after the second slash is intentionally ignored.
 */
bool parseObjCorner(const char* begin, const char* end, int vertexCount, int texcoordCount, ObjCorner& corner) {
    const char* firstSlash = static_cast<const char*>(std::memchr(begin, '/', static_cast<std::size_t>(end - begin)));
    const char* vertexEnd = firstSlash == nullptr ? end : firstSlash;
    if (!parseObjIndexText(begin, vertexEnd, vertexCount, corner.vertex)) {
        return false;
    }
    if (firstSlash == nullptr) {
        return true;
    }
    const char* texcoordBegin = firstSlash + 1;
    const char* secondSlash =
        static_cast<const char*>(std::memchr(texcoordBegin, '/', static_cast<std::size_t>(end - texcoordBegin)));
    const char* texcoordEnd = secondSlash == nullptr ? end : secondSlash;
    if (texcoordBegin == texcoordEnd) {
        return true;
    }
    return parseObjIndexText(texcoordBegin, texcoordEnd, texcoordCount, corner.texcoord);
}

} // namespace

bool loadStl(const std::string& path, Mesh& mesh, std::string* error, double mergeRelativeEpsilon) {
    if (error) {
        error->clear();
    }
    if (!std::isfinite(mergeRelativeEpsilon) || mergeRelativeEpsilon < 0.0) {
        if (error)
            *error = "mergeRelativeEpsilon must be finite and non-negative.";
        return false;
    }
    // STL carries no texture coordinates; drop any stale ones so they cannot
    // silently align with the freshly loaded faces of a reused mesh.
    mesh.faceTexCoords.clear();

    uint32_t triangleCount = 0;
    std::string localError;
    const StlFormat format = probeStlFormat(path, triangleCount, &localError);

    std::vector<std::array<Vec3, 3>> triangles;
    bool loaded = false;
    if (format == StlFormat::Binary) {
        loaded = readBinaryTriangles(path, triangleCount, triangles, &localError);
    } else if (format == StlFormat::Ascii) {
        loaded = readAsciiTriangles(path, triangles, &localError);
    }
    if (!loaded) {
        if (error)
            *error = localError;
        return false;
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
    if (error) {
        error->clear();
    }
    std::string text;
    if (!readFileToString(path, text)) {
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

    std::vector<ObjCorner> corners;
    std::vector<ObjTriangle> polygonTriangles;
    const char* cursor = text.data();
    const char* const textEnd = cursor + text.size();
    int lineNumber = 0;
    while (cursor < textEnd) {
        ++lineNumber;
        const char* lineEnd =
            static_cast<const char*>(std::memchr(cursor, '\n', static_cast<std::size_t>(textEnd - cursor)));
        const char* const nextLine = lineEnd == nullptr ? textEnd : lineEnd + 1;
        if (lineEnd == nullptr) {
            lineEnd = textEnd;
        }

        const char* p = skipSpaces(cursor, lineEnd);
        cursor = nextLine;
        if (p == lineEnd) {
            continue;
        }
        const char* tagEnd = findTokenEnd(p, lineEnd);
        const std::size_t tagLength = static_cast<std::size_t>(tagEnd - p);

        if (tagLength == 1 && p[0] == 'v') {
            double coords[3] = {0.0, 0.0, 0.0};
            const char* q = tagEnd;
            for (double& coord : coords) {
                q = skipSpaces(q, lineEnd);
                q = parseDoubleAt(q, lineEnd, coord);
                if (q == nullptr) {
                    break;
                }
            }
            if (q == nullptr || !std::isfinite(coords[0]) || !std::isfinite(coords[1]) || !std::isfinite(coords[2])) {
                if (error)
                    *error = objLineError(lineNumber, "malformed or non-finite vertex coordinate.");
                return false;
            }
            positions.emplace_back(coords[0], coords[1], coords[2]);
        } else if (tagLength == 2 && p[0] == 'v' && p[1] == 't') {
            double u = 0.0;
            double v = 0.0;
            const char* q = skipSpaces(tagEnd, lineEnd);
            q = parseDoubleAt(q, lineEnd, u);
            if (q == nullptr || !std::isfinite(u)) {
                if (error)
                    *error = objLineError(lineNumber, "malformed or non-finite texture coordinate.");
                return false;
            }
            // The second component is optional per the OBJ spec ("vt u")
            // and defaults to zero; a third component (w) is ignored.
            q = skipSpaces(q, lineEnd);
            if (q != lineEnd) {
                q = parseDoubleAt(q, lineEnd, v);
                if (q == nullptr || !std::isfinite(v)) {
                    if (error)
                        *error = objLineError(lineNumber, "malformed or non-finite texture coordinate.");
                    return false;
                }
            }
            texcoords.emplace_back(u, v);
        } else if (tagLength == 1 && p[0] == 'f') {
            corners.clear();
            const char* q = tagEnd;
            while (true) {
                q = skipSpaces(q, lineEnd);
                if (q == lineEnd) {
                    break;
                }
                const char* tokenEnd = findTokenEnd(q, lineEnd);
                ObjCorner corner;
                if (!parseObjCorner(
                        q, tokenEnd, static_cast<int>(positions.size()), static_cast<int>(texcoords.size()), corner
                    )) {
                    if (error)
                        *error =
                            objLineError(lineNumber, "face references an invalid vertex or texture-coordinate index.");
                    return false;
                }
                sawTextureReference = sawTextureReference || corner.texcoord >= 0;
                corners.push_back(corner);
                q = tokenEnd;
            }
            const bool allTextured = std::all_of(corners.begin(), corners.end(), [](const ObjCorner& corner) {
                return corner.texcoord >= 0;
            });
            const bool noneTextured = std::all_of(corners.begin(), corners.end(), [](const ObjCorner& corner) {
                return corner.texcoord < 0;
            });
            if (!allTextured && !noneTextured) {
                if (error)
                    *error = objLineError(lineNumber, "face mixes textured and untextured corners.");
                return false;
            }
            const char* triangulationFailure = nullptr;
            if (!triangulateObjPolygon(corners, positions, polygonTriangles, triangulationFailure)) {
                if (error)
                    *error = objLineError(lineNumber, triangulationFailure);
                return false;
            }
            for (const ObjTriangle& triangle : polygonTriangles) {
                Face face;
                face.v = {
                    corners[static_cast<std::size_t>(triangle[0])].vertex,
                    corners[static_cast<std::size_t>(triangle[1])].vertex,
                    corners[static_cast<std::size_t>(triangle[2])].vertex
                };
                mesh.faces.push_back(face);
                FaceTexCoords faceUv;
                for (Vec2& uv : faceUv.uv) {
                    uv = Vec2::Zero();
                }
                faceUv.valid = allTextured;
                if (allTextured) {
                    for (int corner = 0; corner < 3; ++corner) {
                        const int polygonCorner = triangle[static_cast<std::size_t>(corner)];
                        const int textureId = corners[static_cast<std::size_t>(polygonCorner)].texcoord;
                        faceUv.uv[static_cast<std::size_t>(corner)] = texcoords[static_cast<std::size_t>(textureId)];
                    }
                }
                mesh.faceTexCoords.push_back(faceUv);
            }
        }
        // Everything else (vn, g, o, s, usemtl, comments, ...) is ignored.
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
    if (error) {
        error->clear();
    }
    std::string extension = pathFromUtf8(path).extension().u8string();
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

bool saveBinaryStl(const std::string& path, const Mesh& mesh, std::string* error) {
    if (!validateMeshGeometry(mesh, error)) {
        return false;
    }
    if (mesh.faces.size() > std::numeric_limits<uint32_t>::max()) {
        if (error) {
            *error = "Binary STL supports at most UINT32_MAX triangles.";
        }
        return false;
    }
    const double maxFloat = static_cast<double>(std::numeric_limits<float>::max());
    for (std::size_t vertexIndex = 0; vertexIndex < mesh.vertices.size(); ++vertexIndex) {
        const Vec3& vertex = mesh.vertices[vertexIndex];
        if (std::abs(vertex.x()) > maxFloat || std::abs(vertex.y()) > maxFloat || std::abs(vertex.z()) > maxFloat) {
            if (error) {
                *error = "Mesh vertex " + std::to_string(vertexIndex) +
                         " contains a coordinate outside the binary STL float32 range.";
            }
            return false;
        }
    }

    const std::filesystem::path outputPath = pathFromUtf8(path);
    if (outputPath.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(outputPath.parent_path(), ec);
        if (ec) {
            if (error)
                *error = "Failed to create output directory: " + ec.message();
            return false;
        }
    }
    std::ofstream out(outputPath, std::ios::binary);
    if (!out) {
        if (error)
            *error = "Failed to open output STL.";
        return false;
    }

    std::array<char, 80> header{};
    constexpr char kHeaderText[] = "ManuMesh binary STL";
    std::copy_n(kHeaderText, sizeof(kHeaderText) - 1, header.begin());
    out.write(header.data(), static_cast<std::streamsize>(header.size()));

    std::array<char, 4> triangleCountBytes{};
    writeUint32LE(triangleCountBytes.data(), static_cast<uint32_t>(mesh.faces.size()));
    out.write(triangleCountBytes.data(), static_cast<std::streamsize>(triangleCountBytes.size()));

    std::array<char, 50> record{};
    for (const Face& face : mesh.faces) {
        record.fill('\0');
        const Vec3& a = mesh.vertices[face.v[0]];
        const Vec3& b = mesh.vertices[face.v[1]];
        const Vec3& c = mesh.vertices[face.v[2]];
        const Vec3 normal = triangleNormal(a, b, c);
        const std::array<Vec3, 4> values = {normal, a, b, c};
        for (std::size_t valueIndex = 0; valueIndex < values.size(); ++valueIndex) {
            const Vec3& value = values[valueIndex];
            const std::size_t offset = valueIndex * 12;
            writeFloatLE(record.data() + offset, static_cast<float>(value.x()));
            writeFloatLE(record.data() + offset + 4, static_cast<float>(value.y()));
            writeFloatLE(record.data() + offset + 8, static_cast<float>(value.z()));
        }
        out.write(record.data(), static_cast<std::streamsize>(record.size()));
    }
    out.flush();
    if (!out) {
        if (error)
            *error = "Failed to write output STL: the stream reported an error (disk full or I/O failure).";
        return false;
    }
    return true;
}

bool saveAsciiStl(const std::string& path, const Mesh& mesh, const std::string& solidName, std::string* error) {
    if (!validateMeshGeometry(mesh, error)) {
        return false;
    }
    const std::filesystem::path outputPath = pathFromUtf8(path);
    if (outputPath.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(outputPath.parent_path(), ec);
        if (ec) {
            if (error)
                *error = "Failed to create output directory: " + ec.message();
            return false;
        }
    }
    std::ofstream out(outputPath);
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
    out.flush();
    if (!out) {
        if (error)
            *error = "Failed to write output STL: the stream reported an error (disk full or I/O failure).";
        return false;
    }
    return true;
}

} // namespace manumesh
