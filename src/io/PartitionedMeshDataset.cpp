/**
 * @file src/io/PartitionedMeshDataset.cpp
 * @brief Implements bounded-memory partitioned triangle storage and binary STL streaming import.
 * @ingroup manumesh_io
 */

#include "io/PartitionedMeshDataset.h"

#include "core/Filesystem.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <functional>
#include <limits>
#include <new>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace manumesh {
namespace {

constexpr std::size_t kDatasetHeaderBytes = 64u;
constexpr std::size_t kPartitionHeaderBytes = 96u;
constexpr std::size_t kDirectoryHeaderBytes = 16u;
constexpr std::size_t kDirectoryRecordBytes = 96u;
constexpr std::size_t kTriangleRecordBytes = 50u;
constexpr std::uint32_t kDatasetVersion = 1u;
constexpr std::uint64_t kChecksumOffsetBasis = 14695981039346656037ull;
constexpr std::uint64_t kChecksumPrime = 1099511628211ull;

const std::array<char, 8> kDatasetMagic{{'M', 'A', 'N', 'U', 'M', 'D', 'S', '\0'}};
const std::array<char, 8> kPartitionMagic{{'M', 'M', 'P', 'A', 'R', 'T', '\0', '\0'}};
const std::array<char, 8> kDirectoryMagic{{'M', 'M', 'D', 'I', 'R', '\0', '\0', '\0'}};

Status invalidArgument(std::string message) { return Status(StatusCode::InvalidArgument, std::move(message)); }
Status ioError(std::string message) { return Status(StatusCode::IoError, std::move(message)); }
Status outOfMemory(std::string message) { return Status(StatusCode::OutOfMemory, std::move(message)); }

bool checkedAdd(std::uint64_t lhs, std::uint64_t rhs, std::uint64_t& result) {
    if (rhs > std::numeric_limits<std::uint64_t>::max() - lhs) {
        return false;
    }
    result = lhs + rhs;
    return true;
}

bool checkedMultiply(std::uint64_t lhs, std::uint64_t rhs, std::uint64_t& result) {
    if (lhs != 0 && rhs > std::numeric_limits<std::uint64_t>::max() / lhs) {
        return false;
    }
    result = lhs * rhs;
    return true;
}

std::uint64_t updateChecksum(std::uint64_t checksum, const char* bytes, std::size_t size) {
    for (std::size_t index = 0; index < size; ++index) {
        checksum ^= static_cast<unsigned char>(bytes[index]);
        checksum *= kChecksumPrime;
    }
    return checksum;
}

void writeUint16LE(char* bytes, std::uint16_t value) {
    bytes[0] = static_cast<char>(value & 0xffu);
    bytes[1] = static_cast<char>((value >> 8u) & 0xffu);
}

void writeUint32LE(char* bytes, std::uint32_t value) {
    for (unsigned int i = 0; i < 4u; ++i) {
        bytes[i] = static_cast<char>((value >> (i * 8u)) & 0xffu);
    }
}

void writeUint64LE(char* bytes, std::uint64_t value) {
    for (unsigned int i = 0; i < 8u; ++i) {
        bytes[i] = static_cast<char>((value >> (i * 8u)) & 0xffu);
    }
}

std::uint16_t readUint16LE(const char* bytes) {
    const auto* data = reinterpret_cast<const unsigned char*>(bytes);
    return static_cast<std::uint16_t>(data[0]) | static_cast<std::uint16_t>(data[1] << 8u);
}

std::uint32_t readUint32LE(const char* bytes) {
    const auto* data = reinterpret_cast<const unsigned char*>(bytes);
    return static_cast<std::uint32_t>(data[0]) | (static_cast<std::uint32_t>(data[1]) << 8u) |
           (static_cast<std::uint32_t>(data[2]) << 16u) | (static_cast<std::uint32_t>(data[3]) << 24u);
}

std::uint64_t readUint64LE(const char* bytes) {
    const auto* data = reinterpret_cast<const unsigned char*>(bytes);
    std::uint64_t value = 0;
    for (unsigned int i = 0; i < 8u; ++i) {
        value |= static_cast<std::uint64_t>(data[i]) << (i * 8u);
    }
    return value;
}

void writeFloatLE(char* bytes, float value) {
    static_assert(sizeof(float) == sizeof(std::uint32_t), "Binary STL requires 32-bit floats.");
    static_assert(std::numeric_limits<float>::is_iec559, "Binary STL requires IEEE-754 floats.");
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    writeUint32LE(bytes, bits);
}

float readFloatLE(const char* bytes) {
    const std::uint32_t bits = readUint32LE(bytes);
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

void writeDoubleLE(char* bytes, double value) {
    static_assert(sizeof(double) == sizeof(std::uint64_t), "Dataset format requires 64-bit doubles.");
    static_assert(std::numeric_limits<double>::is_iec559, "Dataset format requires IEEE-754 doubles.");
    std::uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    writeUint64LE(bytes, bits);
}

double readDoubleLE(const char* bytes) {
    const std::uint64_t bits = readUint64LE(bytes);
    double value = 0.0;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

bool finiteTriangle(const BinaryStlTriangleRecord& triangle) {
    for (float value : triangle.normal) {
        if (!std::isfinite(value)) {
            return false;
        }
    }
    for (const std::array<float, 3>& vertex : triangle.vertices) {
        for (float value : vertex) {
            if (!std::isfinite(value)) {
                return false;
            }
        }
    }
    return true;
}

std::array<char, kTriangleRecordBytes> encodeTriangle(const BinaryStlTriangleRecord& triangle) {
    std::array<char, kTriangleRecordBytes> bytes{};
    for (std::size_t component = 0; component < 3u; ++component) {
        writeFloatLE(bytes.data() + component * 4u, triangle.normal[component]);
    }
    for (std::size_t vertex = 0; vertex < 3u; ++vertex) {
        for (std::size_t component = 0; component < 3u; ++component) {
            writeFloatLE(bytes.data() + 12u + vertex * 12u + component * 4u, triangle.vertices[vertex][component]);
        }
    }
    writeUint16LE(bytes.data() + 48u, triangle.attributeByteCount);
    return bytes;
}

BinaryStlTriangleRecord decodeTriangle(const char* bytes) {
    BinaryStlTriangleRecord triangle;
    for (std::size_t component = 0; component < 3u; ++component) {
        triangle.normal[component] = readFloatLE(bytes + component * 4u);
    }
    for (std::size_t vertex = 0; vertex < 3u; ++vertex) {
        for (std::size_t component = 0; component < 3u; ++component) {
            triangle.vertices[vertex][component] = readFloatLE(bytes + 12u + vertex * 12u + component * 4u);
        }
    }
    triangle.attributeByteCount = readUint16LE(bytes + 48u);
    return triangle;
}

std::array<char, kDatasetHeaderBytes>
encodeDatasetHeader(std::uint64_t triangles, std::uint64_t partitions, std::uint64_t directoryOffset) {
    std::array<char, kDatasetHeaderBytes> bytes{};
    std::copy(kDatasetMagic.begin(), kDatasetMagic.end(), bytes.begin());
    writeUint32LE(bytes.data() + 8u, kDatasetVersion);
    writeUint32LE(bytes.data() + 12u, static_cast<std::uint32_t>(kDatasetHeaderBytes));
    writeUint32LE(bytes.data() + 16u, static_cast<std::uint32_t>(kTriangleRecordBytes));
    writeUint32LE(bytes.data() + 20u, static_cast<std::uint32_t>(kDirectoryRecordBytes));
    writeUint64LE(bytes.data() + 24u, triangles);
    writeUint64LE(bytes.data() + 32u, partitions);
    writeUint64LE(bytes.data() + 40u, directoryOffset);
    return bytes;
}

std::array<char, kPartitionHeaderBytes> encodePartitionHeader(const MeshPartitionMetadata& metadata) {
    std::array<char, kPartitionHeaderBytes> bytes{};
    std::copy(kPartitionMagic.begin(), kPartitionMagic.end(), bytes.begin());
    writeUint64LE(bytes.data() + 8u, metadata.id);
    writeUint64LE(bytes.data() + 16u, metadata.firstTriangleId);
    writeUint64LE(bytes.data() + 24u, metadata.triangleCount);
    writeUint64LE(bytes.data() + 32u, metadata.payloadBytes);
    for (std::size_t axis = 0; axis < 3u; ++axis) {
        writeDoubleLE(bytes.data() + 40u + axis * 8u, metadata.boundsMin[axis]);
        writeDoubleLE(bytes.data() + 64u + axis * 8u, metadata.boundsMax[axis]);
    }
    writeUint64LE(bytes.data() + 88u, metadata.payloadChecksum);
    return bytes;
}

std::array<char, kDirectoryRecordBytes> encodeDirectoryRecord(const MeshPartitionMetadata& metadata) {
    std::array<char, kDirectoryRecordBytes> bytes{};
    writeUint64LE(bytes.data(), metadata.id);
    writeUint64LE(bytes.data() + 8u, metadata.firstTriangleId);
    writeUint64LE(bytes.data() + 16u, metadata.triangleCount);
    writeUint64LE(bytes.data() + 24u, metadata.payloadOffset);
    writeUint64LE(bytes.data() + 32u, metadata.payloadBytes);
    for (std::size_t axis = 0; axis < 3u; ++axis) {
        writeDoubleLE(bytes.data() + 40u + axis * 8u, metadata.boundsMin[axis]);
        writeDoubleLE(bytes.data() + 64u + axis * 8u, metadata.boundsMax[axis]);
    }
    writeUint64LE(bytes.data() + 88u, metadata.payloadChecksum);
    return bytes;
}

MeshPartitionMetadata decodeDirectoryRecord(const char* bytes) {
    MeshPartitionMetadata metadata;
    metadata.id = readUint64LE(bytes);
    metadata.firstTriangleId = readUint64LE(bytes + 8u);
    metadata.triangleCount = readUint64LE(bytes + 16u);
    metadata.payloadOffset = readUint64LE(bytes + 24u);
    metadata.payloadBytes = readUint64LE(bytes + 32u);
    for (std::size_t axis = 0; axis < 3u; ++axis) {
        metadata.boundsMin[axis] = readDoubleLE(bytes + 40u + axis * 8u);
        metadata.boundsMax[axis] = readDoubleLE(bytes + 64u + axis * 8u);
    }
    metadata.payloadChecksum = readUint64LE(bytes + 88u);
    return metadata;
}

bool pathsReferToSameExistingFile(const filesystem::path& first, const filesystem::path& second) {
    std::error_code ec;
    if (!filesystem::exists(second, ec) || ec) {
        return false;
    }
    ec.clear();
    return filesystem::equivalent(first, second, ec) && !ec;
}

bool metadataBoundsValid(const MeshPartitionMetadata& metadata) {
    for (std::size_t axis = 0; axis < 3u; ++axis) {
        if (!std::isfinite(metadata.boundsMin[axis]) || !std::isfinite(metadata.boundsMax[axis]) ||
            metadata.boundsMin[axis] > metadata.boundsMax[axis]) {
            return false;
        }
    }
    return true;
}

filesystem::path pathFromUtf8(const std::string& path) { return filesystem::u8path(path); }

filesystem::path temporaryOutputPath(const filesystem::path& outputPath) {
    static std::atomic<unsigned long long> sequence{0};
    const auto tick =
        static_cast<unsigned long long>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    const auto thread = static_cast<unsigned long long>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
    const auto ordinal = sequence.fetch_add(1, std::memory_order_relaxed);
#if defined(_WIN32)
    const auto process = static_cast<unsigned long long>(GetCurrentProcessId());
#else
    const auto process = static_cast<unsigned long long>(::getpid());
#endif
    return outputPath.parent_path() /
           (outputPath.filename().u8string() + ".manumesh-dataset-" + std::to_string(tick) + "-" +
            std::to_string(process) + "-" + std::to_string(thread) + "-" + std::to_string(ordinal) + ".tmp");
}

Status createExclusiveFile(const filesystem::path& path, bool& created) {
    created = false;
#if defined(_WIN32)
    const HANDLE handle = CreateFileW(
        path.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    if (handle == INVALID_HANDLE_VALUE) {
        return ioError(
            "Failed to exclusively create temporary partitioned dataset file (Windows error " +
            std::to_string(GetLastError()) + ")."
        );
    }
    created = true;
    if (!CloseHandle(handle)) {
        return ioError(
            "Failed to close exclusively created temporary partitioned dataset file (Windows error " +
            std::to_string(GetLastError()) + ")."
        );
    }
#else
    const int descriptor = ::open(path.c_str(), O_CREAT | O_EXCL | O_RDWR, 0600);
    if (descriptor < 0) {
        const int error = errno;
        return ioError("Failed to exclusively create temporary partitioned dataset file: " +
                       std::string(std::strerror(error)) + ".");
    }
    created = true;
    if (::close(descriptor) != 0) {
        const int error = errno;
        return ioError("Failed to close exclusively created temporary partitioned dataset file: " +
                       std::string(std::strerror(error)) + ".");
    }
#endif
    return Status::success();
}

Status replaceOutputFile(const filesystem::path& temporaryPath, const filesystem::path& outputPath) {
#if defined(_WIN32)
    if (MoveFileExW(temporaryPath.c_str(), outputPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) ==
        0) {
        return ioError(
            "Failed to atomically replace partitioned dataset (Windows error " + std::to_string(GetLastError()) + ")."
        );
    }
#else
    std::error_code ec;
    filesystem::rename(temporaryPath, outputPath, ec);
    if (ec) {
        return ioError("Failed to atomically replace partitioned dataset: " + ec.message());
    }
#endif
    return Status::success();
}

bool streamPosition(std::ostream& stream, std::uint64_t& position) {
    const std::streampos current = stream.tellp();
    if (current == std::streampos(-1)) {
        return false;
    }
    const std::streamoff offset = current - std::streampos(0);
    if (offset < 0) {
        return false;
    }
    position = static_cast<std::uint64_t>(offset);
    return true;
}

Status validateMemoryBudget(const LargeMeshMemoryBudget& memory) {
    if (memory.ioBufferBytes < kTriangleRecordBytes) {
        return invalidArgument("Large-mesh I/O buffer must hold at least one 50-byte triangle record.");
    }
    if (memory.ioBufferBytes > memory.maxResidentBytes) {
        return invalidArgument("Large-mesh I/O buffer exceeds the declared resident-memory budget.");
    }
    if (memory.ioBufferBytes > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        return invalidArgument("Large-mesh I/O buffer exceeds the addressable size range.");
    }
    if (memory.ioBufferBytes > static_cast<std::uint64_t>(std::numeric_limits<std::streamsize>::max())) {
        return invalidArgument("Large-mesh I/O buffer exceeds the stream-size range.");
    }
    return Status::success();
}

Status validatePartitionMetadata(
    const MeshPartitionMetadata& metadata,
    std::uint64_t expectedIndex,
    std::uint64_t totalTriangles,
    std::uint64_t directoryOffset
) {
    if (metadata.id != expectedIndex) {
        return ioError("Partition directory contains a non-sequential partition ID.");
    }
    if (metadata.triangleCount == 0 || metadata.triangleCount > std::numeric_limits<LocalTriangleIndex>::max()) {
        return ioError("Partition triangle count is outside the supported local-index range.");
    }
    std::uint64_t expectedPayloadBytes = 0;
    if (!checkedMultiply(metadata.triangleCount, kTriangleRecordBytes, expectedPayloadBytes) ||
        metadata.payloadBytes != expectedPayloadBytes) {
        return ioError("Partition payload size does not match its triangle count.");
    }
    std::uint64_t globalEnd = 0;
    if (!checkedAdd(metadata.firstTriangleId, metadata.triangleCount, globalEnd) || globalEnd > totalTriangles) {
        return ioError("Partition global triangle range exceeds the dataset triangle count.");
    }
    std::uint64_t payloadEnd = 0;
    if (metadata.payloadOffset < kDatasetHeaderBytes + kPartitionHeaderBytes ||
        !checkedAdd(metadata.payloadOffset, metadata.payloadBytes, payloadEnd) || payloadEnd > directoryOffset) {
        return ioError("Partition payload range is outside the dataset payload region.");
    }
    if (!metadataBoundsValid(metadata)) {
        return ioError("Partition directory contains invalid bounds.");
    }
    return Status::success();
}

} // namespace

Status validatePartitionedMeshConfig(const PartitionedMeshConfig& config) {
    const Status memoryStatus = validateMemoryBudget(config.memory);
    if (!memoryStatus.ok()) {
        return memoryStatus;
    }
    if (config.trianglesPerPartition == 0) {
        return invalidArgument("Partition triangle limit must be greater than zero.");
    }
    return Status::success();
}

struct PartitionedMeshWriter::Impl {
    filesystem::path outputPath;
    filesystem::path temporaryPath;
    filesystem::path indexPath;
    PartitionedMeshConfig config{};
    std::fstream output;
    std::ofstream index;
    std::vector<char> buffer;
    std::size_t bufferedBytes = 0;
    std::uint64_t totalTriangles = 0;
    std::uint64_t totalPartitions = 0;
    std::uint64_t currentHeaderOffset = 0;
    MeshPartitionMetadata current{};
    bool partitionOpen = false;
    bool complete = false;
    bool ownsTemporaryPath = false;
    bool ownsIndexPath = false;
    bool failed = false;
    Status failure{};

    explicit Impl(PartitionedMeshConfig requestedConfig)
        : config(std::move(requestedConfig)) {}

    ~Impl() { cleanupTemporaryFiles(); }

    void cleanupTemporaryFiles() {
        if (output.is_open()) {
            output.close();
        }
        if (index.is_open()) {
            index.close();
        }
        if (!complete) {
            std::error_code ignored;
            if (ownsTemporaryPath && !temporaryPath.empty()) {
                filesystem::remove(temporaryPath, ignored);
            }
            ignored.clear();
            if (ownsIndexPath && !indexPath.empty()) {
                filesystem::remove(indexPath, ignored);
            }
        }
    }

    Status rememberFailure(Status status) {
        if (!status.ok() && !failed) {
            failed = true;
            failure = status;
        }
        return status;
    }

    Status writableStatus() const {
        if (complete) {
            return invalidArgument("Partitioned dataset writer has already been finished.");
        }
        if (failed) {
            return failure;
        }
        return Status::success();
    }

    Status initialize(const std::string& path) {
        if (path.empty()) {
            return invalidArgument("Partitioned dataset output path is empty.");
        }
        outputPath = pathFromUtf8(path);
        if (!outputPath.parent_path().empty()) {
            std::error_code ec;
            filesystem::create_directories(outputPath.parent_path(), ec);
            if (ec) {
                return ioError("Failed to create partitioned dataset output directory: " + ec.message());
            }
        }
        temporaryPath = temporaryOutputPath(outputPath);
        indexPath = temporaryPath;
        indexPath += ".index";
        bool temporaryCreated = false;
        const Status temporaryCreateStatus = createExclusiveFile(temporaryPath, temporaryCreated);
        ownsTemporaryPath = temporaryCreated;
        if (!temporaryCreateStatus.ok()) {
            return temporaryCreateStatus;
        }
        bool indexCreated = false;
        const Status indexCreateStatus = createExclusiveFile(indexPath, indexCreated);
        ownsIndexPath = indexCreated;
        if (!indexCreateStatus.ok()) {
            return indexCreateStatus;
        }
        output.open(temporaryPath, std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);
        if (!output) {
            return ioError("Failed to create partitioned dataset temporary file.");
        }
        index.open(indexPath, std::ios::binary | std::ios::trunc);
        if (!index) {
            return ioError("Failed to create partitioned dataset directory sidecar.");
        }
        buffer.resize(static_cast<std::size_t>(config.memory.ioBufferBytes));
        const std::array<char, kDatasetHeaderBytes> header = encodeDatasetHeader(0, 0, 0);
        output.write(header.data(), static_cast<std::streamsize>(header.size()));
        if (!output) {
            return ioError("Failed to write partitioned dataset header.");
        }
        return Status::success();
    }

    Status flushBuffer() {
        if (bufferedBytes == 0) {
            return Status::success();
        }
        output.write(buffer.data(), static_cast<std::streamsize>(bufferedBytes));
        if (!output) {
            return rememberFailure(ioError("Failed while writing partition triangle payload."));
        }
        bufferedBytes = 0;
        return Status::success();
    }

    Status beginPartition() {
        if (totalPartitions == std::numeric_limits<std::uint64_t>::max()) {
            return rememberFailure(ioError(
                "Partition ID range is exhausted; dataset format cannot represent another partition."
            ));
        }
        if (!streamPosition(output, currentHeaderOffset)) {
            return rememberFailure(ioError("Failed to determine partition header offset."));
        }
        current = MeshPartitionMetadata{};
        current.id = totalPartitions;
        current.firstTriangleId = totalTriangles;
        current.payloadChecksum = kChecksumOffsetBasis;
        if (!checkedAdd(currentHeaderOffset, kPartitionHeaderBytes, current.payloadOffset)) {
            return rememberFailure(ioError(
                "Partition payload offset overflowed; dataset format cannot address another partition."
            ));
        }
        const std::array<char, kPartitionHeaderBytes> placeholder = encodePartitionHeader(current);
        output.write(placeholder.data(), static_cast<std::streamsize>(placeholder.size()));
        if (!output) {
            return rememberFailure(ioError("Failed to write partition header placeholder."));
        }
        partitionOpen = true;
        return Status::success();
    }

    void includeBounds(const BinaryStlTriangleRecord& triangle) {
        for (std::size_t vertexIndex = 0; vertexIndex < triangle.vertices.size(); ++vertexIndex) {
            const std::array<float, 3>& vertex = triangle.vertices[vertexIndex];
            for (std::size_t axis = 0; axis < 3u; ++axis) {
                const double coordinate = static_cast<double>(vertex[axis]);
                if (current.triangleCount == 0 && vertexIndex == 0u) {
                    current.boundsMin[axis] = coordinate;
                    current.boundsMax[axis] = coordinate;
                } else {
                    current.boundsMin[axis] = std::min(current.boundsMin[axis], coordinate);
                    current.boundsMax[axis] = std::max(current.boundsMax[axis], coordinate);
                }
            }
        }
    }

    Status closePartition() {
        if (!partitionOpen) {
            return Status::success();
        }
        const Status flushStatus = flushBuffer();
        if (!flushStatus.ok()) {
            return flushStatus;
        }
        if (current.triangleCount == 0) {
            return rememberFailure(ioError("Internal error: attempted to close an empty partition."));
        }
        if (!checkedMultiply(current.triangleCount, kTriangleRecordBytes, current.payloadBytes)) {
            return rememberFailure(ioError(
                "Partition payload byte count overflowed; dataset format cannot represent this partition."
            ));
        }
        std::uint64_t payloadEnd = 0;
        if (!streamPosition(output, payloadEnd)) {
            return rememberFailure(ioError("Failed to determine partition payload end offset."));
        }
        std::uint64_t expectedEnd = 0;
        if (!checkedAdd(current.payloadOffset, current.payloadBytes, expectedEnd) || payloadEnd != expectedEnd) {
            return rememberFailure(ioError("Partition payload length is inconsistent with its record count."));
        }
        const std::array<char, kPartitionHeaderBytes> header = encodePartitionHeader(current);
        output.seekp(static_cast<std::streamoff>(currentHeaderOffset), std::ios::beg);
        output.write(header.data(), static_cast<std::streamsize>(header.size()));
        output.seekp(static_cast<std::streamoff>(payloadEnd), std::ios::beg);
        if (!output) {
            return rememberFailure(ioError("Failed to finalize partition header."));
        }
        const std::array<char, kDirectoryRecordBytes> directoryRecord = encodeDirectoryRecord(current);
        index.write(directoryRecord.data(), static_cast<std::streamsize>(directoryRecord.size()));
        if (!index) {
            return rememberFailure(ioError("Failed to stage partition directory entry."));
        }
        ++totalPartitions;
        partitionOpen = false;
        return Status::success();
    }

    Status append(const BinaryStlTriangleRecord& triangle) {
        const Status state = writableStatus();
        if (!state.ok()) {
            return state;
        }
        if (!finiteTriangle(triangle)) {
            return invalidArgument("Triangle record contains a non-finite normal or vertex coordinate.");
        }
        if (totalTriangles == std::numeric_limits<std::uint64_t>::max()) {
            return rememberFailure(ioError(
                "Global triangle ID range is exhausted; dataset format cannot represent another triangle."
            ));
        }
        if (!partitionOpen) {
            const Status beginStatus = beginPartition();
            if (!beginStatus.ok()) {
                return beginStatus;
            }
        }
        if (buffer.size() - bufferedBytes < kTriangleRecordBytes) {
            const Status flushStatus = flushBuffer();
            if (!flushStatus.ok()) {
                return flushStatus;
            }
        }
        const std::array<char, kTriangleRecordBytes> bytes = encodeTriangle(triangle);
        current.payloadChecksum = updateChecksum(current.payloadChecksum, bytes.data(), bytes.size());
        std::copy(bytes.begin(), bytes.end(), buffer.begin() + static_cast<std::ptrdiff_t>(bufferedBytes));
        bufferedBytes += bytes.size();
        includeBounds(triangle);
        ++current.triangleCount;
        ++totalTriangles;
        if (current.triangleCount == config.trianglesPerPartition) {
            return closePartition();
        }
        return Status::success();
    }

    Status finishDataset() {
        if (complete) {
            return Status::success();
        }
        const Status state = writableStatus();
        if (!state.ok()) {
            return state;
        }
        const Status closeStatus = closePartition();
        if (!closeStatus.ok()) {
            return closeStatus;
        }
        index.flush();
        if (!index) {
            return rememberFailure(ioError("Failed to flush partition directory sidecar."));
        }
        index.close();
        if (index.fail()) {
            return rememberFailure(ioError("Failed to close partition directory sidecar."));
        }

        std::uint64_t directoryOffset = 0;
        if (!streamPosition(output, directoryOffset)) {
            return rememberFailure(ioError("Failed to determine partition directory offset."));
        }
        std::array<char, kDirectoryHeaderBytes> directoryHeader{};
        std::copy(kDirectoryMagic.begin(), kDirectoryMagic.end(), directoryHeader.begin());
        writeUint64LE(directoryHeader.data() + 8u, totalPartitions);
        output.write(directoryHeader.data(), static_cast<std::streamsize>(directoryHeader.size()));
        if (!output) {
            return rememberFailure(ioError("Failed to write partition directory header."));
        }

        std::ifstream stagedIndex(indexPath, std::ios::binary);
        if (!stagedIndex) {
            return rememberFailure(ioError("Failed to reopen partition directory sidecar."));
        }
        while (stagedIndex) {
            stagedIndex.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
            const std::streamsize bytesRead = stagedIndex.gcount();
            if (bytesRead > 0) {
                output.write(buffer.data(), bytesRead);
                if (!output) {
                    return rememberFailure(ioError("Failed while copying partition directory."));
                }
            }
        }
        if (!stagedIndex.eof()) {
            return rememberFailure(ioError("Failed while reading partition directory sidecar."));
        }
        // Reading the final chunk sets eofbit (and, for an exact-end read, failbit).
        // Clear those read-state bits before checking whether close itself fails.
        stagedIndex.clear();
        stagedIndex.close();
        if (stagedIndex.fail()) {
            return rememberFailure(ioError("Failed to close partition directory sidecar."));
        }

        std::uint64_t expectedDirectoryBytes = 0;
        std::uint64_t expectedEnd = 0;
        std::uint64_t actualEnd = 0;
        if (!checkedMultiply(totalPartitions, kDirectoryRecordBytes, expectedDirectoryBytes) ||
            !checkedAdd(directoryOffset, kDirectoryHeaderBytes, expectedEnd) ||
            !checkedAdd(expectedEnd, expectedDirectoryBytes, expectedEnd) || !streamPosition(output, actualEnd) ||
            expectedEnd != actualEnd) {
            return rememberFailure(ioError("Partition directory byte count is inconsistent."));
        }

        const std::array<char, kDatasetHeaderBytes> header =
            encodeDatasetHeader(totalTriangles, totalPartitions, directoryOffset);
        output.seekp(0, std::ios::beg);
        output.write(header.data(), static_cast<std::streamsize>(header.size()));
        output.flush();
        if (!output) {
            return rememberFailure(ioError("Failed to finalize partitioned dataset header."));
        }
        output.close();
        if (output.fail()) {
            return rememberFailure(ioError("Failed to close partitioned dataset temporary file."));
        }

        // Remove the staging sidecar before publishing the dataset. If cleanup
        // cannot complete, fail while the destination is still untouched so the
        // transactional writer contract remains true.
        std::error_code cleanupError;
        const bool sidecarRemoved = filesystem::remove(indexPath, cleanupError);
        if (cleanupError || !sidecarRemoved) {
            return rememberFailure(ioError(
                "Failed to remove partitioned dataset directory sidecar before commit" +
                (cleanupError ? std::string(": ") + cleanupError.message() : std::string("."))
            ));
        }
        ownsIndexPath = false;

        const Status replaceStatus = replaceOutputFile(temporaryPath, outputPath);
        if (!replaceStatus.ok()) {
            return rememberFailure(replaceStatus);
        }
        ownsTemporaryPath = false;
        complete = true;
        return Status::success();
    }
};

PartitionedMeshWriter::PartitionedMeshWriter(std::unique_ptr<Impl> impl)
    : impl_(std::move(impl)) {}

Result<PartitionedMeshWriter>
PartitionedMeshWriter::create(const std::string& path, const PartitionedMeshConfig& config) {
    const Status configStatus = validatePartitionedMeshConfig(config);
    if (!configStatus.ok()) {
        return Result<PartitionedMeshWriter>(configStatus);
    }
    try {
        std::unique_ptr<Impl> impl(new Impl(config));
        const Status initStatus = impl->initialize(path);
        if (!initStatus.ok()) {
            return Result<PartitionedMeshWriter>(initStatus);
        }
        return Result<PartitionedMeshWriter>(PartitionedMeshWriter(std::move(impl)));
    } catch (const std::bad_alloc&) {
        return Result<PartitionedMeshWriter>(outOfMemory("Failed to allocate partitioned dataset I/O buffer."));
    } catch (const std::exception& exception) {
        return Result<PartitionedMeshWriter>(
            ioError("Failed to create partitioned dataset: " + std::string(exception.what()))
        );
    }
}

PartitionedMeshWriter::~PartitionedMeshWriter() = default;
PartitionedMeshWriter::PartitionedMeshWriter(PartitionedMeshWriter&& other) noexcept = default;
PartitionedMeshWriter& PartitionedMeshWriter::operator=(PartitionedMeshWriter&& other) noexcept = default;

Status PartitionedMeshWriter::appendTriangle(const BinaryStlTriangleRecord& triangle) {
    if (!impl_) {
        return invalidArgument("Partitioned dataset writer is moved from.");
    }
    return impl_->append(triangle);
}

Status PartitionedMeshWriter::endPartition() {
    if (!impl_) {
        return invalidArgument("Partitioned dataset writer is moved from.");
    }
    const Status state = impl_->writableStatus();
    return state.ok() ? impl_->closePartition() : state;
}

Status PartitionedMeshWriter::finish() {
    if (!impl_) {
        return invalidArgument("Partitioned dataset writer is moved from.");
    }
    return impl_->finishDataset();
}

std::uint64_t PartitionedMeshWriter::triangleCount() const { return impl_ ? impl_->totalTriangles : 0; }
std::uint64_t PartitionedMeshWriter::partitionCount() const { return impl_ ? impl_->totalPartitions : 0; }
bool PartitionedMeshWriter::finished() const { return impl_ && impl_->complete; }

struct PartitionedMeshReader::Impl {
    filesystem::path path;
    mutable std::ifstream directoryInput;
    std::ifstream payloadInput;
    std::vector<char> buffer;
    std::size_t bufferedOffset = 0;
    std::size_t bufferedBytes = 0;
    std::uint64_t fileBytes = 0;
    std::uint64_t totalTriangles = 0;
    std::uint64_t totalPartitions = 0;
    std::uint64_t directoryOffset = 0;
    std::uint64_t activeRemaining = 0;
    std::uint64_t expectedPayloadChecksum = 0;
    std::uint64_t activePayloadChecksum = kChecksumOffsetBasis;
    bool partitionActive = false;
    Status readFailure{};

    Status rememberReadFailure(Status status) {
        if (!status.ok() && readFailure.ok()) {
            readFailure = status;
            partitionActive = false;
        }
        return status;
    }

    Status initialize(const std::string& requestedPath, const LargeMeshMemoryBudget& memory) {
        if (requestedPath.empty()) {
            return invalidArgument("Partitioned dataset input path is empty.");
        }
        const Status memoryStatus = validateMemoryBudget(memory);
        if (!memoryStatus.ok()) {
            return memoryStatus;
        }
        path = pathFromUtf8(requestedPath);
        std::error_code ec;
        const std::uintmax_t measuredBytes = filesystem::file_size(path, ec);
        if (ec || measuredBytes > std::numeric_limits<std::uint64_t>::max()) {
            return ioError("Failed to determine partitioned dataset file size.");
        }
        fileBytes = static_cast<std::uint64_t>(measuredBytes);
        if (fileBytes > static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max())) {
            return ioError("Partitioned dataset exceeds the supported stream-offset range.");
        }
        if (fileBytes < kDatasetHeaderBytes + kDirectoryHeaderBytes) {
            return ioError("Partitioned dataset is too small to contain a header and directory.");
        }
        directoryInput.open(path, std::ios::binary);
        payloadInput.open(path, std::ios::binary);
        if (!directoryInput || !payloadInput) {
            return ioError("Failed to open partitioned dataset.");
        }
        std::array<char, kDatasetHeaderBytes> header{};
        directoryInput.read(header.data(), static_cast<std::streamsize>(header.size()));
        if (!directoryInput || !std::equal(kDatasetMagic.begin(), kDatasetMagic.end(), header.begin())) {
            return ioError("Partitioned dataset header magic is invalid or truncated.");
        }
        if (readUint32LE(header.data() + 8u) != kDatasetVersion ||
            readUint32LE(header.data() + 12u) != kDatasetHeaderBytes ||
            readUint32LE(header.data() + 16u) != kTriangleRecordBytes ||
            readUint32LE(header.data() + 20u) != kDirectoryRecordBytes) {
            return ioError("Partitioned dataset format version or record layout is unsupported.");
        }
        totalTriangles = readUint64LE(header.data() + 24u);
        totalPartitions = readUint64LE(header.data() + 32u);
        directoryOffset = readUint64LE(header.data() + 40u);

        std::uint64_t directoryRecordsBytes = 0;
        std::uint64_t directoryEnd = 0;
        if (directoryOffset < kDatasetHeaderBytes ||
            !checkedMultiply(totalPartitions, kDirectoryRecordBytes, directoryRecordsBytes) ||
            !checkedAdd(directoryOffset, kDirectoryHeaderBytes, directoryEnd) ||
            !checkedAdd(directoryEnd, directoryRecordsBytes, directoryEnd) || directoryEnd != fileBytes) {
            return ioError("Partitioned dataset directory range is invalid.");
        }
        if ((totalPartitions == 0) != (totalTriangles == 0)) {
            return ioError("Partitioned dataset has inconsistent empty counters.");
        }
        directoryInput.seekg(static_cast<std::streamoff>(directoryOffset), std::ios::beg);
        std::array<char, kDirectoryHeaderBytes> directoryHeader{};
        directoryInput.read(directoryHeader.data(), static_cast<std::streamsize>(directoryHeader.size()));
        if (!directoryInput || !std::equal(kDirectoryMagic.begin(), kDirectoryMagic.end(), directoryHeader.begin()) ||
            readUint64LE(directoryHeader.data() + 8u) != totalPartitions) {
            return ioError("Partitioned dataset directory header is invalid or truncated.");
        }
        const Status directoryStatus = validateCompleteDirectory();
        if (!directoryStatus.ok()) {
            return directoryStatus;
        }
        buffer.resize(static_cast<std::size_t>(memory.ioBufferBytes));
        return Status::success();
    }

    Status validateBlockHeader(const MeshPartitionMetadata& metadata) {
        const std::uint64_t blockHeaderOffset = metadata.payloadOffset - kPartitionHeaderBytes;
        payloadInput.clear();
        payloadInput.seekg(static_cast<std::streamoff>(blockHeaderOffset), std::ios::beg);
        std::array<char, kPartitionHeaderBytes> blockHeader{};
        payloadInput.read(blockHeader.data(), static_cast<std::streamsize>(blockHeader.size()));
        if (!payloadInput || !std::equal(kPartitionMagic.begin(), kPartitionMagic.end(), blockHeader.begin()) ||
            readUint64LE(blockHeader.data() + 8u) != metadata.id ||
            readUint64LE(blockHeader.data() + 16u) != metadata.firstTriangleId ||
            readUint64LE(blockHeader.data() + 24u) != metadata.triangleCount ||
            readUint64LE(blockHeader.data() + 32u) != metadata.payloadBytes ||
            readUint64LE(blockHeader.data() + 88u) != metadata.payloadChecksum) {
            return ioError("Partition block header does not match its directory entry.");
        }
        for (std::size_t axis = 0; axis < 3u; ++axis) {
            if (readDoubleLE(blockHeader.data() + 40u + axis * 8u) != metadata.boundsMin[axis] ||
                readDoubleLE(blockHeader.data() + 64u + axis * 8u) != metadata.boundsMax[axis]) {
                return ioError("Partition block bounds do not match its directory entry.");
            }
        }
        return Status::success();
    }

    Status validateCompleteDirectory() {
        std::uint64_t expectedFirstTriangle = 0;
        std::uint64_t expectedBlockOffset = kDatasetHeaderBytes;
        for (std::uint64_t partitionIndex = 0; partitionIndex < totalPartitions; ++partitionIndex) {
            std::uint64_t recordDelta = 0;
            std::uint64_t recordOffset = 0;
            if (!checkedMultiply(partitionIndex, kDirectoryRecordBytes, recordDelta) ||
                !checkedAdd(directoryOffset, kDirectoryHeaderBytes, recordOffset) ||
                !checkedAdd(recordOffset, recordDelta, recordOffset)) {
                return ioError("Partition directory offset overflowed.");
            }
            directoryInput.clear();
            directoryInput.seekg(static_cast<std::streamoff>(recordOffset), std::ios::beg);
            std::array<char, kDirectoryRecordBytes> bytes{};
            directoryInput.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
            if (!directoryInput) {
                return ioError("Partition directory is truncated.");
            }
            const MeshPartitionMetadata metadata = decodeDirectoryRecord(bytes.data());
            const Status metadataStatus =
                validatePartitionMetadata(metadata, partitionIndex, totalTriangles, directoryOffset);
            if (!metadataStatus.ok()) {
                return metadataStatus;
            }
            std::uint64_t expectedPayloadOffset = 0;
            if (metadata.firstTriangleId != expectedFirstTriangle ||
                !checkedAdd(expectedBlockOffset, kPartitionHeaderBytes, expectedPayloadOffset) ||
                metadata.payloadOffset != expectedPayloadOffset) {
                return ioError("Partition ranges are not contiguous in global-ID and payload order.");
            }
            const Status blockStatus = validateBlockHeader(metadata);
            if (!blockStatus.ok()) {
                return blockStatus;
            }
            if (!checkedAdd(expectedFirstTriangle, metadata.triangleCount, expectedFirstTriangle) ||
                !checkedAdd(metadata.payloadOffset, metadata.payloadBytes, expectedBlockOffset)) {
                return ioError("Partition range end overflowed.");
            }
        }
        if (expectedFirstTriangle != totalTriangles || expectedBlockOffset != directoryOffset) {
            return ioError("Partition directory does not exactly cover the dataset payload.");
        }
        return Status::success();
    }

    Status readMetadata(std::uint64_t partitionIndex, MeshPartitionMetadata& metadata) const {
        if (partitionIndex >= totalPartitions) {
            return invalidArgument("Partition index is outside the dataset directory.");
        }
        std::uint64_t recordDelta = 0;
        std::uint64_t recordOffset = 0;
        if (!checkedMultiply(partitionIndex, kDirectoryRecordBytes, recordDelta) ||
            !checkedAdd(directoryOffset, kDirectoryHeaderBytes, recordOffset) ||
            !checkedAdd(recordOffset, recordDelta, recordOffset)) {
            return ioError("Partition directory offset overflowed.");
        }
        directoryInput.clear();
        directoryInput.seekg(static_cast<std::streamoff>(recordOffset), std::ios::beg);
        std::array<char, kDirectoryRecordBytes> bytes{};
        directoryInput.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        if (!directoryInput) {
            return ioError("Failed to read partition directory entry.");
        }
        MeshPartitionMetadata decoded = decodeDirectoryRecord(bytes.data());
        const Status validation = validatePartitionMetadata(decoded, partitionIndex, totalTriangles, directoryOffset);
        if (!validation.ok()) {
            return validation;
        }
        metadata = decoded;
        return Status::success();
    }

    Status selectPartition(std::uint64_t partitionIndex) {
        if (!readFailure.ok()) {
            return readFailure;
        }
        if (partitionActive && activeRemaining != 0) {
            return invalidArgument(
                "The currently selected partition must be consumed before selecting another partition."
            );
        }
        MeshPartitionMetadata metadata;
        const Status metadataStatus = readMetadata(partitionIndex, metadata);
        if (!metadataStatus.ok()) {
            return metadataStatus;
        }

        const Status blockStatus = validateBlockHeader(metadata);
        if (!blockStatus.ok()) {
            return blockStatus;
        }

        payloadInput.clear();
        payloadInput.seekg(static_cast<std::streamoff>(metadata.payloadOffset), std::ios::beg);
        if (!payloadInput) {
            return ioError("Failed to seek to partition triangle payload.");
        }
        activeRemaining = metadata.triangleCount;
        expectedPayloadChecksum = metadata.payloadChecksum;
        activePayloadChecksum = kChecksumOffsetBasis;
        bufferedOffset = 0;
        bufferedBytes = 0;
        partitionActive = true;
        return Status::success();
    }

    Status readNext(BinaryStlTriangleRecord& triangle, bool& hasTriangle) {
        hasTriangle = false;
        if (!readFailure.ok()) {
            return readFailure;
        }
        if (!partitionActive) {
            return invalidArgument("No partition is selected for sequential reading.");
        }
        if (activeRemaining == 0) {
            return Status::success();
        }
        if (bufferedBytes - bufferedOffset < kTriangleRecordBytes) {
            const std::uint64_t bufferRecords = buffer.size() / kTriangleRecordBytes;
            const std::uint64_t recordsToRead = std::min(activeRemaining, bufferRecords);
            const std::size_t bytesToRead = static_cast<std::size_t>(recordsToRead * kTriangleRecordBytes);
            payloadInput.read(buffer.data(), static_cast<std::streamsize>(bytesToRead));
            if (!payloadInput) {
                return rememberReadFailure(ioError("Partition triangle payload is truncated."));
            }
            bufferedOffset = 0;
            bufferedBytes = bytesToRead;
        }
        const char* encoded = buffer.data() + bufferedOffset;
        BinaryStlTriangleRecord decoded = decodeTriangle(encoded);
        if (!finiteTriangle(decoded)) {
            return rememberReadFailure(ioError("Partition triangle payload contains a non-finite value."));
        }
        activePayloadChecksum = updateChecksum(activePayloadChecksum, encoded, kTriangleRecordBytes);
        bufferedOffset += kTriangleRecordBytes;
        --activeRemaining;
        if (activeRemaining == 0 && activePayloadChecksum != expectedPayloadChecksum) {
            return rememberReadFailure(ioError("Partition triangle payload checksum does not match its directory entry."));
        }
        triangle = decoded;
        hasTriangle = true;
        return Status::success();
    }
};

PartitionedMeshReader::PartitionedMeshReader(std::unique_ptr<Impl> impl)
    : impl_(std::move(impl)) {}

Result<PartitionedMeshReader>
PartitionedMeshReader::open(const std::string& path, const LargeMeshMemoryBudget& memory) {
    try {
        std::unique_ptr<Impl> impl(new Impl());
        const Status initStatus = impl->initialize(path, memory);
        if (!initStatus.ok()) {
            return Result<PartitionedMeshReader>(initStatus);
        }
        return Result<PartitionedMeshReader>(PartitionedMeshReader(std::move(impl)));
    } catch (const std::bad_alloc&) {
        return Result<PartitionedMeshReader>(outOfMemory("Failed to allocate partitioned dataset read buffer."));
    } catch (const std::exception& exception) {
        return Result<PartitionedMeshReader>(
            ioError("Failed to open partitioned dataset: " + std::string(exception.what()))
        );
    }
}

PartitionedMeshReader::~PartitionedMeshReader() = default;
PartitionedMeshReader::PartitionedMeshReader(PartitionedMeshReader&& other) noexcept = default;
PartitionedMeshReader& PartitionedMeshReader::operator=(PartitionedMeshReader&& other) noexcept = default;

std::uint64_t PartitionedMeshReader::triangleCount() const { return impl_ ? impl_->totalTriangles : 0; }
std::uint64_t PartitionedMeshReader::partitionCount() const { return impl_ ? impl_->totalPartitions : 0; }

Status PartitionedMeshReader::partitionMetadata(std::uint64_t partitionIndex, MeshPartitionMetadata& metadata) const {
    if (!impl_) {
        return invalidArgument("Partitioned dataset reader is moved from.");
    }
    return impl_->readMetadata(partitionIndex, metadata);
}

Status PartitionedMeshReader::beginPartition(std::uint64_t partitionIndex) {
    if (!impl_) {
        return invalidArgument("Partitioned dataset reader is moved from.");
    }
    return impl_->selectPartition(partitionIndex);
}

Status PartitionedMeshReader::readNextTriangle(BinaryStlTriangleRecord& triangle, bool& hasTriangle) {
    if (!impl_) {
        hasTriangle = false;
        return invalidArgument("Partitioned dataset reader is moved from.");
    }
    return impl_->readNext(triangle, hasTriangle);
}

Status importBinaryStlToPartitionedMesh(
    const std::string& stlPath,
    const std::string& datasetPath,
    const PartitionedMeshConfig& config,
    PartitionedMeshSummary* summary
) {
    if (summary) {
        *summary = PartitionedMeshSummary{};
    }
    try {
        const Status configStatus = validatePartitionedMeshConfig(config);
        if (!configStatus.ok()) {
            return configStatus;
        }
        if (stlPath.empty()) {
            return invalidArgument("Binary STL input path is empty.");
        }

        std::uint64_t inputBytes = 0;
        std::uint32_t declaredTriangles = 0;
        std::array<char, 84> stlHeader{};
        const filesystem::path inputPath = pathFromUtf8(stlPath);
        const filesystem::path outputPath = pathFromUtf8(datasetPath);
        if (pathsReferToSameExistingFile(inputPath, outputPath)) {
            return invalidArgument("Binary STL input and partitioned dataset output refer to the same file.");
        }
        std::error_code ec;
        const std::uintmax_t measuredBytes = filesystem::file_size(inputPath, ec);
        if (ec || measuredBytes > std::numeric_limits<std::uint64_t>::max()) {
            return ioError("Failed to determine binary STL file size.");
        }
        inputBytes = static_cast<std::uint64_t>(measuredBytes);
        if (inputBytes < stlHeader.size()) {
            return ioError("Binary STL is too small to contain its 84-byte header.");
        }

        std::ifstream input(inputPath, std::ios::binary);
        if (!input) {
            return ioError("Failed to open binary STL input.");
        }
        input.read(stlHeader.data(), static_cast<std::streamsize>(stlHeader.size()));
        if (!input) {
            return ioError("Failed to read binary STL header.");
        }
        declaredTriangles = readUint32LE(stlHeader.data() + 80u);
        std::uint64_t payloadBytes = 0;
        std::uint64_t requiredBytes = 0;
        if (!checkedMultiply(declaredTriangles, kTriangleRecordBytes, payloadBytes) ||
            !checkedAdd(stlHeader.size(), payloadBytes, requiredBytes) || inputBytes < requiredBytes) {
            return ioError("Truncated binary STL: declared triangle records exceed the available file bytes.");
        }

        Result<PartitionedMeshWriter> writerResult = PartitionedMeshWriter::create(datasetPath, config);
        if (!writerResult.ok()) {
            return writerResult.status();
        }
        PartitionedMeshWriter writer = std::move(writerResult.value());

        // The writer owns one configured output buffer. Use only the remaining
        // declared budget for the input side, and make the chunk an integral
        // number of fixed-width STL records so decoding never needs a carry
        // buffer. Very small budgets retain the one-record fallback.
        const std::uint64_t inputBudget = config.memory.maxResidentBytes > config.memory.ioBufferBytes
                                              ? config.memory.maxResidentBytes - config.memory.ioBufferBytes
                                              : 0;
        std::uint64_t requestedInputBytes = std::min(config.memory.ioBufferBytes, inputBudget);
        requestedInputBytes -= requestedInputBytes % kTriangleRecordBytes;
        std::vector<char> inputBuffer;
        if (requestedInputBytes >= kTriangleRecordBytes) {
            inputBuffer.resize(static_cast<std::size_t>(requestedInputBytes));
        }

        auto appendEncodedTriangle = [&](const char* encoded) -> Status {
            const BinaryStlTriangleRecord triangle = decodeTriangle(encoded);
            return writer.appendTriangle(triangle);
        };

        if (inputBuffer.empty()) {
            std::array<char, kTriangleRecordBytes> recordBytes{};
            for (std::uint64_t index = 0; index < declaredTriangles; ++index) {
                input.read(recordBytes.data(), static_cast<std::streamsize>(recordBytes.size()));
                if (!input) {
                    return ioError("Unexpected end of binary STL triangle payload.");
                }
                const Status appendStatus = appendEncodedTriangle(recordBytes.data());
                if (!appendStatus.ok()) {
                    return appendStatus;
                }
            }
        } else {
            const std::size_t recordsPerChunk = inputBuffer.size() / kTriangleRecordBytes;
            std::uint64_t index = 0;
            while (index < declaredTriangles) {
                const std::uint64_t remaining = declaredTriangles - index;
                const std::size_t recordsToRead = static_cast<std::size_t>(
                    std::min<std::uint64_t>(remaining, static_cast<std::uint64_t>(recordsPerChunk))
                );
                const std::size_t bytesToRead = recordsToRead * kTriangleRecordBytes;
                input.read(inputBuffer.data(), static_cast<std::streamsize>(bytesToRead));
                if (!input || static_cast<std::size_t>(input.gcount()) != bytesToRead) {
                    return ioError("Unexpected end of binary STL triangle payload.");
                }
                for (std::size_t record = 0; record < recordsToRead; ++record) {
                    const Status appendStatus =
                        appendEncodedTriangle(inputBuffer.data() + record * kTriangleRecordBytes);
                    if (!appendStatus.ok()) {
                        return appendStatus;
                    }
                }
                index += recordsToRead;
            }
        }
        input.close();
        if (input.fail()) {
            return ioError("Failed to close binary STL input.");
        }

        const Status finishStatus = writer.finish();
        if (!finishStatus.ok()) {
            return finishStatus;
        }
        if (summary) {
            summary->triangleCount = writer.triangleCount();
            summary->partitionCount = writer.partitionCount();
            summary->sourceBytes = inputBytes;
        }
        return Status::success();
    } catch (const std::bad_alloc&) {
        return outOfMemory("Failed to allocate memory while importing binary STL.");
    } catch (const std::exception& exception) {
        return ioError("Failed to import binary STL: " + std::string(exception.what()));
    }
}

} // namespace manumesh
