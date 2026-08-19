/**
 * @file apps/ManuMeshLargeMeshCommands.cpp
 * @brief Implements bounded-memory import and validation commands.
 * @ingroup manumesh_cli
 */

#include "ManuMeshLargeMeshCommands.h"

#include "CliPath.h"
#include "CliPerformance.h"
#include "io/PartitionedMeshDataset.h"

#include <array>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace fs = manumesh::filesystem;

namespace manumesh {
namespace cli {
namespace large_mesh_commands {
namespace {

constexpr std::uint64_t kBytesPerMiB = 1024ull * 1024ull;
constexpr std::uint64_t kDefaultMemoryMiB = 256ull;
constexpr std::uint64_t kDefaultIoBufferMiB = 4ull;
constexpr std::uint64_t kDefaultPartitionTriangles = 1000000ull;

struct ParsedMemoryOptions {
    LargeMeshMemoryBudget budget{};
    std::uint64_t memoryMiB = 0;
    std::uint64_t ioBufferMiB = 0;
};

std::uint64_t parseUnsignedDecimal(const std::string& value, const std::string& name) {
    if (value.empty()) {
        throw std::invalid_argument(name + " must be a positive integer.");
    }
    std::uint64_t result = 0;
    for (char ch : value) {
        if (ch < '0' || ch > '9') {
            throw std::invalid_argument(name + " must be a positive integer.");
        }
        const std::uint64_t digit = static_cast<std::uint64_t>(ch - '0');
        if (result > (std::numeric_limits<std::uint64_t>::max() - digit) / 10ull) {
            throw std::invalid_argument(name + " is outside the supported 64-bit range.");
        }
        result = result * 10ull + digit;
    }
    if (result == 0) {
        throw std::invalid_argument(name + " must be greater than zero.");
    }
    return result;
}

std::uint64_t unsignedOption(const Args& args, const std::string& name, std::uint64_t defaultValue) {
    const std::string value = getArg(args, name);
    return value.empty() ? defaultValue : parseUnsignedDecimal(value, name);
}

std::uint64_t mibToBytes(std::uint64_t value, const std::string& name) {
    if (value > std::numeric_limits<std::uint64_t>::max() / kBytesPerMiB) {
        throw std::invalid_argument(name + " overflows the supported byte range.");
    }
    return value * kBytesPerMiB;
}

ParsedMemoryOptions parseMemoryOptions(const Args& args) {
    ParsedMemoryOptions result;
    result.memoryMiB = unsignedOption(args, "--memory-mib", kDefaultMemoryMiB);
    result.ioBufferMiB = unsignedOption(args, "--io-buffer-mib", kDefaultIoBufferMiB);
    if (result.ioBufferMiB > result.memoryMiB) {
        throw std::invalid_argument("--io-buffer-mib must be less than or equal to --memory-mib.");
    }
    result.budget.maxResidentBytes = mibToBytes(result.memoryMiB, "--memory-mib");
    result.budget.ioBufferBytes = mibToBytes(result.ioBufferMiB, "--io-buffer-mib");
    return result;
}

void requireStatus(const Status& status) {
    if (!status.ok()) {
        throw std::runtime_error(status.message().empty() ? "Large-mesh operation failed." : status.message());
    }
}

void requireExactPositionals(const Args& args, std::size_t expected, const char* command, const char* usage) {
    const std::vector<std::string> positional = positionalArgs(args);
    if (positional.size() != expected) {
        throw std::invalid_argument(std::string(command) + " requires exactly " + usage + ".");
    }
}

void requirePerformanceOutputDoesNotAlias(
    const fs::path& output, const fs::path& protectedPath, const char* protectedLabel
) {
    if (pathsReferToSameLocation(output, protectedPath)) {
        throw std::invalid_argument(std::string("--performance-csv must not overwrite ") + protectedLabel + ".");
    }
}

std::uintmax_t sourceFileSize(const fs::path& path) {
    std::error_code error;
    const std::uintmax_t bytes = fs::file_size(path, error);
    return error ? 0u : bytes;
}

std::string formatBounds(const std::array<double, 3>& bounds) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(std::numeric_limits<double>::max_digits10) << bounds[0] << "," << bounds[1] << ","
        << bounds[2];
    return out.str();
}

} // namespace

int importDataset(const Args& args) {
    requireExactPositionals(args, 2u, "large-import", "input.stl output.mmpd");
    const std::vector<std::string> positional = positionalArgs(args);
    const fs::path inputPath = pathFromUtf8(positional[0]);
    const fs::path outputPath = pathFromUtf8(positional[1]);
    const std::string performanceCsv = getArg(args, "--performance-csv");
    const fs::path performancePath = performanceCsv.empty() ? fs::path() : pathFromUtf8(performanceCsv);
    if (pathsReferToSameLocation(inputPath, outputPath)) {
        throw std::invalid_argument("large-import input and output must not refer to the same file.");
    }
    if (!performanceCsv.empty()) {
        requirePerformanceOutputDoesNotAlias(performancePath, inputPath, "the input mesh");
        requirePerformanceOutputDoesNotAlias(performancePath, outputPath, "the partitioned dataset");
    }
    const ParsedMemoryOptions memory = parseMemoryOptions(args);
    const std::uint64_t partitionTriangles = unsignedOption(args, "--partition-triangles", kDefaultPartitionTriangles);
    if (partitionTriangles > std::numeric_limits<std::uint32_t>::max()) {
        throw std::invalid_argument("--partition-triangles exceeds the supported 32-bit local index range.");
    }

    PartitionedMeshConfig config;
    config.memory = memory.budget;
    config.trianglesPerPartition = static_cast<std::uint32_t>(partitionTriangles);
    requireStatus(validatePartitionedMeshConfig(config));

    PerformanceTimer timer("large-import", !performanceCsv.empty());
    setPerformanceMemoryBudget(timer.record(), memory.memoryMiB, memory.ioBufferMiB);
    timer.record().hasPartitionTriangles = true;
    timer.record().partitionTriangles = partitionTriangles;
    PartitionedMeshSummary summary;
    timer.begin(PerformancePhase::Operation);
    requireStatus(importBinaryStlToPartitionedMesh(positional[0], positional[1], config, &summary));
    timer.end(PerformancePhase::Operation);
    setPerformanceDatasetSummary(timer.record(), summary.triangleCount, summary.partitionCount, summary.sourceBytes);
    std::cout << "large_import triangles=" << summary.triangleCount << " partitions=" << summary.partitionCount
              << " source_bytes=" << summary.sourceBytes << " partition_triangles=" << partitionTriangles
              << " memory_mib=" << memory.memoryMiB << " io_buffer_mib=" << memory.ioBufferMiB << "\n";
    if (timer.enabled()) {
        timer.finish();
        writePerformanceCsv(performancePath, timer.record());
        writePerformanceSummary(std::cerr, timer.record());
    }
    return 0;
}

int validateDataset(const Args& args) {
    requireExactPositionals(args, 1u, "large-validate", "input.mmpd");
    const std::vector<std::string> positional = positionalArgs(args);
    const fs::path inputPath = pathFromUtf8(positional[0]);
    const std::string performanceCsv = getArg(args, "--performance-csv");
    const fs::path performancePath = performanceCsv.empty() ? fs::path() : pathFromUtf8(performanceCsv);
    if (!performanceCsv.empty()) {
        requirePerformanceOutputDoesNotAlias(performancePath, inputPath, "the partitioned dataset");
    }
    const ParsedMemoryOptions memory = parseMemoryOptions(args);
    PerformanceTimer timer("large-validate", !performanceCsv.empty());
    setPerformanceMemoryBudget(timer.record(), memory.memoryMiB, memory.ioBufferMiB);
    PartitionedMeshValidationReport report;
    timer.begin(PerformancePhase::Operation);
    requireStatus(validatePartitionedMeshDataset(positional[0], memory.budget, &report));
    timer.end(PerformancePhase::Operation);
    setPerformanceDatasetSummary(timer.record(), report.triangleCount, report.partitionCount, sourceFileSize(inputPath));

    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(std::numeric_limits<double>::max_digits10)
        << "large_validate triangles=" << report.triangleCount << " partitions=" << report.partitionCount
        << " area=" << report.surfaceArea << " degenerate=" << report.degenerateTriangleCount;
    if (report.hasBounds) {
        out << " bounds_min=" << formatBounds(report.boundsMin)
            << " bounds_max=" << formatBounds(report.boundsMax);
    } else {
        out << " bounds_min=empty bounds_max=empty";
    }
    out << " count_consistency=ok checksum_consistency=ok\n";
    std::cout << out.str();
    if (timer.enabled()) {
        timer.finish();
        writePerformanceCsv(performancePath, timer.record());
        writePerformanceSummary(std::cerr, timer.record());
    }
    return 0;
}

} // namespace large_mesh_commands
} // namespace cli
} // namespace manumesh
