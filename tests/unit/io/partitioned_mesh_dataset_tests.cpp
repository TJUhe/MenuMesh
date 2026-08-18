#include "io/PartitionedMeshDataset.h"

#include "core/Filesystem.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace {

unsigned long long currentProcessId() {
#if defined(_WIN32)
    return static_cast<unsigned long long>(::GetCurrentProcessId());
#else
    return static_cast<unsigned long long>(::getpid());
#endif
}

manumesh::filesystem::path tempPath(const std::string& suffix) {
    static std::atomic<unsigned long long> sequence{0};
    return manumesh::filesystem::temp_directory_path() /
           ("manumesh_partitioned_dataset_" + std::to_string(currentProcessId()) + "_" +
            std::to_string(sequence.fetch_add(1)) + suffix);
}

struct ScopedRemoval {
    explicit ScopedRemoval(manumesh::filesystem::path requestedPath)
        : path(std::move(requestedPath)) {}

    ~ScopedRemoval() {
        std::error_code ignored;
        manumesh::filesystem::remove(path, ignored);
    }

    manumesh::filesystem::path path;
};

void writeUint16LE(char* bytes, std::uint16_t value) {
    bytes[0] = static_cast<char>(value & 0xffu);
    bytes[1] = static_cast<char>((value >> 8u) & 0xffu);
}

void writeUint32LE(char* bytes, std::uint32_t value) {
    for (unsigned int i = 0; i < 4u; ++i) {
        bytes[i] = static_cast<char>((value >> (i * 8u)) & 0xffu);
    }
}

void writeFloatLE(char* bytes, float value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    writeUint32LE(bytes, bits);
}

