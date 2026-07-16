/**
 * @file include/core/PlainMesh.h
 * @brief Declares plain mesh facilities for ManuMesh's core-mesh module.
 * @ingroup manumesh_core
 *
 * @details Core types establish the storage, validation, tolerance, topology, and status contracts consumed by every algorithm module.
 */

#pragma once

#include "Export.h"

#include <array>
#include <vector>

namespace manumesh {

struct Mesh;

/// Eigen-free vertex type for SDK boundaries that should not expose Eigen.
struct PlainVec3 {
    double x = 0.0; ///< X coordinate in model units.
    double y = 0.0; ///< Y coordinate in model units.
    double z = 0.0; ///< Z coordinate in model units.
};

/// Eigen-free texture coordinate used at SDK boundaries.
struct PlainVec2 {
    double u = 0.0; ///< U texture coordinate.
    double v = 0.0; ///< V texture coordinate.
};

/// Eigen-free triangle face storing three zero-based vertex indices.
struct PlainFace {
    std::array<int, 3> v{};
};

/// Eigen-free per-corner texture coordinates for one triangle.
struct PlainFaceTexCoords {
    std::array<PlainVec2, 3> uv{};
    bool valid = false;
};

/// Eigen-free triangle mesh exchange container.
///
/// Algorithms still use `Mesh` internally. This type gives host applications a
/// plain C++ data boundary when they do not want Eigen in their own public API.
struct PlainMesh {
    std::vector<PlainVec3> vertices;
    std::vector<PlainFace> faces;
    std::vector<PlainFaceTexCoords> faceTexCoords;
};

/// Converts a plain SDK mesh into the internal Eigen-backed mesh type.
/// @param[in] plain Source data; indices and numeric values are copied verbatim.
/// @return Independent Eigen-backed mesh copy.
MANUMESH_API Mesh toMesh(const PlainMesh& plain);
/// Converts the internal Eigen-backed mesh type into a plain SDK mesh.
/// @param[in] mesh Source mesh.
/// @return Independent Eigen-free mesh copy.
MANUMESH_API PlainMesh toPlainMesh(const Mesh& mesh);

} // namespace manumesh
