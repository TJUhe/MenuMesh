/**
 * @file include/io/PartitionedMeshDataset.h
 * @brief Defines a bounded-memory, partitioned triangle dataset and binary STL importer.
 * @ingroup manumesh_io
 *
 * @details This API is independent from the in-memory Mesh container. Version 1 is a bounded-memory
 * triangle staging format: records are written and read incrementally, use 64-bit global triangle
 * IDs, and are grouped into partitions whose local triangle range fits in 32 bits. It deliberately
 * does not model shared vertices, edges, owner/ghost state, or algorithm halos; a future topological
 * out-of-core dataset must use a separate versioned entity schema rather than reinterpret these
 * triangle-record partitions.
 */

#pragma once

#include "Export.h"
#include "core/Status.h"

#include <array>
#include <cstdint>
#include <memory>
#include <string>

namespace manumesh {

/// Stable identifier of a triangle across all partitions in one dataset.
using GlobalTriangleId = std::uint64_t;
/// Stable identifier of a partition in one dataset.
using MeshPartitionId = std::uint64_t;
/// Compact index of a triangle inside its owning partition.
using LocalTriangleIndex = std::uint32_t;

/// Memory limits used by partitioned mesh streaming operations.
struct LargeMeshMemoryBudget {
    /// Maximum variable-size working-set bytes owned by one reader or writer operation.
    std::uint64_t maxResidentBytes = 256ull * 1024ull * 1024ull;
    /// Sequential I/O buffer size. It is allocated once and reused.
    std::uint64_t ioBufferBytes = 4ull * 1024ull * 1024ull;
};

/// Controls how a streamed triangle sequence is divided into independently addressable partitions.
struct PartitionedMeshConfig {
    LargeMeshMemoryBudget memory{};
    /// Maximum triangle count in an automatically closed partition.
    std::uint32_t trianglesPerPartition = 1000000u;
};

/// One standard 50-byte binary STL triangle record in decoded form.
struct BinaryStlTriangleRecord {
    std::array<float, 3> normal{{0.0f, 0.0f, 0.0f}};
    std::array<std::array<float, 3>, 3> vertices{{
        {{0.0f, 0.0f, 0.0f}},
        {{0.0f, 0.0f, 0.0f}},
        {{0.0f, 0.0f, 0.0f}},
    }};
    std::uint16_t attributeByteCount = 0;
};

/// Persistent directory entry for one triangle partition.
struct MeshPartitionMetadata {
    MeshPartitionId id = 0;
    GlobalTriangleId firstTriangleId = 0;
    std::uint64_t triangleCount = 0;
    std::uint64_t payloadOffset = 0;
    std::uint64_t payloadBytes = 0;
    /// FNV-1a checksum of the encoded 50-byte triangle records in this partition.
    std::uint64_t payloadChecksum = 0;
    std::array<double, 3> boundsMin{{0.0, 0.0, 0.0}};
    std::array<double, 3> boundsMax{{0.0, 0.0, 0.0}};
};

/// Result counters produced by a streaming import or exposed by an opened dataset.
struct PartitionedMeshSummary {
    std::uint64_t triangleCount = 0;
    std::uint64_t partitionCount = 0;
    std::uint64_t sourceBytes = 0;
};

/// Aggregate measurements produced while validating every record in a triangle staging dataset.
struct PartitionedMeshValidationReport {
    std::uint64_t triangleCount = 0;
    std::uint64_t partitionCount = 0;
    std::uint64_t degenerateTriangleCount = 0;
    double surfaceArea = 0.0;
    std::array<double, 3> boundsMin{{0.0, 0.0, 0.0}};
    std::array<double, 3> boundsMax{{0.0, 0.0, 0.0}};
    bool hasBounds = false;
};

/// Validates that the configured buffer fits in the declared resident-memory budget.
MANUMESH_API Status validatePartitionedMeshConfig(const PartitionedMeshConfig& config);

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

/**
 * @brief Transactional streaming writer for the versioned ManuMesh partitioned triangle format.
 *
 * Triangle payload is flushed incrementally. Partition directory entries are staged in a disk
 * sidecar rather than retained in memory. Unless finish() succeeds, the requested destination is
 * not replaced and temporary files are removed by the destructor.
 */
class MANUMESH_API PartitionedMeshWriter {
public:
    static Result<PartitionedMeshWriter>
    create(const std::string& path, const PartitionedMeshConfig& config = PartitionedMeshConfig{});

