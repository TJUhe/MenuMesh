#pragma once

#include "core/Mesh.h"

#include <array>
#include <utility>

namespace manumesh::common {

double triangleQuality(const Vec3& a, const Vec3& b, const Vec3& c);
double pointTriangleDistanceSquared(const Vec3& p, const Vec3& a, const Vec3& b, const Vec3& c);
double pointAabbDistanceSquared(const Vec3& p, const Vec3& lo, const Vec3& hi);
std::pair<Vec3, Vec3> triangleAabb(const std::array<Vec3, 3>& tri, double padding = 0.0);
/// Triangle-triangle intersection test with a RELATIVE tolerance.
///
/// eps is dimensionless: every internal comparison is normalized by the local
/// geometric scale of the two triangles (lengths by scale, areas/orientations
/// by scale^2, the Moller-Trumbore determinant by its own factor norms), so
/// the decision is invariant under uniform scaling of the input.
/// Typical eps: 1e-9 .. 1e-12.
bool trianglesIntersect(const std::array<Vec3, 3>& lhs, const std::array<Vec3, 3>& rhs, double eps);

} // namespace manumesh::common

namespace manumesh {
// Transitional alias: manumesh::detail was renamed to manumesh::common
// (architecture v2, R6). New code must use manumesh::common; this alias is
// removed after one minor version.
namespace detail = common;
} // namespace manumesh
