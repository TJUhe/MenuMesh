#pragma once

#include "core/Mesh.h"

#include <array>
#include <utility>

namespace manumesh::detail {

double triangleQuality(const Vec3& a, const Vec3& b, const Vec3& c);
double pointTriangleDistanceSquared(const Vec3& p, const Vec3& a, const Vec3& b, const Vec3& c);
double pointAabbDistanceSquared(const Vec3& p, const Vec3& lo, const Vec3& hi);
std::pair<Vec3, Vec3> triangleAabb(const std::array<Vec3, 3>& tri, double padding = 0.0);
bool trianglesIntersect(const std::array<Vec3, 3>& lhs, const std::array<Vec3, 3>& rhs, double eps);

} // namespace manumesh::detail
