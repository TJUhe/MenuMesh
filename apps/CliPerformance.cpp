/**
 * @file apps/CliPerformance.cpp
 * @brief Implements opt-in CLI wall-clock performance reporting.
 * @ingroup manumesh_cli
 */

#include "CliPerformance.h"

#include "CliCsv.h"

#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace manumesh {
namespace cli {
namespace {

double elapsedMilliseconds(std::chrono::steady_clock::time_point start, std::chrono::steady_clock::time_point stop) {
    return std::chrono::duration<double, std::milli>(stop - start).count();
}

std::string formatDuration(double value) {
    if (value < 0.0) {
        return "";
    }
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::setprecision(12) << value;
    return output.str();
}

std::string formatOptional(bool present, std::uintmax_t value) {
    return present ? std::to_string(value) : "";
}

} // namespace

PerformanceTimer::PerformanceTimer(std::string command, bool enabled)
    : enabled_(enabled) {
    record_.command = std::move(command);
    if (enabled_) {
        startedAt_ = Clock::now();
    }
}

double* PerformanceTimer::phaseOutput(PerformancePhase phase) {
    switch (phase) {
    case PerformancePhase::Load:
        return &record_.loadMilliseconds;
    case PerformancePhase::FeatureDetection:
        return &record_.featureDetectionMilliseconds;
    case PerformancePhase::Simplification:
        return &record_.simplificationMilliseconds;
    case PerformancePhase::Save:
        return &record_.saveMilliseconds;
    case PerformancePhase::Postprocess:
        return &record_.postprocessMilliseconds;
    case PerformancePhase::Operation:
        return &record_.operationMilliseconds;
    }
    return nullptr;
}

void PerformanceTimer::begin(PerformancePhase phase) {
    if (!enabled_) {
        return;
    }
    if (phaseRunning_) {
        throw std::logic_error("Cannot begin a performance phase before ending the previous phase.");
    }
    activePhase_ = phase;
    phaseStartedAt_ = Clock::now();
    phaseRunning_ = true;
}

void PerformanceTimer::end(PerformancePhase phase) {
    if (!enabled_) {
        return;
    }
    if (!phaseRunning_ || activePhase_ != phase) {
        throw std::logic_error("Performance phases must end in the order they began.");
    }
    double* const output = phaseOutput(phase);
    if (output != nullptr) {
        *output = elapsedMilliseconds(phaseStartedAt_, Clock::now());
    }
    phaseRunning_ = false;
}

void PerformanceTimer::finish() {
    if (!enabled_ || finished_) {
        return;
    }
    if (phaseRunning_) {
        end(activePhase_);
    }
    record_.totalMilliseconds = elapsedMilliseconds(startedAt_, Clock::now());
    finished_ = true;
}

void setPerformanceExecution(PerformanceRecord& record, const ExecutionOptions& execution) {
    record.backend = parallelExecutionBackendName();
    record.threadsRequested = execution.mode == ExecutionMode::Parallel ? execution.maxConcurrency : 1;
}

void setPerformanceInputMesh(PerformanceRecord& record, const Mesh& mesh, const filesystem::path& path) {
    record.hasInputMesh = true;
    record.inputVertices = mesh.vertices.size();
    record.inputFaces = mesh.faces.size();
    std::error_code error;
    const std::uintmax_t bytes = filesystem::file_size(path, error);
    if (!error) {
        record.hasSourceBytes = true;
        record.sourceBytes = bytes;
    }
}

void setPerformanceOutputMesh(PerformanceRecord& record, const Mesh& mesh) {
    record.hasOutputMesh = true;
    record.outputVertices = mesh.vertices.size();
    record.outputFaces = mesh.faces.size();
}

void setPerformanceDatasetSummary(
    PerformanceRecord& record, std::uint64_t triangles, std::uint64_t partitions, std::uintmax_t sourceBytes
) {
    record.hasDatasetSummary = true;
    record.triangleCount = triangles;
    record.partitionCount = partitions;
    record.hasSourceBytes = true;
    record.sourceBytes = sourceBytes;
}

void setPerformanceMemoryBudget(PerformanceRecord& record, std::uint64_t memoryMiB, std::uint64_t ioBufferMiB) {
    record.hasMemoryBudget = true;
    record.memoryMiB = memoryMiB;
    record.ioBufferMiB = ioBufferMiB;
}

void writePerformanceCsv(const filesystem::path& path, const PerformanceRecord& record) {
    AtomicCsvOutput csv(path);
    std::ofstream& output = csv.stream();
    output << "schema_version,command,backend,threads_requested,"
              "input_vertices,input_faces,output_vertices,output_faces,source_bytes,"
              "triangles,partitions,memory_mib,io_buffer_mib,partition_triangles,"
              "load_ms,feature_detect_ms,simplify_ms,save_ms,postprocess_ms,operation_ms,total_ms\n";
    output << "1," << quoteCsv(record.command) << "," << quoteCsv(record.backend) << ","
           << record.threadsRequested << ","
           << formatOptional(record.hasInputMesh, record.inputVertices) << ","
           << formatOptional(record.hasInputMesh, record.inputFaces) << ","
           << formatOptional(record.hasOutputMesh, record.outputVertices) << ","
           << formatOptional(record.hasOutputMesh, record.outputFaces) << ","
           << formatOptional(record.hasSourceBytes, record.sourceBytes) << ","
           << formatOptional(record.hasDatasetSummary, record.triangleCount) << ","
           << formatOptional(record.hasDatasetSummary, record.partitionCount) << ","
           << formatOptional(record.hasMemoryBudget, record.memoryMiB) << ","
           << formatOptional(record.hasMemoryBudget, record.ioBufferMiB) << ","
           << formatOptional(record.hasPartitionTriangles, record.partitionTriangles) << ","
           << formatDuration(record.loadMilliseconds) << ","
           << formatDuration(record.featureDetectionMilliseconds) << ","
           << formatDuration(record.simplificationMilliseconds) << ","
           << formatDuration(record.saveMilliseconds) << ","
           << formatDuration(record.postprocessMilliseconds) << ","
           << formatDuration(record.operationMilliseconds) << ","
           << formatDuration(record.totalMilliseconds) << "\n";
    csv.commit();
}

void writePerformanceSummary(std::ostream& output, const PerformanceRecord& record) {
    output << "performance command=" << record.command << " backend=" << record.backend
           << " threads_requested=" << record.threadsRequested;
    if (record.hasInputMesh) {
        output << " input_vertices=" << record.inputVertices << " input_faces=" << record.inputFaces;
    }
    if (record.hasOutputMesh) {
        output << " output_vertices=" << record.outputVertices << " output_faces=" << record.outputFaces;
    }
    if (record.hasDatasetSummary) {
        output << " triangles=" << record.triangleCount << " partitions=" << record.partitionCount;
    }
    if (record.hasSourceBytes) {
        output << " source_bytes=" << record.sourceBytes;
    }
    if (record.loadMilliseconds >= 0.0) {
        output << " load_ms=" << formatDuration(record.loadMilliseconds);
    }
    if (record.featureDetectionMilliseconds >= 0.0) {
        output << " feature_detect_ms=" << formatDuration(record.featureDetectionMilliseconds);
    }
    if (record.simplificationMilliseconds >= 0.0) {
        output << " simplify_ms=" << formatDuration(record.simplificationMilliseconds);
    }
    if (record.saveMilliseconds >= 0.0) {
        output << " save_ms=" << formatDuration(record.saveMilliseconds);
    }
    if (record.postprocessMilliseconds >= 0.0) {
        output << " postprocess_ms=" << formatDuration(record.postprocessMilliseconds);
    }
    if (record.operationMilliseconds >= 0.0) {
        output << " operation_ms=" << formatDuration(record.operationMilliseconds);
    }
    output << " total_ms=" << formatDuration(record.totalMilliseconds) << "\n";
}

} // namespace cli
} // namespace manumesh
