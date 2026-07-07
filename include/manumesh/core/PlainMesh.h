#pragma once

#include "manumesh/Export.h"

#include <array>
#include <vector>

namespace manumesh {

struct Mesh;

/// Eigen-free vertex type for SDK boundaries that should not expose Eigen.
struct PlainVec3 {
  double x = 0.0;
  double y = 0.0;
  double z = 0.0;
};

/// Eigen-free triangle face storing three zero-based vertex indices.
struct PlainFace {
  std::array<int, 3> v{};
};

/// Eigen-free triangle mesh exchange container.
///
/// Algorithms still use `Mesh` internally. This type gives host applications a
/// plain C++ data boundary when they do not want Eigen in their own public API.
struct PlainMesh {
  std::vector<PlainVec3> vertices;
  std::vector<PlainFace> faces;
};

/// Converts a plain SDK mesh into the internal Eigen-backed mesh type.
MANUMESH_API Mesh toMesh(const PlainMesh& plain);
/// Converts the internal Eigen-backed mesh type into a plain SDK mesh.
MANUMESH_API PlainMesh toPlainMesh(const Mesh& mesh);

} // namespace manumesh