    ~PartitionedMeshWriter();
    PartitionedMeshWriter(PartitionedMeshWriter&& other) noexcept;
    PartitionedMeshWriter& operator=(PartitionedMeshWriter&& other) noexcept;
    PartitionedMeshWriter(const PartitionedMeshWriter&) = delete;
    PartitionedMeshWriter& operator=(const PartitionedMeshWriter&) = delete;

    /// Appends one record and automatically closes the partition at the configured limit.
    Status appendTriangle(const BinaryStlTriangleRecord& triangle);
    /// Closes the current non-empty partition before the configured limit.
    Status endPartition();
    /// Writes the directory, validates counters, and atomically replaces the destination.
    Status finish();

    std::uint64_t triangleCount() const;
    std::uint64_t partitionCount() const;
    bool finished() const;

private:
    struct Impl;
    explicit PartitionedMeshWriter(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

/**
 * @brief Bounded-memory reader for partitioned triangle datasets.
 *
 * The on-disk partition directory is queried one entry at a time; opening a dataset therefore
 * does not allocate memory proportional to its partition or triangle count.
 */
class MANUMESH_API PartitionedMeshReader {
public:
    static Result<PartitionedMeshReader>
    open(const std::string& path, const LargeMeshMemoryBudget& memory = LargeMeshMemoryBudget{});

    ~PartitionedMeshReader();
    PartitionedMeshReader(PartitionedMeshReader&& other) noexcept;
    PartitionedMeshReader& operator=(PartitionedMeshReader&& other) noexcept;
    PartitionedMeshReader(const PartitionedMeshReader&) = delete;
    PartitionedMeshReader& operator=(const PartitionedMeshReader&) = delete;

    std::uint64_t triangleCount() const;
    std::uint64_t partitionCount() const;
    /// Reads one directory entry without retaining the complete directory.
    Status partitionMetadata(std::uint64_t partitionIndex, MeshPartitionMetadata& metadata) const;
    /// Selects a partition and positions its sequential record cursor at local index zero.
    /// The currently selected partition must be fully consumed before another partition is selected;
    /// this makes its payload checksum a completed read contract rather than an optional best effort.
    Status beginPartition(std::uint64_t partitionIndex);
    /// Reads the next record in the selected partition; hasTriangle is false at partition end.
    Status readNextTriangle(BinaryStlTriangleRecord& triangle, bool& hasTriangle);

private:
    struct Impl;
    explicit PartitionedMeshReader(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

/**
 * @brief Streams a binary STL into a partitioned dataset without constructing Mesh.
 *
 * Binary STL trailing padding is ignored. Truncated files and non-finite record values are
 * rejected. The standard uint32 triangle-count field is accepted in full; the legacy in-memory
 * STL size and temporary-allocation limits do not apply.
 */
MANUMESH_API Status importBinaryStlToPartitionedMesh(
    const std::string& stlPath,
    const std::string& datasetPath,
    const PartitionedMeshConfig& config = PartitionedMeshConfig{},
    PartitionedMeshSummary* summary = nullptr
);

/**
 * @brief Streams every partition and validates directory ranges, block metadata, finite records,
 * checksums, payload bounds, counts, area, and degenerate triangles without constructing Mesh.
 *
 * @details Success means the complete version-1 triangle staging dataset was consumed. This is a
 * storage-integrity and triangle-geometry contract, not a manifold/topology validation.
 */
MANUMESH_API Status validatePartitionedMeshDataset(
    const std::string& path,
    const LargeMeshMemoryBudget& memory = LargeMeshMemoryBudget{},
    PartitionedMeshValidationReport* report = nullptr
);

} // namespace manumesh
