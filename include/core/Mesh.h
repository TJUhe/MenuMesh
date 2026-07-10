#pragma once

#include "Export.h"

#include <Eigen/Dense>
#include <array>
#include <string>
#include <utility>
#include <vector>

namespace manumesh {

/// Double precision 3D vector used throughout the public geometry API.
using Vec3 = Eigen::Vector3d;
/// Homogeneous 4x4 quadric matrix.
using Mat4 = Eigen::Matrix4d;

/// Triangle face storing three zero-based vertex indices.
struct Face {
    std::array<int, 3> v{};
};

/// Minimal triangle mesh container used by the simplifier and utilities.
struct Mesh {
    std::vector<Vec3> vertices;
    std::vector<Face> faces;

    /// Returns true when either vertex or face storage is empty.
    MANUMESH_API bool empty() const;
    /// Axis-aligned bounding-box minimum corner.
    MANUMESH_API Vec3 bboxMin() const;
    /// Axis-aligned bounding-box maximum corner.
    MANUMESH_API Vec3 bboxMax() const;
    /// Length of the axis-aligned bounding-box diagonal.
    MANUMESH_API double bboxDiag() const;
    /// Compacts vertex storage and rewrites face indices after deletions.
    MANUMESH_API void removeUnusedVertices();
};

/// Returns false and writes an error when any face index is out of range.
MANUMESH_API bool validateMeshIndices(const Mesh& mesh, std::string* error = nullptr);
/// Returns false when indices are invalid, vertex coordinates are not finite,
/// or a face is degenerate.
MANUMESH_API bool validateMeshGeometry(const Mesh& mesh, std::string* error = nullptr);

/// Returns the area of one triangle.
MANUMESH_API double triangleArea(const Vec3& a, const Vec3& b, const Vec3& c);
/// Returns a unit triangle normal, or zero for degenerate triangles.
MANUMESH_API Vec3 triangleNormal(const Vec3& a, const Vec3& b, const Vec3& c);
/// Returns each undirected mesh edge once as sorted vertex index pairs.
MANUMESH_API std::vector<std::pair<int, int>> uniqueEdges(const Mesh& mesh);

} // namespace manumesh
