#pragma once

#include "Export.h"
#include "core/Mesh.h"

#include <string>

namespace manumesh {

/// Loads an STL file, automatically handling ASCII and binary encodings.
MANUMESH_API bool loadStl(const std::string& path, Mesh& mesh,
                          std::string* error = nullptr,
                          double mergeRelativeEpsilon = 1e-9);
/// Loads a simple OBJ triangle mesh.
MANUMESH_API bool loadObj(const std::string& path, Mesh& mesh,
                          std::string* error = nullptr);
/// Loads a mesh by file extension. Supported formats are STL and OBJ.
MANUMESH_API bool loadMesh(const std::string& path, Mesh& mesh,
                           std::string* error = nullptr,
                           double mergeRelativeEpsilon = 1e-9);
/// Writes the mesh as an ASCII STL file.
MANUMESH_API bool saveAsciiStl(const std::string& path, const Mesh& mesh,
                               const std::string& solidName = "mesh",
                               std::string* error = nullptr);

} // namespace manumesh
