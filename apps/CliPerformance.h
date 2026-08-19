/**
 * @file apps/CliPerformance.h
 * @brief Declares opt-in command-level wall-clock performance reporting.
 * @ingroup manumesh_cli
 *
 * @details Performance output belongs to the CLI boundary: it measures file
 *          I/O and presentation-adjacent work without changing the library's
 *          algorithm result or report contracts.
 */

#pragma once

#include "core/ExecutionOptions.h"
#include "core/Filesystem.h"
#include "core/Mesh.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <string>

namespace manumesh {
namespace cli {

enum class PerformancePhase {
    Load,
    FeatureDetection,
    Simplification,
    Save,
    Postprocess,
    Operation,
};

/// One CSV row emitted by an opt-in CLI timing run. Negative durations mean a phase did not run.
struct PerformanceRecord {
    std::string command;
    std::string backend = "serial";
    int threadsRequested = 1;

    bool hasInputMesh = false;
    std::size_t inputVertices = 0;
    std::size_t inputFaces = 0;
    bool hasOutputMesh = false;
    std::size_t outputVertices = 0;
    std::size_t outputFaces = 0;
    bool hasSourceBytes = false;
    std::uintmax_t sourceBytes = 0;
    bool hasDatasetSummary = false;
    std::uint64_t triangleCount = 0;
    std::uint64_t partitionCount = 0;
    bool hasMemoryBudget = false;
    std::uint64_t memoryMiB = 0;
    std::uint64_t ioBufferMiB = 0;
    bool hasPartitionTriangles = false;
    std::uint64_t partitionTriangles = 0;

    double loadMilliseconds = -1.0;
    double featureDetectionMilliseconds = -1.0;
    double simplificationMilliseconds = -1.0;
    double saveMilliseconds = -1.0;
    double postprocessMilliseconds = -1.0;
    double operationMilliseconds = -1.0;
    double totalMilliseconds = -1.0;
};

/// Measures non-overlapping command phases only when --performance-csv is present.
class PerformanceTimer {
public:
    PerformanceTimer(std::string command, bool enabled);

    bool enabled() const noexcept { return enabled_; }
    void begin(PerformancePhase phase);
    void end(PerformancePhase phase);
    void finish();

    PerformanceRecord& record() noexcept { return record_; }
    const PerformanceRecord& record() const noexcept { return record_; }

private:
    using Clock = std::chrono::steady_clock;

    double* phaseOutput(PerformancePhase phase);

    bool enabled_ = false;
    bool finished_ = false;
    bool phaseRunning_ = false;
    PerformancePhase activePhase_ = PerformancePhase::Load;
    Clock::time_point startedAt_{};
    Clock::time_point phaseStartedAt_{};
    PerformanceRecord record_;
};

/// Records the resolved backend and requested concurrency for an algorithm command.
void setPerformanceExecution(PerformanceRecord& record, const ExecutionOptions& execution);
/// Records mesh dimensions and, when available, the input file size.
void setPerformanceInputMesh(PerformanceRecord& record, const Mesh& mesh, const filesystem::path& path);
/// Records output mesh dimensions.
void setPerformanceOutputMesh(PerformanceRecord& record, const Mesh& mesh);
/// Records a streamed dataset operation without claiming it materialized a mesh topology.
void setPerformanceDatasetSummary(
    PerformanceRecord& record, std::uint64_t triangles, std::uint64_t partitions, std::uintmax_t sourceBytes
);
/// Records the declared data-layer budget, which is distinct from process RSS.
void setPerformanceMemoryBudget(PerformanceRecord& record, std::uint64_t memoryMiB, std::uint64_t ioBufferMiB);

/// Transactionally writes exactly one schema-versioned CSV record.
void writePerformanceCsv(const filesystem::path& path, const PerformanceRecord& record);
/// Writes the same record as a concise diagnostic line without disturbing normal stdout results.
void writePerformanceSummary(std::ostream& output, const PerformanceRecord& record);

} // namespace cli
} // namespace manumesh
