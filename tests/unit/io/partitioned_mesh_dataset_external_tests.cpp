#include "io/PartitionedMeshDataset.h"

#include "core/Filesystem.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <limits>
#include <sstream>
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

struct ScopedRemoval {
    explicit ScopedRemoval(manumesh::filesystem::path requestedPath)
        : path(std::move(requestedPath)) {}

    ~ScopedRemoval() {
        std::error_code ignored;
        manumesh::filesystem::remove(path, ignored);
    }

    manumesh::filesystem::path path;
};

struct LargeThingi10kFixture {
    manumesh::filesystem::path path;
    std::uint64_t expectedFaces = 0;
};

struct ManifestFixtureReadResult {
    std::vector<LargeThingi10kFixture> fixtures;
    bool available = false;
    std::string error;
};

bool parseUnsignedDecimal(const std::string& text, std::uint64_t& value) {
    if (text.empty()) {
        return false;
    }
    value = 0;
    for (const char digit : text) {
        if (digit < '0' || digit > '9') {
            return false;
        }
        const std::uint64_t next = static_cast<std::uint64_t>(digit - '0');
        if (value > (std::numeric_limits<std::uint64_t>::max() - next) / 10u) {
            return false;
        }
        value = value * 10u + next;
    }
    return true;
}

bool isHexDigest(const std::string& text) {
    if (text.size() != 64u) {
        return false;
    }
    for (const char digit : text) {
        const bool decimal = digit >= '0' && digit <= '9';
        const bool lowerHex = digit >= 'a' && digit <= 'f';
        const bool upperHex = digit >= 'A' && digit <= 'F';
        if (!decimal && !lowerHex && !upperHex) {
            return false;
        }
    }
    return true;
}

bool isStableLargeFixtureFilename(const std::string& filename, std::uint64_t expectedFaces) {
    const std::string prefix = "thingi10k_";
    const std::string suffix = "_faces.stl";
    if (filename.compare(0, prefix.size(), prefix) != 0 ||
        filename.size() <= prefix.size() + suffix.size() ||
        filename.compare(filename.size() - suffix.size(), suffix.size(), suffix) != 0 ||
        filename.find('/') != std::string::npos || filename.find('\\') != std::string::npos) {
        return false;
    }

    const std::string encodedIdAndFaces =
        filename.substr(prefix.size(), filename.size() - prefix.size() - suffix.size());
    const std::string::size_type separator = encodedIdAndFaces.find('_');
    if (separator == std::string::npos || separator == 0 || separator + 1 >= encodedIdAndFaces.size() ||
        encodedIdAndFaces.find('_', separator + 1) != std::string::npos) {
        return false;
    }

    std::uint64_t encodedFaces = 0;
    return parseUnsignedDecimal(encodedIdAndFaces.substr(separator + 1), encodedFaces) &&
           encodedFaces == expectedFaces;
}

ManifestFixtureReadResult readLargeThingi10kManifestIndex() {
    ManifestFixtureReadResult result;
#if defined(MANUMESH_THINGI10K_MANIFEST_INDEX)
    const char* indexPath = MANUMESH_THINGI10K_MANIFEST_INDEX;
    if (!indexPath || *indexPath == '\0') {
        return result;
    }
    std::ifstream index(manumesh::filesystem::u8path(indexPath), std::ios::binary);
    if (!index) {
        // The manifest is optional. The CTest setup creates this index only
        // after validating a downloaded fixture set.
        return result;
    }
    result.available = true;

    std::string line;
    std::size_t lineNumber = 0;
    while (std::getline(index, line)) {
        ++lineNumber;
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty() || line.front() == '#') {
            continue;
        }

        std::istringstream fields(line);
        std::string filename;
        std::string faceText;
        std::string hash;
        std::string trailing;
        if (!std::getline(fields, filename, '\t') || !std::getline(fields, faceText, '\t') ||
            !std::getline(fields, hash, '\t') || std::getline(fields, trailing) || filename.empty()) {
            result.error = "Malformed Thingi10K fixture index line " + std::to_string(lineNumber) + ".";
            return result;
        }

        std::uint64_t expectedFaces = 0;
        if (!parseUnsignedDecimal(faceText, expectedFaces) || expectedFaces < 2000000u ||
            !isStableLargeFixtureFilename(filename, expectedFaces) || !isHexDigest(hash)) {
            result.error = "Invalid Thingi10K fixture index record on line " + std::to_string(lineNumber) + ".";
            return result;
        }
        for (const LargeThingi10kFixture& existing : result.fixtures) {
            if (existing.path.filename().u8string() == filename) {
                result.error = "Duplicate Thingi10K fixture in generated index: " + filename;
                return result;
            }
        }

        const manumesh::filesystem::path repositoryRoot =
            manumesh::filesystem::path(MANUMESH_TEST_DATA_DIR).parent_path().parent_path();
        result.fixtures.push_back({repositoryRoot / "output" / "thingi10k_large" / filename, expectedFaces});
    }
    if (!index.eof()) {
        result.error = "Failed while reading the generated Thingi10K fixture index.";
    }
