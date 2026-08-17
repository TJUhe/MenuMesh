/**
 * @file tests/unit/common/geometry_predicates_tests.cpp
 * @brief 验证三角形质量、距离、包围盒和相交谓词的数值契约。
 * @ingroup manumesh_tests
 */

#include "../../../src/common/detail/GeometryPredicates.h"

#include "core/Mesh.h"

#include <array>
#include <cmath>
#include <gtest/gtest.h>

TEST(ManuMesh, GeometryPredicatesMeasureTriangleQualityAndDistance) {
    const manumesh::Vec3 a(0.0, 0.0, 0.0);
    const manumesh::Vec3 b(1.0, 0.0, 0.0);
    const manumesh::Vec3 c(0.5, std::sqrt(3.0) / 2.0, 0.0);

    EXPECT_NEAR(1.0, manumesh::common::triangleQuality(a, b, c), 1e-12);
    EXPECT_EQ(0.0, manumesh::common::triangleQuality(a, a, b));

    const double aboveDistance =
        manumesh::common::pointTriangleDistanceSquared(manumesh::Vec3(0.5, 0.25, 2.0), a, b, c);
    EXPECT_NEAR(4.0, aboveDistance, 1e-12);

    const double outsideDistance =
        manumesh::common::pointTriangleDistanceSquared(manumesh::Vec3(-1.0, 0.0, 0.0), a, b, c);
    EXPECT_NEAR(1.0, outsideDistance, 1e-12);
}

TEST(ManuMesh, GeometryPredicatesDistanceAndQualityRemainStableAtExtremeScales) {
    for (const double scale : {1e-100, 1e+150}) {
        SCOPED_TRACE(testing::Message() << "scale=" << scale);
        const manumesh::Vec3 a(0.0, 0.0, 0.0);
        const manumesh::Vec3 b(scale, 0.0, 0.0);
        const manumesh::Vec3 c(0.0, scale, 0.0);
        const manumesh::Vec3 p(0.25 * scale, 0.25 * scale, 0.2 * scale);
        EXPECT_NEAR(std::sqrt(3.0) / 2.0, manumesh::common::triangleQuality(a, b, c), 1e-12);
        const double distanceSquared = manumesh::common::pointTriangleDistanceSquared(p, a, b, c);
        ASSERT_TRUE(std::isfinite(distanceSquared));
        EXPECT_NEAR(1.0, distanceSquared / (0.04 * scale * scale), 1e-12);
    }
}

TEST(ManuMesh, GeometryPredicatesDistanceUsesSegmentsForDegenerateTriangles) {
    EXPECT_NEAR(
        9.0,
        manumesh::common::pointTriangleDistanceSquared(
            manumesh::Vec3(1.0, 3.0, 0.0),
            manumesh::Vec3(0.0, 0.0, 0.0),
            manumesh::Vec3(2.0, 0.0, 0.0),
            manumesh::Vec3(1.0, 0.0, 0.0)
        ),
        1e-12
    );
}

TEST(ManuMesh, GeometryPredicatesMeasureAabbDistanceAndTriangleBounds) {
    const manumesh::Vec3 lo(0.0, 0.0, 0.0);
    const manumesh::Vec3 hi(1.0, 2.0, 3.0);

    EXPECT_EQ(0.0, manumesh::common::pointAabbDistanceSquared(manumesh::Vec3(0.5, 1.0, 2.0), lo, hi));
    EXPECT_NEAR(5.0, manumesh::common::pointAabbDistanceSquared(manumesh::Vec3(2.0, -2.0, 0.0), lo, hi), 1e-12);

    const std::array<manumesh::Vec3, 3> tri = {
        manumesh::Vec3(0.0, 1.0, 2.0),
        manumesh::Vec3(2.0, -1.0, 3.0),
        manumesh::Vec3(1.0, 0.0, -2.0),
    };
    const std::pair<manumesh::Vec3, manumesh::Vec3> bounds = manumesh::common::triangleAabb(tri, 0.25);
    const manumesh::Vec3& triLo = bounds.first;
    const manumesh::Vec3& triHi = bounds.second;
    EXPECT_TRUE(triLo.isApprox(manumesh::Vec3(-0.25, -1.25, -2.25)));
    EXPECT_TRUE(triHi.isApprox(manumesh::Vec3(2.25, 1.25, 3.25)));
}