std::array<char, 50> encodeStlRecord(const manumesh::BinaryStlTriangleRecord& triangle) {
    std::array<char, 50> bytes{};
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

manumesh::BinaryStlTriangleRecord triangleRecord(int index) {
    const float base = static_cast<float>(index);
    manumesh::BinaryStlTriangleRecord triangle;
    triangle.normal = {{base + 0.1f, base + 0.2f, base + 0.3f}};
    triangle.vertices = {{
        {{base, -base, base * 2.0f}},
        {{base + 0.25f, -base + 0.5f, base * 2.0f + 0.75f}},
        {{base - 0.5f, -base - 0.25f, base * 2.0f + 0.125f}},
    }};
    triangle.attributeByteCount = static_cast<std::uint16_t>(index * 7);
    return triangle;
}

void expectTriangleEqual(
    const manumesh::BinaryStlTriangleRecord& expected, const manumesh::BinaryStlTriangleRecord& actual
) {
    EXPECT_EQ(actual.normal, expected.normal);
    EXPECT_EQ(actual.vertices, expected.vertices);
    EXPECT_EQ(actual.attributeByteCount, expected.attributeByteCount);
}

void writeBinaryStl(
    const manumesh::filesystem::path& path,
    const std::vector<manumesh::BinaryStlTriangleRecord>& triangles,
    std::uint32_t declaredTriangles,
    std::size_t paddingBytes = 0
) {
    std::array<char, 84> header{};
    const char label[] = "ManuMesh partitioned dataset test";
    std::copy(label, label + sizeof(label) - 1, header.begin());
    writeUint32LE(header.data() + 80u, declaredTriangles);
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    ASSERT_TRUE(output);
    output.write(header.data(), static_cast<std::streamsize>(header.size()));
    for (const manumesh::BinaryStlTriangleRecord& triangle : triangles) {
        const std::array<char, 50> bytes = encodeStlRecord(triangle);
        output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }
    const std::vector<char> padding(paddingBytes, static_cast<char>(0xa5));
    output.write(padding.data(), static_cast<std::streamsize>(padding.size()));
    ASSERT_TRUE(output);
}

std::string readFile(const manumesh::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

manumesh::PartitionedMeshConfig tinyStreamingConfig(std::uint32_t trianglesPerPartition) {
    manumesh::PartitionedMeshConfig config;
    config.memory.maxResidentBytes = 100u;
    config.memory.ioBufferBytes = 100u;
    config.trianglesPerPartition = trianglesPerPartition;
    return config;
}

} // namespace

TEST(ManuMeshPartitionedDataset, RejectsInvalidMemoryAndPartitionBudgets) {
    manumesh::PartitionedMeshConfig config;
    config.memory.maxResidentBytes = 49u;
    config.memory.ioBufferBytes = 49u;
    EXPECT_EQ(manumesh::validatePartitionedMeshConfig(config).code(), manumesh::StatusCode::InvalidArgument);

    config.memory.maxResidentBytes = 50u;
    config.memory.ioBufferBytes = 51u;
    EXPECT_EQ(manumesh::validatePartitionedMeshConfig(config).code(), manumesh::StatusCode::InvalidArgument);

    config.memory.ioBufferBytes = 50u;
    config.trianglesPerPartition = 0;
    EXPECT_EQ(manumesh::validatePartitionedMeshConfig(config).code(), manumesh::StatusCode::InvalidArgument);
}

TEST(ManuMeshPartitionedDataset, StreamsAutomaticPartitionsAndReadsThemRandomly) {
    const manumesh::filesystem::path path = tempPath("_roundtrip.mmpd");
    const ScopedRemoval cleanup(path);
    manumesh::filesystem::remove(path);
    const manumesh::PartitionedMeshConfig config = tinyStreamingConfig(3u);

    auto writerResult = manumesh::PartitionedMeshWriter::create(path.u8string(), config);
    ASSERT_TRUE(writerResult.ok()) << writerResult.status().message();
    manumesh::PartitionedMeshWriter writer = std::move(writerResult.value());
    std::vector<manumesh::BinaryStlTriangleRecord> expected;
    for (int index = 0; index < 7; ++index) {
        expected.push_back(triangleRecord(index));
        ASSERT_TRUE(writer.appendTriangle(expected.back()).ok());
    }
    ASSERT_TRUE(writer.finish().ok());
    EXPECT_TRUE(writer.finished());
    EXPECT_EQ(writer.triangleCount(), 7u);
    EXPECT_EQ(writer.partitionCount(), 3u);
    EXPECT_TRUE(writer.finish().ok());

    auto readerResult = manumesh::PartitionedMeshReader::open(path.u8string(), config.memory);
    ASSERT_TRUE(readerResult.ok()) << readerResult.status().message();
    manumesh::PartitionedMeshReader reader = std::move(readerResult.value());
    EXPECT_EQ(reader.triangleCount(), 7u);
    EXPECT_EQ(reader.partitionCount(), 3u);

    manumesh::BinaryStlTriangleRecord record;
    bool hasTriangle = true;
    EXPECT_EQ(reader.readNextTriangle(record, hasTriangle).code(), manumesh::StatusCode::InvalidArgument);
    EXPECT_FALSE(hasTriangle);

    const std::array<std::uint64_t, 3> order{{2u, 0u, 1u}};
    for (std::uint64_t partitionIndex : order) {
        manumesh::MeshPartitionMetadata metadata;
        ASSERT_TRUE(reader.partitionMetadata(partitionIndex, metadata).ok());
        EXPECT_EQ(metadata.id, partitionIndex);
        EXPECT_EQ(metadata.firstTriangleId, partitionIndex * 3u);
        EXPECT_EQ(metadata.triangleCount, partitionIndex == 2u ? 1u : 3u);
        EXPECT_EQ(metadata.payloadBytes, metadata.triangleCount * 50u);
        ASSERT_TRUE(reader.beginPartition(partitionIndex).ok());
        for (std::uint64_t local = 0; local < metadata.triangleCount; ++local) {
            ASSERT_TRUE(reader.readNextTriangle(record, hasTriangle).ok());
            ASSERT_TRUE(hasTriangle);
            expectTriangleEqual(expected[static_cast<std::size_t>(metadata.firstTriangleId + local)], record);
        }
        ASSERT_TRUE(reader.readNextTriangle(record, hasTriangle).ok());
        EXPECT_FALSE(hasTriangle);
    }
    manumesh::MeshPartitionMetadata unused;
    EXPECT_EQ(reader.partitionMetadata(3u, unused).code(), manumesh::StatusCode::InvalidArgument);
}

TEST(ManuMeshPartitionedDataset, ManualPartitionBoundariesOwnContiguousGlobalRanges) {
    const manumesh::filesystem::path path = tempPath("_manual.mmpd");
    const ScopedRemoval cleanup(path);
    manumesh::filesystem::remove(path);
    auto writerResult = manumesh::PartitionedMeshWriter::create(path.u8string(), tinyStreamingConfig(100u));
    ASSERT_TRUE(writerResult.ok()) << writerResult.status().message();
    manumesh::PartitionedMeshWriter writer = std::move(writerResult.value());
    ASSERT_TRUE(writer.endPartition().ok());
    ASSERT_TRUE(writer.appendTriangle(triangleRecord(0)).ok());
    ASSERT_TRUE(writer.appendTriangle(triangleRecord(1)).ok());
    ASSERT_TRUE(writer.endPartition().ok());
    ASSERT_TRUE(writer.appendTriangle(triangleRecord(2)).ok());
    ASSERT_TRUE(writer.finish().ok());

    auto readerResult = manumesh::PartitionedMeshReader::open(path.u8string(), tinyStreamingConfig(1u).memory);
    ASSERT_TRUE(readerResult.ok()) << readerResult.status().message();
    manumesh::PartitionedMeshReader reader = std::move(readerResult.value());
    ASSERT_EQ(reader.partitionCount(), 2u);
    manumesh::MeshPartitionMetadata first;
    manumesh::MeshPartitionMetadata second;
    ASSERT_TRUE(reader.partitionMetadata(0u, first).ok());
    ASSERT_TRUE(reader.partitionMetadata(1u, second).ok());
    EXPECT_EQ(first.firstTriangleId, 0u);
    EXPECT_EQ(first.triangleCount, 2u);
    EXPECT_EQ(second.firstTriangleId, 2u);
    EXPECT_EQ(second.triangleCount, 1u);
    EXPECT_DOUBLE_EQ(first.boundsMin[0], -0.5);
    EXPECT_DOUBLE_EQ(first.boundsMax[2], 2.75);
}

TEST(ManuMeshPartitionedDataset, ImportsPaddedBinaryStlWithoutBuildingAnInMemoryMesh) {
    const manumesh::filesystem::path stlPath = tempPath("_input.stl");
    const manumesh::filesystem::path datasetPath = tempPath("_import.mmpd");
    const ScopedRemoval stlCleanup(stlPath);
    const ScopedRemoval datasetCleanup(datasetPath);
    std::vector<manumesh::BinaryStlTriangleRecord> triangles;
    for (int index = 0; index < 5; ++index) {
        triangles.push_back(triangleRecord(index));
    }
    writeBinaryStl(stlPath, triangles, 5u, 13u);

    // Leave a two-record buffer for the input side as well as the writer's
    // output buffer, exercising the bounded batched-import path.
    manumesh::PartitionedMeshConfig importConfig = tinyStreamingConfig(2u);
    importConfig.memory.maxResidentBytes = 200u;
    importConfig.memory.ioBufferBytes = 100u;
    manumesh::PartitionedMeshSummary summary;
    const manumesh::Status status = manumesh::importBinaryStlToPartitionedMesh(
        stlPath.u8string(), datasetPath.u8string(), importConfig, &summary
    );
    ASSERT_TRUE(status.ok()) << status.message();
    EXPECT_EQ(summary.triangleCount, 5u);
    EXPECT_EQ(summary.partitionCount, 3u);
    EXPECT_EQ(summary.sourceBytes, 84u + 5u * 50u + 13u);

    auto readerResult = manumesh::PartitionedMeshReader::open(datasetPath.u8string(), tinyStreamingConfig(1u).memory);
    ASSERT_TRUE(readerResult.ok()) << readerResult.status().message();
    manumesh::PartitionedMeshReader reader = std::move(readerResult.value());
    std::size_t global = 0;
    for (std::uint64_t partition = 0; partition < reader.partitionCount(); ++partition) {
        ASSERT_TRUE(reader.beginPartition(partition).ok());
        bool hasTriangle = false;
        manumesh::BinaryStlTriangleRecord triangle;
        while (true) {
            ASSERT_TRUE(reader.readNextTriangle(triangle, hasTriangle).ok());
            if (!hasTriangle) {
                break;
            }
            ASSERT_LT(global, triangles.size());
            expectTriangleEqual(triangles[global], triangle);
            ++global;
        }
    }
    EXPECT_EQ(global, triangles.size());

    manumesh::PartitionedMeshValidationReport report;
    const manumesh::Status validationStatus = manumesh::validatePartitionedMeshDataset(
        datasetPath.u8string(), importConfig.memory, &report
    );
    ASSERT_TRUE(validationStatus.ok()) << validationStatus.message();
    EXPECT_EQ(report.triangleCount, 5u);
    EXPECT_EQ(report.partitionCount, 3u);
    EXPECT_EQ(report.degenerateTriangleCount, 0u);
    EXPECT_TRUE(report.hasBounds);
    EXPECT_GT(report.surfaceArea, 0.0);
}

TEST(ManuMeshPartitionedDataset, TruncatedOrNonFiniteStlDoesNotReplaceDestination) {
    const manumesh::filesystem::path stlPath = tempPath("_bad.stl");
    const manumesh::filesystem::path datasetPath = tempPath("_preserved.mmpd");
    const ScopedRemoval stlCleanup(stlPath);
    const ScopedRemoval datasetCleanup(datasetPath);
    const std::string sentinel = "preserve existing output";
    {
        std::ofstream output(datasetPath, std::ios::binary | std::ios::trunc);
        output << sentinel;
    }

    {
        auto abandonedResult = manumesh::PartitionedMeshWriter::create(datasetPath.u8string(), tinyStreamingConfig(1u));
        ASSERT_TRUE(abandonedResult.ok()) << abandonedResult.status().message();
        manumesh::PartitionedMeshWriter abandoned = std::move(abandonedResult.value());
        ASSERT_TRUE(abandoned.appendTriangle(triangleRecord(0)).ok());
    }
    EXPECT_EQ(readFile(datasetPath), sentinel);

    writeBinaryStl(stlPath, {triangleRecord(0)}, 2u);
    manumesh::Status status =
        manumesh::importBinaryStlToPartitionedMesh(stlPath.u8string(), datasetPath.u8string(), tinyStreamingConfig(1u));
    EXPECT_EQ(status.code(), manumesh::StatusCode::IoError);
    EXPECT_EQ(readFile(datasetPath), sentinel);

    manumesh::BinaryStlTriangleRecord invalid = triangleRecord(0);
    invalid.vertices[1][2] = std::numeric_limits<float>::infinity();
    writeBinaryStl(stlPath, {invalid}, 1u);
    status =
        manumesh::importBinaryStlToPartitionedMesh(stlPath.u8string(), datasetPath.u8string(), tinyStreamingConfig(1u));
    EXPECT_EQ(status.code(), manumesh::StatusCode::InvalidArgument);
    EXPECT_EQ(readFile(datasetPath), sentinel);
}

TEST(ManuMeshPartitionedDataset, ImportRejectsAnInputOutputAliasWithoutChangingTheStl) {
    const manumesh::filesystem::path stlPath = tempPath("_same_path.stl");
    const ScopedRemoval cleanup(stlPath);
    writeBinaryStl(stlPath, {triangleRecord(0)}, 1u);
    const std::string original = readFile(stlPath);

    const manumesh::Status status =
        manumesh::importBinaryStlToPartitionedMesh(stlPath.u8string(), stlPath.u8string(), tinyStreamingConfig(1u));
    EXPECT_EQ(status.code(), manumesh::StatusCode::InvalidArgument);
    EXPECT_EQ(readFile(stlPath), original);
}

TEST(ManuMeshPartitionedDataset, DetectsFinitePayloadCorruptionWhenThePartitionIsRead) {
    const manumesh::filesystem::path path = tempPath("_payload_corrupt.mmpd");
    const ScopedRemoval cleanup(path);
    const manumesh::PartitionedMeshConfig config = tinyStreamingConfig(1u);
    auto writerResult = manumesh::PartitionedMeshWriter::create(path.u8string(), config);
    ASSERT_TRUE(writerResult.ok()) << writerResult.status().message();
    manumesh::PartitionedMeshWriter writer = std::move(writerResult.value());
    ASSERT_TRUE(writer.appendTriangle(triangleRecord(0)).ok());
    ASSERT_TRUE(writer.finish().ok());

    {
        // Dataset header (64), partition header (96), then the first vertex x at record byte 12.
        std::fstream file(path, std::ios::binary | std::ios::in | std::ios::out);
        ASSERT_TRUE(file);
        constexpr std::streamoff corruptOffset = 64 + 96 + 12;
        file.seekg(corruptOffset, std::ios::beg);
        char value = 0;
        file.read(&value, 1);
        ASSERT_TRUE(file);
        value ^= 1;
        file.seekp(corruptOffset, std::ios::beg);
        file.write(&value, 1);
        ASSERT_TRUE(file);
    }

    auto readerResult = manumesh::PartitionedMeshReader::open(path.u8string(), config.memory);
    ASSERT_TRUE(readerResult.ok()) << readerResult.status().message();
    manumesh::PartitionedMeshReader reader = std::move(readerResult.value());
    ASSERT_TRUE(reader.beginPartition(0u).ok());
    manumesh::BinaryStlTriangleRecord triangle;
    bool hasTriangle = true;
    const manumesh::Status status = reader.readNextTriangle(triangle, hasTriangle);
    EXPECT_EQ(status.code(), manumesh::StatusCode::IoError);
    EXPECT_FALSE(hasTriangle);
    EXPECT_NE(status.message().find("checksum"), std::string::npos);
    const manumesh::Status repeatedStatus = reader.readNextTriangle(triangle, hasTriangle);
    EXPECT_EQ(repeatedStatus.code(), manumesh::StatusCode::IoError);
    EXPECT_FALSE(hasTriangle);
    EXPECT_EQ(reader.beginPartition(0u).code(), manumesh::StatusCode::IoError);
}

TEST(ManuMeshPartitionedDataset, RequiresASelectedPartitionToBeFullyConsumedBeforeSwitching) {
    const manumesh::filesystem::path path = tempPath("_consume_before_switch.mmpd");
    const ScopedRemoval cleanup(path);
    const manumesh::PartitionedMeshConfig config = tinyStreamingConfig(1u);
    auto writerResult = manumesh::PartitionedMeshWriter::create(path.u8string(), config);
    ASSERT_TRUE(writerResult.ok()) << writerResult.status().message();
    manumesh::PartitionedMeshWriter writer = std::move(writerResult.value());
    ASSERT_TRUE(writer.appendTriangle(triangleRecord(0)).ok());
    ASSERT_TRUE(writer.appendTriangle(triangleRecord(1)).ok());
    ASSERT_TRUE(writer.finish().ok());

    auto readerResult = manumesh::PartitionedMeshReader::open(path.u8string(), config.memory);
    ASSERT_TRUE(readerResult.ok()) << readerResult.status().message();
    manumesh::PartitionedMeshReader reader = std::move(readerResult.value());
    ASSERT_TRUE(reader.beginPartition(0u).ok());
    EXPECT_EQ(reader.beginPartition(1u).code(), manumesh::StatusCode::InvalidArgument);
}

TEST(ManuMeshPartitionedDataset, RejectsTrailingBytesAfterTheVersionedDirectory) {
    const manumesh::filesystem::path path = tempPath("_trailing_bytes.mmpd");
    const ScopedRemoval cleanup(path);
    const manumesh::PartitionedMeshConfig config = tinyStreamingConfig(1u);
    auto writerResult = manumesh::PartitionedMeshWriter::create(path.u8string(), config);
    ASSERT_TRUE(writerResult.ok()) << writerResult.status().message();
    manumesh::PartitionedMeshWriter writer = std::move(writerResult.value());
    ASSERT_TRUE(writer.appendTriangle(triangleRecord(0)).ok());
    ASSERT_TRUE(writer.finish().ok());
    {
        std::ofstream output(path, std::ios::binary | std::ios::app);
        ASSERT_TRUE(output);
        const char trailing = static_cast<char>(0x5a);
        output.write(&trailing, 1);
        ASSERT_TRUE(output);
    }

    const auto readerResult = manumesh::PartitionedMeshReader::open(path.u8string(), config.memory);
    EXPECT_FALSE(readerResult.ok());
    EXPECT_EQ(readerResult.status().code(), manumesh::StatusCode::IoError);
}

TEST(ManuMeshPartitionedDataset, DiskBackedDirectorySupportsManyPartitionsWithOneRecordBuffer) {
    const manumesh::filesystem::path path = tempPath("_many_partitions.mmpd");
    const ScopedRemoval cleanup(path);
    manumesh::PartitionedMeshConfig config = tinyStreamingConfig(1u);
    config.memory.maxResidentBytes = 50u;
    config.memory.ioBufferBytes = 50u;
    auto writerResult = manumesh::PartitionedMeshWriter::create(path.u8string(), config);
    ASSERT_TRUE(writerResult.ok()) << writerResult.status().message();
    manumesh::PartitionedMeshWriter writer = std::move(writerResult.value());
    for (int index = 0; index < 1000; ++index) {
        ASSERT_TRUE(writer.appendTriangle(triangleRecord(index)).ok());
    }
    ASSERT_TRUE(writer.finish().ok());
    EXPECT_EQ(writer.partitionCount(), 1000u);

    auto readerResult = manumesh::PartitionedMeshReader::open(path.u8string(), config.memory);
    ASSERT_TRUE(readerResult.ok()) << readerResult.status().message();
    manumesh::PartitionedMeshReader reader = std::move(readerResult.value());
    ASSERT_EQ(reader.partitionCount(), 1000u);
    manumesh::MeshPartitionMetadata last;
    ASSERT_TRUE(reader.partitionMetadata(999u, last).ok());
    EXPECT_EQ(last.firstTriangleId, 999u);
    EXPECT_EQ(last.triangleCount, 1u);
}

TEST(ManuMeshPartitionedDataset, EmptyDatasetRoundTripsAndCorruptDirectoryIsRejected) {
    const manumesh::filesystem::path emptyPath = tempPath("_empty.mmpd");
    const manumesh::filesystem::path corruptPath = tempPath("_corrupt.mmpd");
    const ScopedRemoval emptyCleanup(emptyPath);
    const ScopedRemoval corruptCleanup(corruptPath);
    const manumesh::PartitionedMeshConfig config = tinyStreamingConfig(1u);

    auto emptyWriterResult = manumesh::PartitionedMeshWriter::create(emptyPath.u8string(), config);
    ASSERT_TRUE(emptyWriterResult.ok()) << emptyWriterResult.status().message();
    manumesh::PartitionedMeshWriter emptyWriter = std::move(emptyWriterResult.value());
    ASSERT_TRUE(emptyWriter.finish().ok());
    auto emptyReaderResult = manumesh::PartitionedMeshReader::open(emptyPath.u8string(), config.memory);
    ASSERT_TRUE(emptyReaderResult.ok()) << emptyReaderResult.status().message();
    EXPECT_EQ(emptyReaderResult.value().triangleCount(), 0u);
    EXPECT_EQ(emptyReaderResult.value().partitionCount(), 0u);

    auto writerResult = manumesh::PartitionedMeshWriter::create(corruptPath.u8string(), config);
    ASSERT_TRUE(writerResult.ok()) << writerResult.status().message();
    manumesh::PartitionedMeshWriter writer = std::move(writerResult.value());
    ASSERT_TRUE(writer.appendTriangle(triangleRecord(0)).ok());
    ASSERT_TRUE(writer.finish().ok());
    {
        // Header (64), block header (96), payload (50), directory header (16), then partition ID.
        std::fstream file(corruptPath, std::ios::binary | std::ios::in | std::ios::out);
        ASSERT_TRUE(file);
        file.seekp(64 + 96 + 50 + 16, std::ios::beg);
        std::array<char, 8> invalidId{};
        invalidId[0] = 1;
        file.write(invalidId.data(), static_cast<std::streamsize>(invalidId.size()));
        ASSERT_TRUE(file);
    }
    auto corruptReaderResult = manumesh::PartitionedMeshReader::open(corruptPath.u8string(), config.memory);
    EXPECT_FALSE(corruptReaderResult.ok());
    EXPECT_EQ(corruptReaderResult.status().code(), manumesh::StatusCode::IoError);
}
