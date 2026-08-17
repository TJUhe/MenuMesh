/**
 * @file src/io/MeshIo.cpp
 * @brief 解析 STL、OBJ 网格并序列化 ASCII 或二进制 STL。
 * @ingroup manumesh_io
 *
 * @details 实现确定性的 STL/OBJ 解析以及 ASCII/二进制 STL 序列化。
 * @algorithm 二进制 STL 根据记录布局识别，ASCII STL 使用令牌解析；重合 STL 顶点通过
 * 尺度相关量化合并。OBJ 多边形投影到主导平面，凸面使用稳定的扇形三角化，凹面使用
 * 经过校验的耳切法。
 * @failuremodes 重复、退化、自交、非有限或截断输入会返回诊断并被拒绝，不会部分输出。
 */

#include "io/MeshIo.h"

#include "core/Filesystem.h"
#include "core/Tolerances.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale.h>
#include <locale>
#include <new>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <unordered_map>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace manumesh {
namespace {

// These limits keep untrusted mesh input from turning a nominal load operation
// into an unbounded allocation.  They are deliberately well below the int
// index limit used by Mesh and can be raised in a future, versioned API.
constexpr std::uintmax_t kMaxInputFileBytes = 512ull * 1024ull * 1024ull;
constexpr std::size_t kMaxStlTriangles = 10000000u;
constexpr std::size_t kMaxObjVertices = 10000000u;
constexpr std::size_t kMaxObjTexcoords = 10000000u;
constexpr std::size_t kMaxObjTriangles = 10000000u;
constexpr std::size_t kMaxObjFaceCorners = 4096u;
// Parsing a large, valid industrial file may legitimately need hundreds of
// megabytes of temporary storage.  Keep the budget high enough for that use
// case while bounding adversarial STL/OBJ input before allocations grow
// without limit.
constexpr std::uintmax_t kMaxEstimatedLoadBytes = 1ull * 1024ull * 1024ull * 1024ull;
constexpr std::uintmax_t kEstimatedStlBytesPerTriangle = 512u;
constexpr std::uintmax_t kEstimatedObjFixedBytes = 64ull * 1024ull * 1024ull;
constexpr std::uintmax_t kMaxObjTriangulationWork = 250000000ull;

bool checkedAdd(std::uintmax_t lhs, std::uintmax_t rhs, std::uintmax_t& result) {
    if (rhs > std::numeric_limits<std::uintmax_t>::max() - lhs) {
        return false;
    }
    result = lhs + rhs;
    return true;
}

bool checkedMultiply(std::uintmax_t lhs, std::uintmax_t rhs, std::uintmax_t& result) {
    if (lhs != 0 && rhs > std::numeric_limits<std::uintmax_t>::max() / lhs) {
        return false;
    }
    result = lhs * rhs;
    return true;
}

bool estimatedStlBytesWithinBudget(std::size_t triangleCount) {
    std::uintmax_t estimate = 0;
    return checkedMultiply(static_cast<std::uintmax_t>(triangleCount), kEstimatedStlBytesPerTriangle, estimate) &&
           estimate <= kMaxEstimatedLoadBytes;
}

manumesh::filesystem::path pathFromUtf8(const std::string& path) { return manumesh::filesystem::u8path(path); }

/** @brief 用于 STL 顶点合并的量化三维位置。*/
struct QuantizedKey {
    long long x = 0;
    long long y = 0;
    long long z = 0;

    bool operator==(const QuantizedKey& other) const { return x == other.x && y == other.y && z == other.z; }
};

/** @brief 量化 STL 顶点位置的稳定哈希。*/
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
    // std::llround 在参数超出 long long 范围时行为未定义，因此先将过大商值
    // 截断到可表示范围；9.0e18 低于约 9.22e18 的 long long 上限。
    constexpr double kMaxQuantized = 9.0e18;
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
    const auto* data = reinterpret_cast<const unsigned char*>(bytes);
    return static_cast<uint32_t>(data[0]) | (static_cast<uint32_t>(data[1]) << 8u) |
           (static_cast<uint32_t>(data[2]) << 16u) | (static_cast<uint32_t>(data[3]) << 24u);
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
    const uint32_t bits = readUint32LE(bytes);
    float value = 0.0f;
    static_assert(std::numeric_limits<float>::is_iec559, "Binary STL requires IEEE-754 floats.");
    static_assert(sizeof(bits) == sizeof(value), "Binary STL requires 32-bit IEEE-754 floats.");
    std::memcpy(&value, &bits, sizeof(value));
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

class ClassicNumericLocale {
public:
    ClassicNumericLocale()
        : value_(_create_locale(LC_NUMERIC, "C")) {}
    ~ClassicNumericLocale() {
        if (value_ != nullptr) {
            _free_locale(value_);
        }
    }

    ClassicNumericLocale(const ClassicNumericLocale&) = delete;
    ClassicNumericLocale& operator=(const ClassicNumericLocale&) = delete;

