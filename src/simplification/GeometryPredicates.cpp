#include "detail/GeometryPredicates.h"

#include <Eigen/Core>
#include <algorithm>
#include <cmath>

namespace manumesh::simplification {
namespace {

bool aabbOverlap(const Vec3& aLo, const Vec3& aHi, const Vec3& bLo, const Vec3& bHi, double eps) {
    for (int axis = 0; axis < 3; ++axis) {
        if (aHi[axis] + eps < bLo[axis] || bHi[axis] + eps < aLo[axis]) {
            return false;
        }
    }
    return true;
}

bool segmentIntersectsTriangle(
    const Vec3& p0, const Vec3& p1, const Vec3& a, const Vec3& b, const Vec3& c, double eps
) {
    const Vec3 dir = p1 - p0;
    const Vec3 e1 = b - a;
    const Vec3 e2 = c - a;
    const Vec3 h = dir.cross(e2);
    const double det = e1.dot(h);
    if (std::abs(det) <= eps) {
        return false;
    }
    const double invDet = 1.0 / det;
    const Vec3 s = p0 - a;
    const double u = invDet * s.dot(h);
    if (u < -eps || u > 1.0 + eps) {
        return false;
    }
    const Vec3 q = s.cross(e1);
    const double v = invDet * dir.dot(q);
    if (v < -eps || u + v > 1.0 + eps) {
        return false;
    }
    const double t = invDet * e2.dot(q);
    return t >= eps && t <= 1.0 - eps;
}

Eigen::Vector2d projectToDominantPlane(const Vec3& p, int dropAxis) {
    switch (dropAxis) {
    case 0:
        return Eigen::Vector2d(p.y(), p.z());
    case 1:
        return Eigen::Vector2d(p.x(), p.z());
    default:
        return Eigen::Vector2d(p.x(), p.y());
    }
}

double orient2d(const Eigen::Vector2d& a, const Eigen::Vector2d& b, const Eigen::Vector2d& c) {
    return (b.x() - a.x()) * (c.y() - a.y()) - (b.y() - a.y()) * (c.x() - a.x());
}

bool intervalsOverlapWithLength(double a0, double a1, double b0, double b1, double eps) {
    if (a0 > a1)
        std::swap(a0, a1);
    if (b0 > b1)
        std::swap(b0, b1);
    return std::min(a1, b1) - std::max(a0, b0) > eps;
}

bool pointOnSegment2d(const Eigen::Vector2d& p, const Eigen::Vector2d& a, const Eigen::Vector2d& b, double eps) {
    if (std::abs(orient2d(a, b, p)) > eps) {
        return false;
    }
    return p.x() >= std::min(a.x(), b.x()) - eps && p.x() <= std::max(a.x(), b.x()) + eps &&
           p.y() >= std::min(a.y(), b.y()) - eps && p.y() <= std::max(a.y(), b.y()) + eps;
}

bool segmentsIntersect2d(
    const Eigen::Vector2d& a0,
    const Eigen::Vector2d& a1,
    const Eigen::Vector2d& b0,
    const Eigen::Vector2d& b1,
    double eps
) {
    const double o1 = orient2d(a0, a1, b0);
    const double o2 = orient2d(a0, a1, b1);
    const double o3 = orient2d(b0, b1, a0);
    const double o4 = orient2d(b0, b1, a1);
    const auto sign = [eps](double value) {
        if (value > eps)
            return 1;
        if (value < -eps)
            return -1;
        return 0;
    };
    const int s1 = sign(o1);
    const int s2 = sign(o2);
    const int s3 = sign(o3);
    const int s4 = sign(o4);

    if (s1 * s2 < 0 && s3 * s4 < 0) {
        return true;
    }

    const bool collinear = std::abs(o1) <= eps && std::abs(o2) <= eps && std::abs(o3) <= eps && std::abs(o4) <= eps;
    if (collinear) {
        const bool useX = std::abs(a1.x() - a0.x()) >= std::abs(a1.y() - a0.y());
        return useX ? intervalsOverlapWithLength(a0.x(), a1.x(), b0.x(), b1.x(), eps)
                    : intervalsOverlapWithLength(a0.y(), a1.y(), b0.y(), b1.y(), eps);
    }

    const bool endpointTouch =
        (s1 == 0 && pointOnSegment2d(b0, a0, a1, eps)) || (s2 == 0 && pointOnSegment2d(b1, a0, a1, eps)) ||
        (s3 == 0 && pointOnSegment2d(a0, b0, b1, eps)) || (s4 == 0 && pointOnSegment2d(a1, b0, b1, eps));
    return endpointTouch && (s1 * s2 < 0 || s3 * s4 < 0);
}

bool pointStrictlyInsideTriangle2d(const Eigen::Vector2d& p, const std::array<Eigen::Vector2d, 3>& tri, double eps) {
    const double o0 = orient2d(tri[0], tri[1], p);
    const double o1 = orient2d(tri[1], tri[2], p);
    const double o2 = orient2d(tri[2], tri[0], p);
    return (o0 > eps && o1 > eps && o2 > eps) || (o0 < -eps && o1 < -eps && o2 < -eps);
}

bool coplanarTrianglesOverlap(
    const std::array<Vec3, 3>& lhs, const std::array<Vec3, 3>& rhs, const Vec3& normal, double eps
) {
    int dropAxis = 0;
    if (std::abs(normal.y()) > std::abs(normal[dropAxis])) {
        dropAxis = 1;
    }
    if (std::abs(normal.z()) > std::abs(normal[dropAxis])) {
        dropAxis = 2;
    }

    std::array<Eigen::Vector2d, 3> a{};
    std::array<Eigen::Vector2d, 3> b{};
    for (int i = 0; i < 3; ++i) {
        a[i] = projectToDominantPlane(lhs[i], dropAxis);
        b[i] = projectToDominantPlane(rhs[i], dropAxis);
    }

    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            if (segmentsIntersect2d(a[i], a[(i + 1) % 3], b[j], b[(j + 1) % 3], eps)) {
                return true;
            }
        }
    }
    for (int i = 0; i < 3; ++i) {
        if (pointStrictlyInsideTriangle2d(a[i], b, eps) || pointStrictlyInsideTriangle2d(b[i], a, eps)) {
            return true;
        }
    }
    return false;
}

} // namespace