TEST(ManuMesh, GeometryPredicatesDetectTriangleIntersection) {
    const std::array<manumesh::Vec3, 3> base = {
        manumesh::Vec3(0.0, 0.0, 0.0),
        manumesh::Vec3(1.0, 0.0, 0.0),
        manumesh::Vec3(0.0, 1.0, 0.0),
    };
    const std::array<manumesh::Vec3, 3> coplanarInside = {
        manumesh::Vec3(0.25, 0.25, 0.0),
        manumesh::Vec3(0.5, 0.25, 0.0),
        manumesh::Vec3(0.25, 0.5, 0.0),
    };
    const std::array<manumesh::Vec3, 3> separated = {
        manumesh::Vec3(2.0, 2.0, 0.0),
        manumesh::Vec3(3.0, 2.0, 0.0),
        manumesh::Vec3(2.0, 3.0, 0.0),
    };
    const std::array<manumesh::Vec3, 3> piercing = {
        manumesh::Vec3(0.25, 0.25, -1.0),
        manumesh::Vec3(0.25, 0.25, 1.0),
        manumesh::Vec3(0.75, 0.25, 0.0),
    };

    EXPECT_TRUE(manumesh::common::trianglesIntersect(base, coplanarInside, 1e-12));
    EXPECT_FALSE(manumesh::common::trianglesIntersect(base, separated, 1e-12));
    EXPECT_TRUE(manumesh::common::trianglesIntersect(base, piercing, 1e-12));
}
namespace {

std::array<manumesh::Vec3, 3> scaleTriangle(const std::array<manumesh::Vec3, 3>& tri, double scale) {
    return {scale * tri[0], scale * tri[1], scale * tri[2]};
}

} // 匿名命名空间

// 相交谓词接收相对容差，并按局部几何尺度归一化，因此同一组相交/不相交配置
// 在 1e-3 和 1e+3 两种尺度下都必须得到相同判定。
TEST(ManuMesh, GeometryPredicatesTriangleIntersectionIsScaleInvariant) {
    const std::array<manumesh::Vec3, 3> base = {
        manumesh::Vec3(0.0, 0.0, 0.0),
        manumesh::Vec3(1.0, 0.0, 0.0),
        manumesh::Vec3(0.0, 1.0, 0.0),
    };
    const std::array<manumesh::Vec3, 3> coplanarInside = {
        manumesh::Vec3(0.25, 0.25, 0.0),
        manumesh::Vec3(0.5, 0.25, 0.0),
        manumesh::Vec3(0.25, 0.5, 0.0),
    };
    const std::array<manumesh::Vec3, 3> separated = {
        manumesh::Vec3(2.0, 2.0, 0.0),
        manumesh::Vec3(3.0, 2.0, 0.0),
        manumesh::Vec3(2.0, 3.0, 0.0),
    };
    const std::array<manumesh::Vec3, 3> piercing = {
        manumesh::Vec3(0.25, 0.25, -1.0),
        manumesh::Vec3(0.25, 0.25, 1.0),
        manumesh::Vec3(0.75, 0.25, 0.0),
    };
    // 与其平行且近乎接触、但严格分离的三角形：间隙为局部尺度的 1e-6，
    // 在所有缩放下都应保持分离。
    const std::array<manumesh::Vec3, 3> hovering = {
        manumesh::Vec3(0.1, 0.1, 1e-6),
        manumesh::Vec3(0.9, 0.1, 1e-6),
        manumesh::Vec3(0.1, 0.9, 1e-6),
    };

    constexpr double kRelativeEps = 1e-9;
    for (const double scale : {1e-3, 1.0, 1e+3}) {
        SCOPED_TRACE(testing::Message() << "scale=" << scale);
        const auto scaledBase = scaleTriangle(base, scale);
        EXPECT_TRUE(
            manumesh::common::trianglesIntersect(scaledBase, scaleTriangle(coplanarInside, scale), kRelativeEps)
        );
        EXPECT_FALSE(manumesh::common::trianglesIntersect(scaledBase, scaleTriangle(separated, scale), kRelativeEps));
        EXPECT_TRUE(manumesh::common::trianglesIntersect(scaledBase, scaleTriangle(piercing, scale), kRelativeEps));
        EXPECT_FALSE(manumesh::common::trianglesIntersect(scaledBase, scaleTriangle(hovering, scale), kRelativeEps));
    }
}

