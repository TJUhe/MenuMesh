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
MANUMESH_API bool loadObj(const std::string& path, Mesh& mesh, std::string* error = nullptr);
/// Loads a mesh by file extension. Supported formats are STL and OBJ.
///
/// When the function returns false, the contents of `mesh` are unspecified.
MANUMESH_API bool
loadMesh(const std::string& path, Mesh& mesh, std::string* error = nullptr, double mergeRelativeEpsilon = 1e-9);
/// Writes the mesh as an ASCII STL file.
///
/// ASCII STL stores geometry only, so texture coordinates are not written.
/// Returns false and sets `error` when the mesh is invalid or when the file
/// cannot be created or fully written (for example on a full disk).
MANUMESH_API bool saveAsciiStl(
    const std::string& path, const Mesh& mesh, const std::string& solidName = "mesh", std::string* error = nullptr
);

} // namespace manumesh
