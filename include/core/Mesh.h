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
/// Double precision 2D vector used for texture coordinates.
using Vec2 = Eigen::Vector2d;
/// Homogeneous 4x4 matrix (e.g. quadric accumulation or affine transforms).
using Mat4 = Eigen::Matrix4d;

/// Triangle face storing three zero-based vertex indices.
struct Face {
    std::array<int, 3> v{};
};

/// Per-corner texture coordinates for one triangle.
///
/// Texture coordinates are corner-owned rather than vertex-owned so one
/// geometric vertex can retain different coordinates on adjacent UV charts.
struct FaceTexCoords {
    std::array<Vec2, 3> uv{};
    bool valid = false;
};

/// Minimal triangle mesh container used by the simplifier and utilities.
struct Mesh {
    std::vector<Vec3> vertices;
    std::vector<Face> faces;
    /// Empty when the mesh has no texture coordinates. Otherwise this vector
    /// is face-aligned; individual entries may be invalid for untextured faces.
    std::vector<FaceTexCoords> faceTexCoords;

    /// Returns true when either vertex or face storage is empty.
    MANUMESH_API bool empty() const;
    /// Axis-aligned bounding-box minimum corner.
    MANUMESH_API Vec3 bboxMin() const;
    /// Axis-aligned bounding-box maximum corner.
    MANUMESH_API Vec3 bboxMax() const;
    /// Length of the axis-aligned bounding-box diagonal.
    MANUMESH_API double bboxDiag() const;
    /// Returns true when at least one face has valid per-corner coordinates.
    MANUMESH_API bool hasTextureCoordinates() const;
    /// Compacts vertex storage and rewrites face indices after deletions.
    ///
    /// Faces referencing an out-of-range vertex index are silently dropped,
    /// and vertices referenced only by such dropped faces are removed as well:
    /// afterwards every remaining vertex is used by at least one valid face.
    MANUMESH_API void removeUnusedVertices();
};

/// Returns false and writes an error when any face index is out of range.
MANUMESH_API bool validateMeshIndices(const Mesh& mesh, std::string* error = nullptr);
/// Strict validation: returns false when indices are invalid, vertex
/// coordinates are not finite, a face repeats a vertex index, or a face has
/// (numerically) zero area. Used where degenerate geometry cannot be
/// represented downstream (e.g. STL export).
MANUMESH_API bool validateMeshGeometry(const Mesh& mesh, std::string* error = nullptr);
/// Lenient validation for analysis/simplification entry points: rejects the
/// inputs no algorithm can process (invalid indices, non-finite coordinates,
/// misaligned or non-finite texture coordinates, faces repeating a vertex
/// index) but tolerates zero-area faces. Callers should report tolerated
/// degeneracies via countDegenerateFaces.
MANUMESH_API bool validateMeshGeometryLenient(const Mesh& mesh, std::string* error = nullptr);
/// Counts faces that are degenerate: repeated vertex index or (numerically)
/// zero area. Assumes indices are valid (see validateMeshIndices).
MANUMESH_API int countDegenerateFaces(const Mesh& mesh);

/// Returns the area of one triangle.
MANUMESH_API double triangleArea(const Vec3& a, const Vec3& b, const Vec3& c);
/// Returns a unit triangle normal, or zero for degenerate triangles.
MANUMESH_API Vec3 triangleNormal(const Vec3& a, const Vec3& b, const Vec3& c);
/// Returns each undirected mesh edge once as sorted vertex index pairs.
MANUMESH_API std::vector<std::pair<int, int>> uniqueEdges(const Mesh& mesh);

} // namespace manumesh
