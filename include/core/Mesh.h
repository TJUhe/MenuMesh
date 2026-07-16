/**
 * @file include/core/Mesh.h
 * @brief Declares mesh facilities for ManuMesh's core-mesh module.
 * @ingroup manumesh_core
 *
 * @details Core types establish the storage, validation, tolerance, topology, and status contracts consumed by every algorithm module.
 */

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
    std::array<int, 3> v{}; ///< Counter-clockwise zero-based vertex indices.
};

/// Per-corner texture coordinates for one triangle.
///
/// Texture coordinates are corner-owned rather than vertex-owned so one
/// geometric vertex can retain different coordinates on adjacent UV charts.
struct FaceTexCoords {
    std::array<Vec2, 3> uv{}; ///< UV value corresponding to each face corner.
    bool valid = false;       ///< Whether this face owns usable UV coordinates.
};

/// Minimal triangle mesh container used by the simplifier and utilities.
struct Mesh {
    std::vector<Vec3> vertices; ///< Vertex positions in model units.
    std::vector<Face> faces;    ///< Triangle connectivity into `vertices`.
    /// Empty when the mesh has no texture coordinates. Otherwise this vector
    /// is face-aligned; individual entries may be invalid for untextured faces.
    std::vector<FaceTexCoords> faceTexCoords;

    /// @return true when either vertex or face storage is empty.
    MANUMESH_API bool empty() const;
    /// @return Axis-aligned bounding-box minimum, or zero for no vertices.
    MANUMESH_API Vec3 bboxMin() const;
    /// @return Axis-aligned bounding-box maximum, or zero for no vertices.
    MANUMESH_API Vec3 bboxMax() const;
    /// @return Length of the axis-aligned bounding-box diagonal in model units.
    MANUMESH_API double bboxDiag() const;
    /// @return true when at least one face has valid per-corner coordinates.
    MANUMESH_API bool hasTextureCoordinates() const;
    /// Compacts vertex storage and rewrites face indices after deletions.
    ///
    /// Faces referencing an out-of-range vertex index are silently dropped,
    /// and vertices referenced only by such dropped faces are removed as well:
    /// afterwards every remaining vertex is used by at least one valid face.
    MANUMESH_API void removeUnusedVertices();
};

/// Checks that every face index addresses `mesh.vertices`.
/// @param[in] mesh Mesh to inspect.
/// @param[out] error Optional diagnostic, cleared on success.
/// @return true when all indices are valid.
MANUMESH_API bool validateMeshIndices(const Mesh& mesh, std::string* error = nullptr);
/// Strict validation: returns false when indices are invalid, vertex
/// coordinates are not finite, a face repeats a vertex index, or a face has
/// (numerically) zero area. Used where degenerate geometry cannot be
/// represented downstream (e.g. STL export).
/// @param[in] mesh Mesh to validate.
/// @param[out] error Optional diagnostic, cleared on success.
/// @return true only when indices, coordinates, topology-per-face, UVs, and
/// triangle areas satisfy the strict storage contract.
MANUMESH_API bool validateMeshGeometry(const Mesh& mesh, std::string* error = nullptr);
/// Lenient validation for analysis/simplification entry points: rejects the
/// inputs no algorithm can process (invalid indices, non-finite coordinates,
/// misaligned or non-finite texture coordinates, faces repeating a vertex
/// index) but tolerates zero-area faces. Callers should report tolerated
/// degeneracies via countDegenerateFaces.
/// @param[in] mesh Mesh to validate.
/// @param[out] error Optional diagnostic, cleared on success.
/// @return true when the mesh is algorithm-safe apart from tolerated zero-area faces.
MANUMESH_API bool validateMeshGeometryLenient(const Mesh& mesh, std::string* error = nullptr);
/// @param[in] mesh Mesh whose indices have already been validated.
/// @return Number of faces with repeated indices or numerically zero area.
///
/// Counts faces that are degenerate: repeated vertex index or (numerically)
/// zero area. Assumes indices are valid (see validateMeshIndices).
MANUMESH_API int countDegenerateFaces(const Mesh& mesh);

/// @param[in] a First triangle position.
/// @param[in] b Second triangle position.
/// @param[in] c Third triangle position.
/// @return Unsigned triangle area in squared model units.
MANUMESH_API double triangleArea(const Vec3& a, const Vec3& b, const Vec3& c);
/// @param[in] a First triangle position.
/// @param[in] b Second triangle position.
/// @param[in] c Third triangle position.
/// @return Unit normal following `(b-a) x (c-a)`, or zero when degenerate.
MANUMESH_API Vec3 triangleNormal(const Vec3& a, const Vec3& b, const Vec3& c);
/// @param[in] mesh Mesh with valid face indices.
/// @return Each undirected edge once as an ascending vertex-index pair.
/// @complexity Expected O(F), with F the number of faces.
MANUMESH_API std::vector<std::pair<int, int>> uniqueEdges(const Mesh& mesh);

} // namespace manumesh