    _locale_t get() const { return value_; }

private:
    _locale_t value_ = nullptr;
};

_locale_t classicNumericLocale() {
    static const ClassicNumericLocale locale;
    return locale.get();
}

/**
 * @brief 从 `p` 解析 double，并允许显式的前导 `+`。返回首个未消费字符；无法解析时返回 nullptr。
 */
const char* parseDoubleAt(const char* p, const char* end, double& value) {
    if (p == end || isAsciiSpace(*p)) {
        return nullptr;
    }
    if (p != end && *p == '+') {
        ++p;
        if (p != end && (*p == '+' || *p == '-')) {
            return nullptr;
        }
    }
    const char* magnitude = p;
    if (magnitude != end && *magnitude == '-') {
        ++magnitude;
    }
    if (p == end || (end - magnitude >= 2 && magnitude[0] == '0' && (magnitude[1] == 'x' || magnitude[1] == 'X'))) {
        return nullptr;
    }
    const char* const tokenEnd = findTokenEnd(p, end);

    const _locale_t locale = classicNumericLocale();
    if (locale == nullptr) {
        return nullptr;
    }
    errno = 0;
    char* parsedEnd = nullptr;
    const double parsed = _strtod_l(p, &parsedEnd, locale);
    // C libraries may report ERANGE for a non-zero subnormal even though the
    // value is still representable.  Accept that case so saveObj/loadObj can
    // round-trip every finite UV value, while continuing to reject values
    // that underflow all the way to zero.  Overflow is rejected by each
    // caller's finite-value check.
    if (parsedEnd != tokenEnd || (errno == ERANGE && parsed == 0.0)) {
        return nullptr;
    }
    value = parsed;
    return tokenEnd;
}

/**
 * @brief 将整个文件读入 `text`；无法打开或完整读取时返回 false。
 */
bool readFileToString(const std::string& path, std::string& text, std::string* error) {
    const manumesh::filesystem::path inputPath = pathFromUtf8(path);
    std::error_code ec;
    const std::uintmax_t size = manumesh::filesystem::file_size(inputPath, ec);
    if (ec) {
        if (error) {
            *error = "Failed to determine input file size: " + ec.message();
        }
        return false;
    }
    if (size > kMaxInputFileBytes || size > static_cast<std::uintmax_t>(std::numeric_limits<std::streamsize>::max())) {
        if (error) {
            *error = "Input file exceeds the supported 512 MiB size limit.";
        }
        return false;
    }
    std::ifstream in(inputPath, std::ios::binary);
    if (!in) {
        if (error) {
            *error = "Failed to open input file.";
        }
        return false;
    }
    text.resize(static_cast<std::size_t>(size));
    if (size > 0) {
        in.read(&text[0], static_cast<std::streamsize>(size));
        if (in.gcount() != static_cast<std::streamsize>(size)) {
            if (error) {
                *error = "Failed to read the complete input file.";
            }
            return false;
        }
    }
    return true;
}

manumesh::filesystem::path temporaryOutputPath(const manumesh::filesystem::path& outputPath) {
    static std::atomic<unsigned long long> sequence{0};
    const auto tick =
        static_cast<unsigned long long>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    const auto thread = static_cast<unsigned long long>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
    const auto ordinal = sequence.fetch_add(1, std::memory_order_relaxed);
#if defined(_WIN32)
    const auto process = static_cast<unsigned long long>(GetCurrentProcessId());
#else
    const auto process = 0ull;
#endif
    return outputPath.parent_path() /
           (outputPath.filename().u8string() + ".manumesh-" + std::to_string(tick) + "-" + std::to_string(process) +
            "-" + std::to_string(thread) + "-" + std::to_string(ordinal) + ".tmp");
}

bool replaceOutputFile(
    const manumesh::filesystem::path& temporaryPath, const manumesh::filesystem::path& outputPath, std::string* error
) {
#if defined(_WIN32)
    if (MoveFileExW(temporaryPath.c_str(), outputPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) ==
        0) {
        if (error) {
            *error = "Failed to atomically replace output file (Windows error " + std::to_string(GetLastError()) + ").";
        }
        return false;
    }
#else
    std::error_code ec;
    manumesh::filesystem::rename(temporaryPath, outputPath, ec);
    if (ec) {
        if (error) {
            *error = "Failed to atomically replace output file: " + ec.message();
        }
        return false;
    }
#endif
    return true;
}

enum class StlFormat { Binary, BinaryOrAscii, Ascii, Invalid };

/**
 * @brief 判断 STL 文件是二进制格式还是 ASCII 格式。
 *
 * 文件大小恰好匹配二进制布局（84 + 50 * n 字节）时，即使头部以 "solid" 开头也按二进制处理。
 * 不以 "solid" 开头的文件同样按二进制处理；允许最后一条记录后存在尾部填充，但短文件会报告
 * 为截断的二进制 STL，而不会回退到 ASCII 解析器。
 */
StlFormat probeStlFormat(const std::string& path, uint32_t& triangleCount, std::string* error) {
    const manumesh::filesystem::path inputPath = pathFromUtf8(path);
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
        offset = 3; // 跳过 UTF-8 字节顺序标记。
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
    const std::uintmax_t fileSize = manumesh::filesystem::file_size(inputPath, ec);
    if (ec) {
        if (startsWithSolid) {
            return StlFormat::Ascii;
        }
        if (error)
            *error = "Failed to determine binary STL file size.";
        return StlFormat::Invalid;
    }
    if (fileSize > kMaxInputFileBytes) {
        if (error)
            *error = "Input STL exceeds the supported 512 MiB size limit.";
        return StlFormat::Invalid;
    }

    const std::uintmax_t expectedSize =
        static_cast<std::uintmax_t>(84) + static_cast<std::uintmax_t>(triangleCount) * 50u;
    if (fileSize == expectedSize) {
        if (triangleCount > kMaxStlTriangles) {
            if (error)
                *error = "STL declares more than the supported 10,000,000 triangles.";
            return StlFormat::Invalid;
        }
        if (!estimatedStlBytesWithinBudget(triangleCount)) {
            if (error)
                *error = "STL exceeds the supported temporary-memory budget.";
            return StlFormat::Invalid;
        }
        return StlFormat::Binary;
    }
    // A valid binary STL may have a human-readable header beginning with
    // "solid" and exporter-specific padding after its records.  Such a file
    // is ambiguous with ASCII until the strict ASCII grammar is attempted.
    if (fileSize > expectedSize && triangleCount > 0 && triangleCount <= kMaxStlTriangles) {
        if (!estimatedStlBytesWithinBudget(triangleCount)) {
            if (error)
                *error = "STL exceeds the supported temporary-memory budget.";
            return StlFormat::Invalid;
        }
        return startsWithSolid ? StlFormat::BinaryOrAscii : StlFormat::Binary;
    }
    if (startsWithSolid) {
        return StlFormat::Ascii;
    }
    if (triangleCount > kMaxStlTriangles) {
        if (error)
            *error = "STL declares more than the supported 10,000,000 triangles.";
        return StlFormat::Invalid;
    }
    if (fileSize > expectedSize) {
        // 某些导出器会在最后一条记录后追加填充字节；忽略这些字节。
        if (!estimatedStlBytesWithinBudget(triangleCount)) {
            if (error)
                *error = "STL exceeds the supported temporary-memory budget.";
            return StlFormat::Invalid;
        }
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
    if (triangleCount > kMaxStlTriangles || !estimatedStlBytesWithinBudget(triangleCount)) {
        if (error) {
            *error = "Binary STL exceeds the supported triangle or temporary-memory limit.";
        }
        return false;
    }
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

bool asciiTokenEquals(const char* begin, const char* end, const char* literal) {
    const std::size_t length = std::strlen(literal);
    if (static_cast<std::size_t>(end - begin) != length) {
        return false;
    }
    for (std::size_t i = 0; i < length; ++i) {
        const unsigned char value = static_cast<unsigned char>(begin[i]);
        if (static_cast<char>(std::tolower(value)) != literal[i]) {
            return false;
        }
    }
    return true;
}

bool parseAsciiStlVector(const char* p, const char* lineEnd, double (&coords)[3]) {
    for (double& coord : coords) {
        p = skipSpaces(p, lineEnd);
        const char* next = parseDoubleAt(p, lineEnd, coord);
        if (next == nullptr) {
            return false;
        }
        p = next;
    }
    return skipSpaces(p, lineEnd) == lineEnd && std::isfinite(coords[0]) && std::isfinite(coords[1]) &&
           std::isfinite(coords[2]);
}

std::string asciiStlLineError(int lineNumber, const char* message) {
    return "ASCII STL line " + std::to_string(lineNumber) + ": " + message;
}

bool readAsciiTriangles(const std::string& path, std::vector<std::array<Vec3, 3>>& triangles, std::string* error) {
    std::string text;
    if (!readFileToString(path, text, error)) {
        if (error)
            *error = "Failed to read ASCII STL: " + *error;
        return false;
    }

    enum class State { Solid, FacetOrEndSolid, OuterLoop, Vertex, EndLoop, EndFacet, Done };
    State state = State::Solid;
    std::array<Vec3, 3> pending{};
    int pendingCount = 0;
    triangles.clear();

    const char* lineStart = text.data();
    const char* const end = lineStart + text.size();
    int lineNumber = 0;
    bool firstLine = true;
    while (lineStart != end) {
        ++lineNumber;
        const char* lineEnd = lineStart;
        while (lineEnd != end && *lineEnd != '\r' && *lineEnd != '\n') {
            ++lineEnd;
        }

        const char* p = lineStart;
        if (firstLine && lineEnd - p >= 3 && std::memcmp(p, "\xEF\xBB\xBF", 3) == 0) {
            p += 3;
        }
        firstLine = false;
        p = skipSpaces(p, lineEnd);
        if (p != lineEnd) {
            const char* const tokenEnd = findTokenEnd(p, lineEnd);
            const auto fail = [&](const char* message) {
                if (error) {
                    *error = asciiStlLineError(lineNumber, message);
                }
                return false;
            };

            if (state == State::Solid) {
                if (!asciiTokenEquals(p, tokenEnd, "solid")) {
                    return fail("expected solid header.");
                }
                state = State::FacetOrEndSolid;
            } else if (state == State::FacetOrEndSolid) {
                if (asciiTokenEquals(p, tokenEnd, "endsolid")) {
                    state = State::Done;
                } else if (asciiTokenEquals(p, tokenEnd, "facet")) {
                    const char* q = skipSpaces(tokenEnd, lineEnd);
                    const char* const normalEnd = findTokenEnd(q, lineEnd);
                    if (!asciiTokenEquals(q, normalEnd, "normal")) {
                        return fail("facet record must contain a normal.");
                    }
                    double normal[3] = {0.0, 0.0, 0.0};
                    if (!parseAsciiStlVector(normalEnd, lineEnd, normal)) {
                        return fail("facet normal must contain exactly three finite coordinates.");
                    }
                    state = State::OuterLoop;
                } else {
                    return fail("expected facet or endsolid.");
                }
            } else if (state == State::OuterLoop) {
                const char* q = skipSpaces(tokenEnd, lineEnd);
                const char* const loopEnd = findTokenEnd(q, lineEnd);
                if (!asciiTokenEquals(p, tokenEnd, "outer") || !asciiTokenEquals(q, loopEnd, "loop") ||
                    skipSpaces(loopEnd, lineEnd) != lineEnd) {
                    return fail("expected outer loop.");
                }
                pendingCount = 0;
                state = State::Vertex;
            } else if (state == State::Vertex) {
                if (!asciiTokenEquals(p, tokenEnd, "vertex")) {
                    return fail("expected vertex.");
                }
                double coords[3] = {0.0, 0.0, 0.0};
                if (!parseAsciiStlVector(tokenEnd, lineEnd, coords)) {
                    return fail("vertex must contain exactly three finite coordinates.");
                }
                pending[static_cast<std::size_t>(pendingCount)] = Vec3(coords[0], coords[1], coords[2]);
                ++pendingCount;
                if (pendingCount == 3) {
                    state = State::EndLoop;
                }
            } else if (state == State::EndLoop) {
                if (!asciiTokenEquals(p, tokenEnd, "endloop") || skipSpaces(tokenEnd, lineEnd) != lineEnd) {
                    return fail("expected endloop after exactly three vertices.");
                }
                state = State::EndFacet;
            } else if (state == State::EndFacet) {
                if (!asciiTokenEquals(p, tokenEnd, "endfacet") || skipSpaces(tokenEnd, lineEnd) != lineEnd) {
                    return fail("expected endfacet.");
                }
                if (triangles.size() == kMaxStlTriangles || !estimatedStlBytesWithinBudget(triangles.size() + 1)) {
                    return fail("exceeds the supported triangle or temporary-memory limit.");
                }
                triangles.push_back(pending);
                state = State::FacetOrEndSolid;
            } else {
                // Some exporters concatenate separately named ASCII STL
                // solids into one file. Accept a new solid header while
                // retaining strict rejection of every other trailing token.
                if (!asciiTokenEquals(p, tokenEnd, "solid")) {
                    return fail("contains data after endsolid.");
                }
                state = State::FacetOrEndSolid;
            }
        }

        lineStart = lineEnd;
        while (lineStart != end && (*lineStart == '\r' || *lineStart == '\n')) {
            ++lineStart;
        }
    }

    if (state != State::Done) {
        if (error) {
            *error = "ASCII STL ended before a complete endsolid record.";
        }
        return false;
    }
    if (triangles.empty()) {
        if (error) {
            *error = "No triangles found in ASCII STL.";
        }
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
        // 当有限端点跨度超过 DBL_MAX 时，先缩放再相减，仍可表示通常的相对小容差。
        return std::abs(high * mergeRelativeEpsilon - low * mergeRelativeEpsilon);
    };
    double relativeEps = 0.0;
    if (mergeRelativeEpsilon > 0.0) {
        relativeEps =
            std::hypot(std::hypot(scaledSpan(lo.x(), hi.x()), scaledSpan(lo.y(), hi.y())), scaledSpan(lo.z(), hi.z()));
        if (!std::isfinite(relativeEps)) {
            relativeEps = std::numeric_limits<double>::max();
        }
    }
    const double eps = std::max(relativeEps, 1e-12);

    std::unordered_map<QuantizedKey, std::vector<int>, QuantizedKeyHash> indicesByKey;
    indicesByKey.reserve(triangles.size() * 3);

    auto addVertex = [&](const Vec3& p) {
        // 以稳定的局部原点进行量化，避免网格平移到远离零点的位置后所有坐标都饱和为同一键。
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
 * @brief 解析一个 OBJ 索引片段（例如 `3/1/2` 中的 `3`），并根据 `valueCount` 解析负索引。
 * 整个片段必须是有效整数，解析后的从零开始索引也必须在范围内。
 */
bool parseObjIndexText(const char* begin, const char* end, int valueCount, int& indexOut) {
    if (begin == end) {
        return false;
    }

    bool negative = false;
    if (*begin == '+' || *begin == '-') {
        negative = *begin == '-';
        ++begin;
        if (begin == end || *begin == '+' || *begin == '-') {
            return false;
        }
    }

    const unsigned int maxPositive = static_cast<unsigned int>(std::numeric_limits<int>::max());
    const unsigned int magnitudeLimit = negative ? maxPositive + 1u : maxPositive;
    unsigned int magnitude = 0;
    for (const char* cursor = begin; cursor != end; ++cursor) {
        if (*cursor < '0' || *cursor > '9') {
            return false;
        }
        const unsigned int digit = static_cast<unsigned int>(*cursor - '0');
        if (magnitude > (magnitudeLimit - digit) / 10u) {
            return false;
        }
        magnitude = magnitude * 10u + digit;
    }
    if (magnitude == 0) {
        return false;
    }

    const long long raw = negative ? -static_cast<long long>(magnitude) : static_cast<long long>(magnitude);
    const long long index = raw > 0 ? raw - 1 : static_cast<long long>(valueCount) + raw;
    if (index < 0 || index >= valueCount) {
        return false;
    }
    indexOut = static_cast<int>(index);
    return true;
}

/** @brief 一个 OBJ 面角点解析后的顶点索引和可选纹理坐标索引。*/
struct ObjCorner {
    int vertex = -1;
    int texcoord = -1;
};

using ObjTriangle = std::array<int, 3>;

bool estimatedObjBytesWithinBudget(
    std::size_t textBytes,
    std::size_t vertexCount,
    std::size_t texcoordCount,
    std::size_t faceCount,
    std::size_t faceTexcoordCount,
    std::size_t scratchCornerCount
) {
    // Account for vector growth and removeUnusedVertices()'s compacted copy.
    // The factors are deliberately conservative; they are a budget estimate,
    // not an allocator-specific byte count.
    std::uintmax_t estimate = kEstimatedObjFixedBytes;
    std::uintmax_t term = 0;
    if (!checkedAdd(estimate, static_cast<std::uintmax_t>(textBytes), estimate) ||
        !checkedMultiply(static_cast<std::uintmax_t>(vertexCount), sizeof(Vec3) * 4u, term) ||
        !checkedAdd(estimate, term, estimate) ||
        !checkedMultiply(static_cast<std::uintmax_t>(texcoordCount), sizeof(Vec2) * 2u, term) ||
        !checkedAdd(estimate, term, estimate) ||
        !checkedMultiply(static_cast<std::uintmax_t>(faceCount), sizeof(Face) * 4u, term) ||
        !checkedAdd(estimate, term, estimate) ||
        !checkedMultiply(static_cast<std::uintmax_t>(faceTexcoordCount), sizeof(FaceTexCoords) * 4u, term) ||
        !checkedAdd(estimate, term, estimate) ||
        !checkedMultiply(static_cast<std::uintmax_t>(scratchCornerCount), sizeof(ObjCorner) * 4u, term) ||
        !checkedAdd(estimate, term, estimate)) {
        return false;
    }
    return estimate <= kMaxEstimatedLoadBytes;
}

struct ObjWorkBudget {
    std::uintmax_t used = 0;

    bool consume(std::uintmax_t amount = 1) {
        if (used > kMaxObjTriangulationWork || amount > kMaxObjTriangulationWork - used) {
            return false;
        }
        used += amount;
        return true;
    }
};

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

bool polygonSelfIntersects(const std::vector<Vec2>& polygon, ObjWorkBudget& work, bool& budgetExhausted) {
    const int size = static_cast<int>(polygon.size());
    for (int first = 0; first < size; ++first) {
        const int firstNext = (first + 1) % size;
        for (int second = first + 1; second < size; ++second) {
            const int secondNext = (second + 1) % size;
            if (first == second || firstNext == second || secondNext == first) {
                continue;
            }
            if (!work.consume()) {
                budgetExhausted = true;
                return false;
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
    ObjWorkBudget& work,
    const char*& failureReason
) {
    triangles.clear();
    if (corners.size() < 3) {
        failureReason = "face has fewer than three corners.";
        return false;
    }
    if (corners.size() == 3) {
        if (!work.consume()) {
            failureReason = "cumulative triangulation work exceeds the supported limit.";
            return false;
        }
        const Vec3& a = positions[static_cast<std::size_t>(corners[0].vertex)];
        const Vec3& b = positions[static_cast<std::size_t>(corners[1].vertex)];
        const Vec3& c = positions[static_cast<std::size_t>(corners[2].vertex)];
        const double area = triangleArea(a, b, c);
        if (!std::isfinite(area) || area <= kMinTriangleArea) {
            failureReason = "face triangle is degenerate or exceeds the supported numeric range.";
            return false;
        }
        triangles.push_back({0, 1, 2});
        return true;
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
            if (!work.consume()) {
                failureReason = "cumulative triangulation work exceeds the supported limit.";
                return false;
            }
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
                if (!work.consume()) {
                    failureReason = "cumulative triangulation work exceeds the supported limit.";
                    return false;
                }
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
    bool budgetExhausted = false;
    if (polygonSelfIntersects(projected, work, budgetExhausted)) {
        failureReason = "face polygon is self-intersecting.";
        return false;
    }
    if (budgetExhausted) {
        failureReason = "cumulative triangulation work exceeds the supported limit.";
        return false;
    }
    if (std::abs(signedAreaTwice) <= kObjPolygonEpsilon) {
        failureReason = "face polygon has zero projected area.";
        return false;
    }
    const int orientation = signedAreaTwice > 0.0 ? 1 : -1;

    bool strictlyConvex = true;
    for (std::size_t i = 0; i < projected.size(); ++i) {
        if (!work.consume()) {
            failureReason = "cumulative triangulation work exceeds the supported limit.";
            return false;
        }
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
            if (!work.consume()) {
                failureReason = "cumulative triangulation work exceeds the supported limit.";
                return false;
            }
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
                if (!work.consume()) {
                    failureReason = "cumulative triangulation work exceeds the supported limit.";
                    return false;
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
    if (!work.consume()) {
        failureReason = "cumulative triangulation work exceeds the supported limit.";
        return false;
    }
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
 * @brief 解析一个 OBJ 面角点令牌：`v`、`v/vt`、`v//vn` 或 `v/vt/vn`。
 * 第二个斜杠后的法线索引会按设计忽略。
 */
bool parseObjCorner(
    const char* begin, const char* end, int vertexCount, int texcoordCount, int normalCount, ObjCorner& corner
) {
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
    if (secondSlash == nullptr) {
        // OBJ does not permit a dangling "v/" token.
        return texcoordBegin != texcoordEnd &&
               parseObjIndexText(texcoordBegin, texcoordEnd, texcoordCount, corner.texcoord);
    }
    if (std::memchr(secondSlash + 1, '/', static_cast<std::size_t>(end - (secondSlash + 1))) != nullptr) {
        return false;
    }
    // v//vn is valid, but both the optional texture index and mandatory normal
    // index must be complete, in-range integer tokens.
    if (texcoordBegin != texcoordEnd &&
        !parseObjIndexText(texcoordBegin, texcoordEnd, texcoordCount, corner.texcoord)) {
        return false;
    }
    int ignoredNormal = -1;
    return parseObjIndexText(secondSlash + 1, end, normalCount, ignoredNormal);
}

} // 命名空间

bool loadStl(const std::string& path, Mesh& mesh, std::string* error, double mergeRelativeEpsilon) {
    if (error) {
        error->clear();
    }
    try {
        if (!std::isfinite(mergeRelativeEpsilon) || mergeRelativeEpsilon < 0.0) {
            if (error)
                *error = "mergeRelativeEpsilon must be finite and non-negative.";
            return false;
        }
        uint32_t triangleCount = 0;
        std::string localError;
        const StlFormat format = probeStlFormat(path, triangleCount, &localError);

        std::vector<std::array<Vec3, 3>> triangles;
        bool loaded = false;
        if (format == StlFormat::BinaryOrAscii) {
            // Preserve a valid ASCII STL even when bytes 80..83 happen to form a
            // plausible binary triangle count; a padded binary solid header will
            // fail this grammar check and then use the binary records.
            std::string asciiError;
            loaded = readAsciiTriangles(path, triangles, &asciiError);
            if (!loaded) {
                loaded = readBinaryTriangles(path, triangleCount, triangles, &localError);
            }
        } else if (format == StlFormat::Binary) {
            loaded = readBinaryTriangles(path, triangleCount, triangles, &localError);
        } else if (format == StlFormat::Ascii) {
            loaded = readAsciiTriangles(path, triangles, &localError);
        }
        if (!loaded) {
            if (error)
                *error = localError;
            return false;
        }

        Mesh parsed;
        mergeDuplicateTriangleVertices(triangles, parsed, mergeRelativeEpsilon);
        parsed.removeUnusedVertices();
        if (parsed.empty()) {
            if (error) {
                *error = "STL contains no non-degenerate triangles after vertex merging.";
            }
            return false;
        }
        std::string validationError;
        if (!validateMeshGeometry(parsed, &validationError)) {
            if (error) {
                *error = "STL produced an invalid mesh: " + validationError;
            }
            return false;
        }
        mesh = std::move(parsed);
        return true;
    } catch (const std::bad_alloc&) {
        if (error) {
            *error = "Mesh load ran out of memory.";
        }
        return false;
    } catch (const std::length_error&) {
        if (error) {
            *error = "Mesh load exceeded a container size limit.";
        }
        return false;
    }
}

bool loadObj(const std::string& path, Mesh& mesh, std::string* error) {
    if (error) {
        error->clear();
    }
    try {
        std::string text;
        if (!readFileToString(path, text, error)) {
            if (error)
                *error = "Failed to read OBJ: " + *error;
            return false;
        }

        std::vector<Vec3> positions;
        std::vector<Vec2> texcoords;
        std::size_t normalCount = 0;
        Mesh parsed;

        std::vector<ObjCorner> corners;
        std::vector<ObjTriangle> polygonTriangles;
        ObjWorkBudget work;
        if (!estimatedObjBytesWithinBudget(text.size(), 0, 0, 0, 0, 0)) {
            if (error) {
                *error = "OBJ exceeds the supported temporary-memory budget.";
            }
            return false;
        }
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

            const char* const comment =
                static_cast<const char*>(std::memchr(cursor, '#', static_cast<std::size_t>(lineEnd - cursor)));
            if (comment != nullptr) {
                lineEnd = comment;
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
                if (q == nullptr || !std::isfinite(coords[0]) || !std::isfinite(coords[1]) ||
                    !std::isfinite(coords[2])) {
                    if (error)
                        *error = objLineError(lineNumber, "malformed or non-finite vertex coordinate.");
                    return false;
                }
                q = skipSpaces(q, lineEnd);
                if (q != lineEnd) {
                    double homogeneous = 1.0;
                    q = parseDoubleAt(q, lineEnd, homogeneous);
                    q = q == nullptr ? nullptr : skipSpaces(q, lineEnd);
                    if (q == nullptr || q != lineEnd || !std::isfinite(homogeneous) || homogeneous == 0.0) {
                        if (error)
                            *error = objLineError(lineNumber, "vertex record has an invalid homogeneous coordinate.");
                        return false;
                    }
                    coords[0] /= homogeneous;
                    coords[1] /= homogeneous;
                    coords[2] /= homogeneous;
                    if (!std::isfinite(coords[0]) || !std::isfinite(coords[1]) || !std::isfinite(coords[2])) {
                        if (error)
                            *error = objLineError(lineNumber, "homogeneous vertex coordinate is not finite.");
                        return false;
                    }
                }
                if (positions.size() == kMaxObjVertices) {
                    if (error)
                        *error = "OBJ exceeds the supported 10,000,000 vertex limit.";
                    return false;
                }
                if (!estimatedObjBytesWithinBudget(
                        text.size(),
                        positions.size() + 1,
                        texcoords.size(),
                        parsed.faces.size(),
                        parsed.faceTexCoords.size(),
                        corners.size()
                    )) {
                    if (error) {
                        *error = "OBJ exceeds the supported temporary-memory budget.";
                    }
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
                // 根据 OBJ 规范，第二个分量（"vt u"）可以省略并默认为零；第三个分量 w 会忽略。
                q = skipSpaces(q, lineEnd);
                if (q != lineEnd) {
                    q = parseDoubleAt(q, lineEnd, v);
                    if (q == nullptr || !std::isfinite(v)) {
                        if (error)
                            *error = objLineError(lineNumber, "malformed or non-finite texture coordinate.");
                        return false;
                    }
                    q = skipSpaces(q, lineEnd);
                    if (q != lineEnd) {
                        double ignoredW = 0.0;
                        q = parseDoubleAt(q, lineEnd, ignoredW);
                        q = q == nullptr ? nullptr : skipSpaces(q, lineEnd);
                        if (q == nullptr || q != lineEnd || !std::isfinite(ignoredW)) {
                            if (error)
                                *error = objLineError(lineNumber, "texture coordinate has invalid trailing data.");
                            return false;
                        }
                    }
                }
                if (texcoords.size() == kMaxObjTexcoords) {
                    if (error)
                        *error = "OBJ exceeds the supported 10,000,000 texture-coordinate limit.";
                    return false;
                }
                if (!estimatedObjBytesWithinBudget(
                        text.size(),
                        positions.size(),
                        texcoords.size() + 1,
                        parsed.faces.size(),
                        parsed.faceTexCoords.size(),
                        corners.size()
                    )) {
                    if (error) {
                        *error = "OBJ exceeds the supported temporary-memory budget.";
                    }
                    return false;
                }
                texcoords.emplace_back(u, v);
            } else if (tagLength == 2 && p[0] == 'v' && p[1] == 'n') {
                double coords[3] = {0.0, 0.0, 0.0};
                const char* q = tagEnd;
                for (double& coord : coords) {
                    q = skipSpaces(q, lineEnd);
                    q = parseDoubleAt(q, lineEnd, coord);
                    if (q == nullptr) {
                        break;
                    }
                }
                q = q == nullptr ? nullptr : skipSpaces(q, lineEnd);
                if (q == nullptr || q != lineEnd || !std::isfinite(coords[0]) || !std::isfinite(coords[1]) ||
                    !std::isfinite(coords[2])) {
                    if (error)
                        *error = objLineError(lineNumber, "malformed or non-finite normal coordinate.");
                    return false;
                }
                if (normalCount == kMaxObjVertices) {
                    if (error)
                        *error = "OBJ exceeds the supported 10,000,000 normal limit.";
                    return false;
                }
                ++normalCount;
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
                            q,
                            tokenEnd,
                            static_cast<int>(positions.size()),
                            static_cast<int>(texcoords.size()),
                            static_cast<int>(normalCount),
                            corner
                        )) {
                        if (error)
                            *error = objLineError(
                                lineNumber, "face references an invalid vertex or texture-coordinate index."
                            );
                        return false;
                    }
                    corners.push_back(corner);
                    if (corners.size() > kMaxObjFaceCorners) {
                        if (error)
                            *error = objLineError(lineNumber, "face exceeds the supported 4096-corner limit.");
                        return false;
                    }
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
                if (!triangulateObjPolygon(corners, positions, polygonTriangles, work, triangulationFailure)) {
                    if (error)
                        *error = objLineError(lineNumber, triangulationFailure);
                    return false;
                }
                for (const ObjTriangle& triangle : polygonTriangles) {
                    if (parsed.faces.size() == kMaxObjTriangles) {
                        if (error)
                            *error = "OBJ exceeds the supported 10,000,000 triangle limit after triangulation.";
                        return false;
                    }
                    const bool trackTextureCoordinates = allTextured || !parsed.faceTexCoords.empty();
                    const std::size_t proposedFaceTexcoords = trackTextureCoordinates ? parsed.faces.size() + 1 : 0;
                    if (!estimatedObjBytesWithinBudget(
                            text.size(),
                            positions.size(),
                            texcoords.size(),
                            parsed.faces.size() + 1,
                            proposedFaceTexcoords,
                            corners.size()
                        )) {
                        if (error) {
                            *error = "OBJ exceeds the supported temporary-memory budget.";
                        }
                        return false;
                    }
                    if (allTextured && parsed.faceTexCoords.empty()) {
                        // Preserve face-to-UV alignment only once a textured face
                        // is encountered.  Untextured OBJ files then avoid a
                        // FaceTexCoords allocation proportional to every face.
                        parsed.faceTexCoords.resize(parsed.faces.size());
                    }
                    Face face;
                    face.v = {
                        corners[static_cast<std::size_t>(triangle[0])].vertex,
                        corners[static_cast<std::size_t>(triangle[1])].vertex,
                        corners[static_cast<std::size_t>(triangle[2])].vertex
                    };
                    parsed.faces.push_back(face);
                    if (trackTextureCoordinates) {
                        FaceTexCoords faceUv;
                        faceUv.valid = allTextured;
                        if (allTextured) {
                            for (int corner = 0; corner < 3; ++corner) {
                                const int polygonCorner = triangle[static_cast<std::size_t>(corner)];
                                const int textureId = corners[static_cast<std::size_t>(polygonCorner)].texcoord;
                                faceUv.uv[static_cast<std::size_t>(corner)] =
                                    texcoords[static_cast<std::size_t>(textureId)];
                            }
                        }
                        parsed.faceTexCoords.push_back(faceUv);
                    }
                }
            }
            // 其他指令（vn、g、o、s、usemtl、注释等）均忽略。
        }

        parsed.vertices = std::move(positions);
        parsed.removeUnusedVertices();
        if (parsed.empty()) {
            if (error)
                *error = "No triangles found in OBJ.";
            return false;
        }
        std::string validationError;
        if (!validateMeshGeometryLenient(parsed, &validationError)) {
            if (error) {
                *error = "OBJ produced an invalid mesh: " + validationError;
            }
            return false;
        }
        mesh = std::move(parsed);
        return true;
    } catch (const std::bad_alloc&) {
        if (error) {
            *error = "Mesh load ran out of memory.";
        }
        return false;
    } catch (const std::length_error&) {
        if (error) {
            *error = "Mesh load exceeded a container size limit.";
        }
        return false;
    }
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

    const manumesh::filesystem::path outputPath = pathFromUtf8(path);
    if (outputPath.has_parent_path()) {
        std::error_code ec;
        manumesh::filesystem::create_directories(outputPath.parent_path(), ec);
        if (ec) {
            if (error)
                *error = "Failed to create output directory: " + ec.message();
            return false;
        }
    }
    const manumesh::filesystem::path temporaryPath = temporaryOutputPath(outputPath);
    std::ofstream out(temporaryPath, std::ios::binary | std::ios::trunc);
    if (!out) {
        if (error)
            *error = "Failed to open temporary output STL.";
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
        const Vec3 quantizedA(static_cast<float>(a.x()), static_cast<float>(a.y()), static_cast<float>(a.z()));
        const Vec3 quantizedB(static_cast<float>(b.x()), static_cast<float>(b.y()), static_cast<float>(b.z()));
        const Vec3 quantizedC(static_cast<float>(c.x()), static_cast<float>(c.y()), static_cast<float>(c.z()));
        const double quantizedArea = triangleArea(quantizedA, quantizedB, quantizedC);
        if (!std::isfinite(quantizedArea) || quantizedArea <= kMinTriangleArea) {
            out.close();
            std::error_code cleanupError;
            manumesh::filesystem::remove(temporaryPath, cleanupError);
            if (error) {
                *error = "Mesh face becomes degenerate after binary STL float32 conversion.";
            }
            return false;
        }
        // The serialized vertices are the float32-quantized values, so derive
        // the advisory STL normal from that same geometry rather than from
        // the higher-precision input triangle.
        const Vec3 normal = triangleNormal(quantizedA, quantizedB, quantizedC);
        const std::array<Vec3, 4> values = {normal, quantizedA, quantizedB, quantizedC};
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
        out.close();
        std::error_code cleanupError;
        manumesh::filesystem::remove(temporaryPath, cleanupError);
        if (error)
            *error = "Failed to write output STL: the stream reported an error (disk full or I/O failure).";
        return false;
    }
    out.close();
    if (!out) {
        std::error_code cleanupError;
        manumesh::filesystem::remove(temporaryPath, cleanupError);
        if (error) {
            *error = "Failed to finalize temporary output STL.";
        }
        return false;
    }
    if (!replaceOutputFile(temporaryPath, outputPath, error)) {
        std::error_code cleanupError;
        manumesh::filesystem::remove(temporaryPath, cleanupError);
        return false;
    }
    return true;
}

bool saveAsciiStl(const std::string& path, const Mesh& mesh, const std::string& solidName, std::string* error) {
    if (!validateMeshGeometry(mesh, error)) {
        return false;
    }
    const manumesh::filesystem::path outputPath = pathFromUtf8(path);
    if (outputPath.has_parent_path()) {
        std::error_code ec;
        manumesh::filesystem::create_directories(outputPath.parent_path(), ec);
        if (ec) {
            if (error)
                *error = "Failed to create output directory: " + ec.message();
            return false;
        }
    }
    std::string sanitizedSolidName;
    sanitizedSolidName.reserve(std::min<std::size_t>(solidName.size(), 256));
    for (unsigned char ch : solidName) {
        if (sanitizedSolidName.size() == 256) {
            break;
        }
        sanitizedSolidName.push_back(std::iscntrl(ch) ? ' ' : static_cast<char>(ch));
    }
    if (sanitizedSolidName.empty()) {
        sanitizedSolidName = "mesh";
    }

    const manumesh::filesystem::path temporaryPath = temporaryOutputPath(outputPath);
    std::ofstream out(temporaryPath, std::ios::out | std::ios::trunc);
    if (!out) {
        out.close();
        std::error_code cleanupError;
        manumesh::filesystem::remove(temporaryPath, cleanupError);
        if (error)
            *error = "Failed to open temporary output STL.";
        return false;
    }

    out.imbue(std::locale::classic());
    out << std::setprecision(17);
    out << "solid " << sanitizedSolidName << "\n";
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
    out << "endsolid " << sanitizedSolidName << "\n";
    out.flush();
    if (!out) {
        out.close();
        std::error_code cleanupError;
        manumesh::filesystem::remove(temporaryPath, cleanupError);
        if (error)
            *error = "Failed to write output STL: the stream reported an error (disk full or I/O failure).";
        return false;
    }
    out.close();
    if (!out) {
        std::error_code cleanupError;
        manumesh::filesystem::remove(temporaryPath, cleanupError);
        if (error) {
            *error = "Failed to finalize temporary output STL.";
        }
        return false;
    }
    if (!replaceOutputFile(temporaryPath, outputPath, error)) {
        std::error_code cleanupError;
        manumesh::filesystem::remove(temporaryPath, cleanupError);
        return false;
    }
    return true;
}

bool saveObj(const std::string& path, const Mesh& mesh, std::string* error) {
    if (!validateMeshGeometry(mesh, error)) {
        return false;
    }
    const manumesh::filesystem::path outputPath = pathFromUtf8(path);
    if (outputPath.has_parent_path()) {
        std::error_code ec;
        manumesh::filesystem::create_directories(outputPath.parent_path(), ec);
        if (ec) {
            if (error) {
                *error = "Failed to create output directory: " + ec.message();
            }
            return false;
        }
    }

    const manumesh::filesystem::path temporaryPath = temporaryOutputPath(outputPath);
    std::ofstream out(temporaryPath, std::ios::out | std::ios::trunc);
    if (!out) {
        out.close();
        std::error_code cleanupError;
        manumesh::filesystem::remove(temporaryPath, cleanupError);
        if (error) {
            *error = "Failed to open temporary output OBJ.";
        }
        return false;
    }

    out.imbue(std::locale::classic());
    out << std::setprecision(17);
    for (const Vec3& vertex : mesh.vertices) {
        out << "v " << vertex.x() << " " << vertex.y() << " " << vertex.z() << "\n";
    }

    bool hasTexcoords = false;
    for (const FaceTexCoords& texcoords : mesh.faceTexCoords) {
        if (texcoords.valid) {
            hasTexcoords = true;
            for (const Vec2& uv : texcoords.uv) {
                out << "vt " << uv.x() << " " << uv.y() << "\n";
            }
        }
    }

    std::size_t nextTexcoord = 1;
    for (std::size_t faceIndex = 0; faceIndex < mesh.faces.size(); ++faceIndex) {
        const Face& face = mesh.faces[faceIndex];
        const bool faceHasTexcoords =
            hasTexcoords && faceIndex < mesh.faceTexCoords.size() && mesh.faceTexCoords[faceIndex].valid;
        if (faceHasTexcoords) {
            out << "f " << face.v[0] + 1 << "/" << nextTexcoord << " " << face.v[1] + 1 << "/" << nextTexcoord + 1
                << " " << face.v[2] + 1 << "/" << nextTexcoord + 2 << "\n";
            nextTexcoord += 3;
        } else {
            out << "f " << face.v[0] + 1 << " " << face.v[1] + 1 << " " << face.v[2] + 1 << "\n";
        }
    }

    out.flush();
    if (!out) {
        out.close();
        std::error_code cleanupError;
        manumesh::filesystem::remove(temporaryPath, cleanupError);
        if (error) {
            *error = "Failed to write output OBJ: the stream reported an error (disk full or I/O failure).";
        }
        return false;
    }
    out.close();
    if (!out) {
        std::error_code cleanupError;
        manumesh::filesystem::remove(temporaryPath, cleanupError);
        if (error) {
            *error = "Failed to finalize temporary output OBJ.";
        }
        return false;
    }
    if (!replaceOutputFile(temporaryPath, outputPath, error)) {
        std::error_code cleanupError;
        manumesh::filesystem::remove(temporaryPath, cleanupError);
        return false;
    }
    return true;
}

} // manumesh 命名空间
