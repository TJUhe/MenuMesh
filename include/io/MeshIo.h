/**
 * @file include/io/MeshIo.h
 * @brief Declares mesh io facilities for ManuMesh's mesh-I/O module.
 * @ingroup manumesh_io
 *
 * @details Import and export paths validate indices and numeric values before materializing or serializing triangle geometry.
 */

#pragma once

#include "Export.h"
#include "core/Mesh.h"

#include <string>

namespace manumesh {

/// Loads an STL file, automatically handling ASCII and binary encodings.
///
/// Binary files whose size exceeds the exact `84 + 50 * n` record layout are
/// accepted and the trailing padding bytes are ignored. Any texture
/// coordinates already stored in `mesh` are cleared because STL carries no
/// UVs. When the function returns false, the contents of `mesh` are
/// unspecified.
/// @param[in] path Input file path.
/// @param[out] mesh Replaced with parsed geometry on success.
/// @param[out] error Optional diagnostic, cleared on success.
/// @param[in] mergeRelativeEpsilon Relative coincident-vertex merge tolerance; zero uses only the numeric floor.
/// @return true on complete parse and validation; false leaves `mesh` unspecified.
MANUMESH_API bool
loadStl(const std::string& path, Mesh& mesh, std::string* error = nullptr, double mergeRelativeEpsilon = 1e-9);
/// Loads an OBJ polygon mesh, triangulating faces and preserving per-corner `vt` coordinates.
///
/// Strictly convex polygons retain deterministic fan triangulation. Concave
/// polygons use projected ear clipping; repeated, degenerate, or
/// self-intersecting polygon faces are rejected instead of emitting invalid
/// triangles.
/// Vertex normals (`vn`) and unknown directives are ignored. When the
/// function returns false, the contents of `mesh` are unspecified.
/// @param[in] path Input file path.
/// @param[out] mesh Replaced with triangulated OBJ geometry on success.
/// @param[out] error Optional diagnostic, cleared on success.
/// @return true on complete parse and triangulation.
MANUMESH_API bool loadObj(const std::string& path, Mesh& mesh, std::string* error = nullptr);
/// Loads a mesh by file extension. Supported formats are STL and OBJ.
///
/// When the function returns false, the contents of `mesh` are unspecified.
/// @param[in] path Input path whose case-insensitive extension selects the loader.
/// @param[out] mesh Replaced on success.
/// @param[out] error Optional diagnostic.
/// @param[in] mergeRelativeEpsilon STL coincident-vertex merge tolerance; ignored for OBJ.
/// @return true for a supported, valid file.
MANUMESH_API bool
loadMesh(const std::string& path, Mesh& mesh, std::string* error = nullptr, double mergeRelativeEpsilon = 1e-9);
/// Writes the mesh as an ASCII STL file.
///
/// ASCII STL stores geometry only, so texture coordinates are not written.
/// Returns false and sets `error` when the mesh is invalid or when the file
/// cannot be created or fully written (for example on a full disk).
/// @param[in] path Destination path.
/// @param[in] mesh Strictly valid triangle mesh.
/// @param[in] solidName ASCII STL solid label.
/// @param[out] error Optional validation or I/O diagnostic.
/// @return true only after the complete file is written successfully.
MANUMESH_API bool saveAsciiStl(
    const std::string& path, const Mesh& mesh, const std::string& solidName = "mesh", std::string* error = nullptr
);

} // namespace manumesh
