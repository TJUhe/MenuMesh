/**
 * @file src/common/GeometryPredicates.cpp
 * @brief 实现尺度归一化的距离、包围盒和三角形相交谓词。
 * @ingroup manumesh_common
 *
 * @details 实现尺度归一化距离、AABB 和三角形精确窄相位谓词。
 * @algorithm 三角形相交将共面与非共面情况分开，按局部因子范数归一化容差，
 * 明确允许限定在声明共享拓扑内的接触，同时拒绝超出该拓扑的重叠。
 * @invariants 对两个三角形进行一致缩放不会改变布尔结果。
 * @failuremodes 对退化和数值上有歧义的情况采取保守处理。
 */

#include "common/detail/GeometryPredicates.h"

#include "core/Tolerances.h"

#include <Eigen/Core>
#include <algorithm>
#include <cmath>

namespace manumesh {
namespace common {
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
    // eps 无量纲。Moller-Trumbore 行列式 dir.(e1 x e2) 按自身因子范数归一化，
    // u、v、t 是直接与 eps 比较的重心/参数（无量纲）量，
    // 因此一致缩放不会改变判定。
    const Vec3 dir = p1 - p0;
    const Vec3 e1 = b - a;
    const Vec3 e2 = c - a;
    const Vec3 h = dir.cross(e2);
    const double det = e1.dot(h);
    const double detScale = e1.norm() * h.norm();
    if (detScale <= 0.0 || std::abs(det) <= eps * detScale) {
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

// 二维辅助函数使用由同一个相对 eps 推导出的两个具有明确量纲的容差：
// epsLen（eps * scale）用于坐标/区间比较，epsArea（eps * scale^2）用于
// orient2d 有向面积比较。在两个量纲中混用单一绝对 eps 会导致谓词判定
// 随网格一致缩放而变化。
bool intervalsOverlapWithLength(double a0, double a1, double b0, double b1, double epsLen) {
    if (a0 > a1)
        std::swap(a0, a1);
    if (b0 > b1)
        std::swap(b0, b1);
    return std::min(a1, b1) - std::max(a0, b0) > epsLen;
}

bool pointOnSegment2d(
    const Eigen::Vector2d& p, const Eigen::Vector2d& a, const Eigen::Vector2d& b, double epsLen, double epsArea
) {
    if (std::abs(orient2d(a, b, p)) > epsArea) {
        return false;
    }
    return p.x() >= std::min(a.x(), b.x()) - epsLen && p.x() <= std::max(a.x(), b.x()) + epsLen &&
           p.y() >= std::min(a.y(), b.y()) - epsLen && p.y() <= std::max(a.y(), b.y()) + epsLen;
}

bool segmentsIntersect2d(
    const Eigen::Vector2d& a0,
    const Eigen::Vector2d& a1,
    const Eigen::Vector2d& b0,
    const Eigen::Vector2d& b1,
    double epsLen,
    double epsArea
) {
    const double o1 = orient2d(a0, a1, b0);
    const double o2 = orient2d(a0, a1, b1);
    const double o3 = orient2d(b0, b1, a0);
    const double o4 = orient2d(b0, b1, a1);
    const auto sign = [epsArea](double value) {
        if (value > epsArea)
            return 1;
        if (value < -epsArea)
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

    const bool collinear =
        std::abs(o1) <= epsArea && std::abs(o2) <= epsArea && std::abs(o3) <= epsArea && std::abs(o4) <= epsArea;
    if (collinear) {
        const bool useX = std::abs(a1.x() - a0.x()) >= std::abs(a1.y() - a0.y());
        return useX ? intervalsOverlapWithLength(a0.x(), a1.x(), b0.x(), b1.x(), epsLen)
                    : intervalsOverlapWithLength(a0.y(), a1.y(), b0.y(), b1.y(), epsLen);
    }

    const bool endpointTouch = (s1 == 0 && pointOnSegment2d(b0, a0, a1, epsLen, epsArea)) ||
                               (s2 == 0 && pointOnSegment2d(b1, a0, a1, epsLen, epsArea)) ||
                               (s3 == 0 && pointOnSegment2d(a0, b0, b1, epsLen, epsArea)) ||
                               (s4 == 0 && pointOnSegment2d(a1, b0, b1, epsLen, epsArea));
    return endpointTouch && (s1 * s2 < 0 || s3 * s4 < 0);
}

bool pointStrictlyInsideTriangle2d(
    const Eigen::Vector2d& p, const std::array<Eigen::Vector2d, 3>& tri, double epsArea
) {
    const double o0 = orient2d(tri[0], tri[1], p);
    const double o1 = orient2d(tri[1], tri[2], p);
    const double o2 = orient2d(tri[2], tri[0], p);
    return (o0 > epsArea && o1 > epsArea && o2 > epsArea) || (o0 < -epsArea && o1 < -epsArea && o2 < -epsArea);
}

bool coplanarTrianglesOverlap(
    const std::array<Vec3, 3>& lhs, const std::array<Vec3, 3>& rhs, const Vec3& normal, double epsLen, double epsArea
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
            if (segmentsIntersect2d(a[i], a[(i + 1) % 3], b[j], b[(j + 1) % 3], epsLen, epsArea)) {
                return true;
            }
        }
    }
    for (int i = 0; i < 3; ++i) {
        if (pointStrictlyInsideTriangle2d(a[i], b, epsArea) || pointStrictlyInsideTriangle2d(b[i], a, epsArea)) {
            return true;
        }
    }
    return false;
}

} // 命名空间

double triangleQuality(const Vec3& a, const Vec3& b, const Vec3& c) {
    const double l0 = (b - a).squaredNorm();
    const double l1 = (c - b).squaredNorm();
    const double l2 = (a - c).squaredNorm();
    const double denom = l0 + l1 + l2;
    if (denom <= kMinSquaredEdgeLengthSum) {
        return 0.0;
    }
    return 4.0 * std::sqrt(3.0) * triangleArea(a, b, c) / denom;
}

double pointTriangleDistanceSquared(const Vec3& p, const Vec3& a, const Vec3& b, const Vec3& c) {
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
    // 相对退化检查：|ab x ac|^2 <= tol * (|ab| |ac|)^2 表示夹角在数值上为零，
    // 与绝对尺度无关。
    if (nn <= 1e-24 * ab.squaredNorm() * ac.squaredNorm()) {
        return std::min({(p - a).squaredNorm(), (p - b).squaredNorm(), (p - c).squaredNorm()});
    }
    const double distance = n.dot(ap);
    return distance * distance / nn;
}

double pointAabbDistanceSquared(const Vec3& p, const Vec3& lo, const Vec3& hi) {
    double d2 = 0.0;
    for (int axis = 0; axis < 3; ++axis) {
        const double value = p[axis];
        if (value < lo[axis]) {
            const double d = lo[axis] - value;
            d2 += d * d;
        } else if (value > hi[axis]) {
            const double d = value - hi[axis];
            d2 += d * d;
        }
    }
    return d2;
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
    // eps 是相对量；根据局部二元尺度推导绝对容差，使长度使用 eps*scale，
    // 面积使用 eps*scale^2。这使谓词判定在网格一致缩放时保持不变。
    const double scale = std::max((lhsHi - lhsLo).maxCoeff(), (rhsHi - rhsLo).maxCoeff());
    const double epsLen = eps * scale;
    const double epsArea = eps * scale * scale;
    if (!aabbOverlap(lhsLo, lhsHi, rhsLo, rhsHi, epsLen)) {
        return false;
    }

    const Vec3 lhsNormal = (lhs[1] - lhs[0]).cross(lhs[2] - lhs[0]);
    const Vec3 rhsNormal = (rhs[1] - rhs[0]).cross(rhs[2] - rhs[0]);
    const double lhsNorm = lhsNormal.norm();
    const double rhsNorm = rhsNormal.norm();
    // 法向量模是三角形面积的两倍，因此应与面积量纲的容差比较。
    if (lhsNorm <= epsArea || rhsNorm <= epsArea) {
        return false;
    }
    const Vec3 lhsUnit = lhsNormal / lhsNorm;
    const Vec3 rhsUnit = rhsNormal / rhsNorm;
    const double parallelError = lhsUnit.cross(rhsUnit).norm();
    double maxPlaneDistance = 0.0;
    for (const Vec3& p : rhs) {
        maxPlaneDistance = std::max(maxPlaneDistance, std::abs((p - lhs[0]).dot(lhsUnit)));
    }
    if (parallelError <= 1e-8 && maxPlaneDistance <= epsLen) {
        return coplanarTrianglesOverlap(lhs, rhs, lhsUnit, epsLen, epsArea);
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

bool trianglesIntersectBeyondSharedTopology(
    const std::array<int, 3>& lhsIds,
    const std::array<Vec3, 3>& lhs,
    const std::array<int, 3>& rhsIds,
    const std::array<Vec3, 3>& rhs,
    double eps
) {
    std::array<int, 2> lhsShared{{-1, -1}};
    int sharedCount = 0;
    int lhsOpposite = -1;
    int rhsOpposite = -1;
    for (int i = 0; i < 3; ++i) {
        bool shared = false;
        for (int j = 0; j < 3; ++j) {
            if (lhsIds[i] == rhsIds[j]) {
                if (sharedCount < 2) {
                    lhsShared[sharedCount] = i;
                }
                ++sharedCount;
                shared = true;
                break;
            }
        }
        if (!shared) {
            lhsOpposite = i;
        }
    }
    for (int j = 0; j < 3; ++j) {
        bool shared = false;
        for (int i = 0; i < 3; ++i) {
            if (rhsIds[j] == lhsIds[i]) {
                shared = true;
                break;
            }
        }
        if (!shared) {
            rhsOpposite = j;
        }
    }

    if (sharedCount <= 1) {
        return trianglesIntersect(lhs, rhs, eps);
    }
    if (sharedCount >= 3) {
        return true;
    }

    const Vec3& s0 = lhs[static_cast<std::size_t>(lhsShared[0])];
    const Vec3& s1 = lhs[static_cast<std::size_t>(lhsShared[1])];
    const Vec3& lhsTip = lhs[static_cast<std::size_t>(lhsOpposite)];
    const Vec3& rhsTip = rhs[static_cast<std::size_t>(rhsOpposite)];
    const Vec3 lhsNormal = (lhs[1] - lhs[0]).cross(lhs[2] - lhs[0]);
    const Vec3 rhsNormal = (rhs[1] - rhs[0]).cross(rhs[2] - rhs[0]);
    const double lhsNorm = lhsNormal.norm();
    const double rhsNorm = rhsNormal.norm();
    const double scale = std::max(
        (lhs[0].cwiseMax(lhs[1]).cwiseMax(lhs[2]) - lhs[0].cwiseMin(lhs[1]).cwiseMin(lhs[2])).maxCoeff(),
        (rhs[0].cwiseMax(rhs[1]).cwiseMax(rhs[2]) - rhs[0].cwiseMin(rhs[1]).cwiseMin(rhs[2])).maxCoeff()
    );
    const double epsLen = eps * scale;
    const double epsArea = eps * scale * scale;
    if (lhsNorm <= epsArea || rhsNorm <= epsArea) {
        return false;
    }

    const Vec3 lhsUnit = lhsNormal / lhsNorm;
    const Vec3 rhsUnit = rhsNormal / rhsNorm;
    const bool coplanar = lhsUnit.cross(rhsUnit).norm() <= 1e-8 && std::abs((rhsTip - s0).dot(lhsUnit)) <= epsLen;
    if (!coplanar) {
        // 共享整条边的两个非共面三角形只能在线上相交，而该直线包含声明的边。
        return false;
    }

    const Vec3 edge = s1 - s0;
    const double lhsSide = lhsUnit.dot(edge.cross(lhsTip - s0));
    const double rhsSide = lhsUnit.dot(edge.cross(rhsTip - s0));
    return (lhsSide > epsArea && rhsSide > epsArea) || (lhsSide < -epsArea && rhsSide < -epsArea);
}

} // namespace common
} // namespace manumesh