TEST(ManuMesh, GeometryPredicatesIntersectionRemainsFiniteForLargeTranslatedTriangles) {
    const double offset = 1.0e150;
    const double extent = 1.0e149;
    const std::array<manumesh::Vec3, 3> base = {
        manumesh::Vec3(offset, offset, offset),
        manumesh::Vec3(offset + extent, offset, offset),
        manumesh::Vec3(offset, offset + extent, offset),
    };
    const std::array<manumesh::Vec3, 3> piercing = {
        manumesh::Vec3(offset + 0.25 * extent, offset + 0.25 * extent, offset - extent),
        manumesh::Vec3(offset + 0.25 * extent, offset + 0.25 * extent, offset + extent),
        manumesh::Vec3(offset + 0.75 * extent, offset + 0.25 * extent, offset),
    };
    const std::array<manumesh::Vec3, 3> separated = {
        manumesh::Vec3(offset + 3.0 * extent, offset, offset),
        manumesh::Vec3(offset + 4.0 * extent, offset, offset),
        manumesh::Vec3(offset + 3.0 * extent, offset + extent, offset),
    };

    EXPECT_TRUE(manumesh::common::trianglesIntersect(base, piercing, 1e-9));
    EXPECT_FALSE(manumesh::common::trianglesIntersect(base, separated, 1e-9));
}

// 共线重叠和点内判定在共面路径中同时涉及长度量纲与面积量纲的比较，
// 两者都必须随尺度一致变化。
TEST(ManuMesh, GeometryPredicatesCoplanarOverlapIsScaleInvariant) {
    const std::array<manumesh::Vec3, 3> base = {
        manumesh::Vec3(0.0, 0.0, 0.0),
        manumesh::Vec3(2.0, 0.0, 0.0),
        manumesh::Vec3(0.0, 2.0, 0.0),
    };
    // 与基准三角形共面并跨越其一条边发生重叠。
    const std::array<manumesh::Vec3, 3> crossing = {
        manumesh::Vec3(1.0, -0.5, 0.0),
        manumesh::Vec3(1.0, 0.5, 0.0),
        manumesh::Vec3(2.5, 0.5, 0.0),
    };
    // 完全位于基准三角形外部的共面三角形，不应发生重叠。
    const std::array<manumesh::Vec3, 3> disjointCoplanar = {
        manumesh::Vec3(3.0, 0.0, 0.0),
        manumesh::Vec3(4.0, 0.0, 0.0),
        manumesh::Vec3(3.0, 1.0, 0.0),
    };

    constexpr double kRelativeEps = 1e-9;
    for (const double scale : {1e-3, 1.0, 1e+3}) {
        SCOPED_TRACE(testing::Message() << "scale=" << scale);
        const auto scaledBase = scaleTriangle(base, scale);
        EXPECT_TRUE(manumesh::common::trianglesIntersect(scaledBase, scaleTriangle(crossing, scale), kRelativeEps));
        EXPECT_FALSE(
            manumesh::common::trianglesIntersect(scaledBase, scaleTriangle(disjointCoplanar, scale), kRelativeEps)
        );
    }
}