#endif
    return result;
}

manumesh::filesystem::path temporaryDatasetPath(const manumesh::filesystem::path& input) {
    static std::atomic<unsigned long long> sequence{0};
    const auto tick =
        static_cast<unsigned long long>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    return manumesh::filesystem::temp_directory_path() /
           (input.stem().u8string() + "_streamed_" + std::to_string(currentProcessId()) + "_" +
            std::to_string(tick) + "_" +
            std::to_string(sequence.fetch_add(1, std::memory_order_relaxed)) + ".mmpd");
}

} // namespace

TEST(ManuMeshPartitionedDatasetExternal, StreamsDownloadedMultiMillionTriangleThingi10kFiles) {
    const ManifestFixtureReadResult manifest = readLargeThingi10kManifestIndex();
    ASSERT_TRUE(manifest.error.empty()) << manifest.error;
    if (!manifest.available || manifest.fixtures.empty()) {
        GTEST_SKIP() << "Run tests/support/fetch_thingi10k_large.py to install optional large fixtures.";
    }

    manumesh::PartitionedMeshConfig config;
    config.memory.maxResidentBytes = 1024u * 1024u;
    config.memory.ioBufferBytes = 1024u * 1024u;
    config.trianglesPerPartition = 250000u;

    for (const LargeThingi10kFixture& fixture : manifest.fixtures) {
        const manumesh::filesystem::path& input = fixture.path;
        const manumesh::filesystem::path output = temporaryDatasetPath(input);
        const ScopedRemoval cleanup(output);
        std::error_code ignored;
        manumesh::filesystem::remove(output, ignored);

        manumesh::PartitionedMeshSummary summary;
        const manumesh::Status importStatus =
            manumesh::importBinaryStlToPartitionedMesh(input.u8string(), output.u8string(), config, &summary);
        ASSERT_TRUE(importStatus.ok()) << input.u8string() << ": " << importStatus.message();
        EXPECT_EQ(summary.triangleCount, fixture.expectedFaces) << input.u8string();
        EXPECT_EQ(
            summary.partitionCount,
            (summary.triangleCount + config.trianglesPerPartition - 1u) / config.trianglesPerPartition
        );

        auto readerResult = manumesh::PartitionedMeshReader::open(output.u8string(), config.memory);
        ASSERT_TRUE(readerResult.ok()) << input.u8string() << ": " << readerResult.status().message();
        manumesh::PartitionedMeshReader reader = std::move(readerResult.value());
        EXPECT_EQ(reader.triangleCount(), summary.triangleCount);
        EXPECT_EQ(reader.partitionCount(), summary.partitionCount);

        std::uint64_t streamedTriangles = 0;
        for (std::uint64_t partition = 0; partition < reader.partitionCount(); ++partition) {
            manumesh::MeshPartitionMetadata metadata;
            ASSERT_TRUE(reader.partitionMetadata(partition, metadata).ok());
            EXPECT_EQ(metadata.firstTriangleId, streamedTriangles);
            ASSERT_TRUE(reader.beginPartition(partition).ok());
            manumesh::BinaryStlTriangleRecord triangle;
            bool hasTriangle = false;
            std::uint64_t localTriangles = 0;
            while (true) {
                const manumesh::Status readStatus = reader.readNextTriangle(triangle, hasTriangle);
                ASSERT_TRUE(readStatus.ok()) << input.u8string() << ": " << readStatus.message();
                if (!hasTriangle) {
                    break;
                }
                ++localTriangles;
            }
            EXPECT_EQ(localTriangles, metadata.triangleCount);
            streamedTriangles += localTriangles;
        }
        EXPECT_EQ(streamedTriangles, summary.triangleCount);
    }
}
