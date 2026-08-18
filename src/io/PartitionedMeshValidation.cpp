/**
 * @file src/io/PartitionedMeshValidation.cpp
 * @brief Implements reusable bounded-memory validation for triangle staging datasets.
 * @ingroup manumesh_io
 */

#include "io/PartitionedMeshDataset.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>

namespace manumesh {
namespace {

Status validationError(std::string message) { return Status(StatusCode::IoError, std::move(message)); }
Status outOfMemory(std::string message) { return Status(StatusCode::OutOfMemory, std::move(message)); }

bool checkedAdd(std::uint64_t lhs, std::uint64_t rhs, std::uint64_t& result) {
    if (rhs > std::numeric_limits<std::uint64_t>::max() - lhs) {
        return false;
    }
    result = lhs + rhs;
    return true;
}

double measuredTriangleArea(const BinaryStlTriangleRecord& triangle) {
    std::array<double, 3> ab{};
    std::array<double, 3> ac{};
    double scale = 0.0;
    for (std::size_t axis = 0; axis < 3u; ++axis) {
        ab[axis] = static_cast<double>(triangle.vertices[1][axis]) -
                   static_cast<double>(triangle.vertices[0][axis]);
        ac[axis] = static_cast<double>(triangle.vertices[2][axis]) -
                   static_cast<double>(triangle.vertices[0][axis]);
        scale = std::max(scale, std::max(std::abs(ab[axis]), std::abs(ac[axis])));
    }
    if (!std::isfinite(scale)) {
        throw std::overflow_error("Triangle area input overflowed.");
    }
    if (scale == 0.0) {
        return 0.0;
    }

    for (std::size_t axis = 0; axis < 3u; ++axis) {
        ab[axis] /= scale;
        ac[axis] /= scale;
    }
    const double crossX = ab[1] * ac[2] - ab[2] * ac[1];
    const double crossY = ab[2] * ac[0] - ab[0] * ac[2];
    const double crossZ = ab[0] * ac[1] - ab[1] * ac[0];
    const double factor = 0.5 * std::hypot(std::hypot(crossX, crossY), crossZ);
    if (!std::isfinite(factor) ||
        (factor > 0.0 && scale > std::sqrt(std::numeric_limits<double>::max() / factor))) {
        throw std::overflow_error("Triangle area is outside the finite double range.");
    }
    const double area = (factor * scale) * scale;
    if (!std::isfinite(area)) {
        throw std::overflow_error("Triangle area is outside the finite double range.");
    }
    return area;
}

void includeVertexBounds(
    const std::array<float, 3>& vertex,
    std::array<double, 3>& boundsMin,
    std::array<double, 3>& boundsMax,
    bool& hasBounds
) {
    for (std::size_t axis = 0; axis < 3u; ++axis) {
        const double coordinate = static_cast<double>(vertex[axis]);
        if (!std::isfinite(coordinate)) {
            throw std::overflow_error("Triangle bounds contain a non-finite coordinate.");
        }
        if (!hasBounds) {
            boundsMin[axis] = coordinate;
            boundsMax[axis] = coordinate;
        } else {
            boundsMin[axis] = std::min(boundsMin[axis], coordinate);
            boundsMax[axis] = std::max(boundsMax[axis], coordinate);
        }
    }
    hasBounds = true;
}

bool boundsEqual(
    const std::array<double, 3>& boundsMin,
    const std::array<double, 3>& boundsMax,
    const MeshPartitionMetadata& metadata
) {
    for (std::size_t axis = 0; axis < 3u; ++axis) {
        if (boundsMin[axis] != metadata.boundsMin[axis] || boundsMax[axis] != metadata.boundsMax[axis]) {
            return false;
        }
    }
    return true;
}

void addArea(double area, double& total, double& compensation) {
    const double corrected = area - compensation;
    const double next = total + corrected;
    if (!std::isfinite(next)) {
        throw std::overflow_error("Accumulated surface area overflowed.");
    }
    compensation = (next - total) - corrected;
    total = next;
}

} // namespace

Status validatePartitionedMeshDataset(
    const std::string& path,
    const LargeMeshMemoryBudget& memory,
    PartitionedMeshValidationReport* report
) {
    if (report) {
        *report = PartitionedMeshValidationReport{};
    }
    try {
        Result<PartitionedMeshReader> readerResult = PartitionedMeshReader::open(path, memory);
        if (!readerResult.ok()) {
            return readerResult.status();
        }
        PartitionedMeshReader reader = std::move(readerResult.value());
        PartitionedMeshValidationReport measured;
        double areaCompensation = 0.0;

        for (std::uint64_t partitionIndex = 0; partitionIndex < reader.partitionCount(); ++partitionIndex) {
            MeshPartitionMetadata metadata;
            Status status = reader.partitionMetadata(partitionIndex, metadata);
            if (!status.ok()) {
                return status;
            }
            if (metadata.firstTriangleId != measured.triangleCount) {
                return validationError("Partition global triangle ranges are not contiguous.");
            }

            status = reader.beginPartition(partitionIndex);
            if (!status.ok()) {
                return status;
            }
            std::uint64_t partitionTriangles = 0;
            std::array<double, 3> partitionBoundsMin{{0.0, 0.0, 0.0}};
            std::array<double, 3> partitionBoundsMax{{0.0, 0.0, 0.0}};
            bool partitionHasBounds = false;
            while (true) {
                BinaryStlTriangleRecord triangle;
                bool hasTriangle = false;
                status = reader.readNextTriangle(triangle, hasTriangle);
                if (!status.ok()) {
                    return status;
                }
                if (!hasTriangle) {
                    break;
                }
                if (partitionTriangles == std::numeric_limits<std::uint64_t>::max()) {
                    return validationError("Partition triangle counter overflowed.");
                }
                ++partitionTriangles;

                const double area = measuredTriangleArea(triangle);
                addArea(area, measured.surfaceArea, areaCompensation);
                if (area == 0.0) {
                    if (measured.degenerateTriangleCount == std::numeric_limits<std::uint64_t>::max()) {
                        return validationError("Degenerate triangle counter overflowed.");
                    }
                    ++measured.degenerateTriangleCount;
                }
                for (const std::array<float, 3>& vertex : triangle.vertices) {
                    includeVertexBounds(
                        vertex, partitionBoundsMin, partitionBoundsMax, partitionHasBounds
                    );
                    includeVertexBounds(vertex, measured.boundsMin, measured.boundsMax, measured.hasBounds);
                }
            }

            if (partitionTriangles != metadata.triangleCount) {
                return validationError("Partition payload count does not match its directory entry.");
            }
            if (!partitionHasBounds || !boundsEqual(partitionBoundsMin, partitionBoundsMax, metadata)) {
                return validationError("Partition payload bounds do not match its directory entry.");
            }
            std::uint64_t nextTriangleCount = 0;
            if (!checkedAdd(measured.triangleCount, partitionTriangles, nextTriangleCount)) {
                return validationError("Dataset triangle counter overflowed.");
            }
            measured.triangleCount = nextTriangleCount;
            if (measured.partitionCount == std::numeric_limits<std::uint64_t>::max()) {
                return validationError("Dataset partition counter overflowed.");
            }
            ++measured.partitionCount;
        }

        if (measured.triangleCount != reader.triangleCount() ||
            measured.partitionCount != reader.partitionCount()) {
            return validationError("Dataset header, directory, and streamed record counts are inconsistent.");
        }
        if ((measured.triangleCount == 0) != !measured.hasBounds) {
            return validationError("Dataset bounds state is inconsistent with its triangle count.");
        }
        if (report) {
            *report = measured;
        }
        return Status::success();
    } catch (const std::bad_alloc&) {
        return outOfMemory("Failed to allocate the partitioned dataset validation buffer.");
    } catch (const std::exception& exception) {
        return validationError("Failed to validate partitioned dataset: " + std::string(exception.what()));
    }
}

} // namespace manumesh