TEST(ManuMesh, GeometryPredicatesAllowOnlyDeclaredSharedTopologyContact) {
    const std::array<int, 3> baseIds{{0, 1, 2}};
    const std::array<manumesh::Vec3, 3> base = {
        manumesh::Vec3(0.0, 0.0, 0.0),
        manumesh::Vec3(1.0, 0.0, 0.0),
        manumesh::Vec3(0.0, 1.0, 0.0),
    };
    constexpr double kRelativeEps = 1e-9;

    const std::array<int, 3> vertexTouchIds{{0, 3, 4}};
    const std::array<manumesh::Vec3, 3> vertexTouch = {
        base[0],
        manumesh::Vec3(-1.0, 0.0, 0.0),
        manumesh::Vec3(0.0, -1.0, 0.0),
    };
    EXPECT_FALSE(
        manumesh::common::trianglesIntersectBeyondSharedTopology(
            baseIds, base, vertexTouchIds, vertexTouch, kRelativeEps
        )
    );

    const std::array<manumesh::Vec3, 3> vertexOverlap = {
        base[0],
        manumesh::Vec3(0.8, 0.1, 0.0),
        manumesh::Vec3(0.1, 0.8, 0.0),
    };
    EXPECT_TRUE(
        manumesh::common::trianglesIntersectBeyondSharedTopology(
            baseIds, base, vertexTouchIds, vertexOverlap, kRelativeEps
        )
    );

    const std::array<int, 3> edgeNeighborIds{{0, 1, 3}};
    const std::array<manumesh::Vec3, 3> edgeNeighbor = {
        base[0],
        base[1],
        manumesh::Vec3(0.0, -1.0, 0.0),
    };
    EXPECT_FALSE(
        manumesh::common::trianglesIntersectBeyondSharedTopology(
            baseIds, base, edgeNeighborIds, edgeNeighbor, kRelativeEps
        )
    );

    const std::array<manumesh::Vec3, 3> edgeOverlap = {
        base[0],
        base[1],
        manumesh::Vec3(0.25, 0.5, 0.0),
    };
    EXPECT_TRUE(
        manumesh::common::trianglesIntersectBeyondSharedTopology(
            baseIds, base, edgeNeighborIds, edgeOverlap, kRelativeEps
        )
    );

    const std::array<manumesh::Vec3, 3> foldedNeighbor = {
        base[0],
        base[1],
        manumesh::Vec3(0.25, 0.0, 0.75),
    };
    EXPECT_FALSE(
        manumesh::common::trianglesIntersectBeyondSharedTopology(
            baseIds, base, edgeNeighborIds, foldedNeighbor, kRelativeEps
        )
    );
}

TEST(ManuMesh, GeometryPredicatesTreatInvalidTopologyIdsAsGeometryOnly) {
    const std::array<int, 3> validIds{{0, 1, 2}};
    const std::array<int, 3> repeatedIds{{0, 0, 1}};
    const std::array<manumesh::Vec3, 3> base = {
        manumesh::Vec3(0.0, 0.0, 0.0),
        manumesh::Vec3(1.0, 0.0, 0.0),
        manumesh::Vec3(0.0, 1.0, 0.0),
    };
    const std::array<manumesh::Vec3, 3> overlapping = {
        manumesh::Vec3(0.1, 0.1, 0.0),
        manumesh::Vec3(0.8, 0.1, 0.0),
        manumesh::Vec3(0.1, 0.8, 0.0),
    };
    const std::array<manumesh::Vec3, 3> disjoint = {
        manumesh::Vec3(2.0, 2.0, 0.0),
        manumesh::Vec3(3.0, 2.0, 0.0),
        manumesh::Vec3(2.0, 3.0, 0.0),
    };

    EXPECT_TRUE(
        manumesh::common::trianglesIntersectBeyondSharedTopology(validIds, base, repeatedIds, overlapping, 1e-9)
    );
    EXPECT_FALSE(manumesh::common::trianglesIntersectBeyondSharedTopology(validIds, base, repeatedIds, disjoint, 1e-9));
}