double triangleQualityLocal(const Vec3& a, const Vec3& b, const Vec3& c) {
    const double l0 = (b - a).squaredNorm();
    const double l1 = (c - b).squaredNorm();
    const double l2 = (a - c).squaredNorm();
    const double denom = l0 + l1 + l2;
    if (denom <= 1e-30) {
        return 0.0;
    }
    return 4.0 * std::sqrt(3.0) * triangleArea(a, b, c) / denom;
}

double pointTriangleDistanceSquaredLocal(const Vec3& p, const Vec3& a, const Vec3& b, const Vec3& c) {
    const Vec3 ab = b - a;
    const Vec3 ac = c - a;
    const Vec3 ap = p - a;
    const double d1 = ab.dot(ap);
    const double d2 = ac.dot(ap);
    if (d1 <= 0.0 && d2 <= 0.0)
        return (p - a).squaredNorm();

    const Vec3 bp = p - b;
    const double d3 = ab.dot(bp);
    const double d4 = ac.dot(bp);
    if (d3 >= 0.0 && d4 <= d3)
        return (p - b).squaredNorm();

    const double vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0 && d1 >= 0.0 && d3 <= 0.0) {
        const double v = d1 / (d1 - d3);
        return (p - (a + v * ab)).squaredNorm();
    }

    const Vec3 cp = p - c;
    const double d5 = ab.dot(cp);
    const double d6 = ac.dot(cp);
    if (d6 >= 0.0 && d5 <= d6)
        return (p - c).squaredNorm();

    const double vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0 && d2 >= 0.0 && d6 <= 0.0) {
        const double w = d2 / (d2 - d6);
        return (p - (a + w * ac)).squaredNorm();
    }

    const double va = d3 * d6 - d5 * d4;
    if (va <= 0.0 && (d4 - d3) >= 0.0 && (d5 - d6) >= 0.0) {
        const double w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return (p - (b + w * (c - b))).squaredNorm();
    }

    const Vec3 n = ab.cross(ac);
    const double nn = n.squaredNorm();
    if (nn <= 1e-30) {
        return std::min({(p - a).squaredNorm(), (p - b).squaredNorm(), (p - c).squaredNorm()});
    }
    const double distance = n.dot(ap);
    return distance * distance / nn;
}

std::pair<Vec3, Vec3> triangleAabb(const std::array<Vec3, 3>& tri, double padding) {
    Vec3 lo = tri[0].cwiseMin(tri[1]).cwiseMin(tri[2]);
    Vec3 hi = tri[0].cwiseMax(tri[1]).cwiseMax(tri[2]);
    const Vec3 pad(padding, padding, padding);
    return {lo - pad, hi + pad};
}

bool trianglesIntersect(const std::array<Vec3, 3>& lhs, const std::array<Vec3, 3>& rhs, double eps) {
    Vec3 lhsLo = lhs[0].cwiseMin(lhs[1]).cwiseMin(lhs[2]);
    Vec3 lhsHi = lhs[0].cwiseMax(lhs[1]).cwiseMax(lhs[2]);
    Vec3 rhsLo = rhs[0].cwiseMin(rhs[1]).cwiseMin(rhs[2]);
    Vec3 rhsHi = rhs[0].cwiseMax(rhs[1]).cwiseMax(rhs[2]);
    if (!aabbOverlap(lhsLo, lhsHi, rhsLo, rhsHi, eps)) {
        return false;
    }

    const Vec3 lhsNormal = (lhs[1] - lhs[0]).cross(lhs[2] - lhs[0]);
    const Vec3 rhsNormal = (rhs[1] - rhs[0]).cross(rhs[2] - rhs[0]);
    const double lhsNorm = lhsNormal.norm();
    const double rhsNorm = rhsNormal.norm();
    if (lhsNorm <= eps || rhsNorm <= eps) {
        return false;
    }
    const Vec3 lhsUnit = lhsNormal / lhsNorm;
    const Vec3 rhsUnit = rhsNormal / rhsNorm;
    const double parallelError = lhsUnit.cross(rhsUnit).norm();
    double maxPlaneDistance = 0.0;
    for (const Vec3& p : rhs) {
        maxPlaneDistance = std::max(maxPlaneDistance, std::abs((p - lhs[0]).dot(lhsUnit)));
    }
    if (parallelError <= 1e-8 && maxPlaneDistance <= eps) {
        return coplanarTrianglesOverlap(lhs, rhs, lhsUnit, eps);
    }

    for (int i = 0; i < 3; ++i) {
        if (segmentIntersectsTriangle(lhs[i], lhs[(i + 1) % 3], rhs[0], rhs[1], rhs[2], eps)) {
            return true;
        }
        if (segmentIntersectsTriangle(rhs[i], rhs[(i + 1) % 3], lhs[0], lhs[1], lhs[2], eps)) {
            return true;
        }
    }
    return false;
}

} // namespace manumesh::simplification
